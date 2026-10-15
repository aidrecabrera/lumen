// @vitest-environment jsdom
import { mqtt5 } from "aws-iot-device-sdk-v2/dist/browser";
import { generate, parser, type Packet } from "mqtt-packet";
import { once } from "node:events";
import type { AddressInfo } from "node:net";
import { setTimeout as sleep } from "node:timers/promises";
import { WebSocketServer, type WebSocket } from "ws";

import { AwsTransport } from "../src/transport/aws.ts";
import { describeTransportContract } from "./transport-contract.ts";

const DEVICE_ID = "lumen-aabbccddeeff";
const TOPIC_PREFIX = `lumen/${DEVICE_ID}/`;
const MQTT5 = { protocolVersion: 5 };
const QOS_AT_LEAST_ONCE = 1;
const REASON_SUCCESS = 0;

interface Received {
  suffix: string;
  payload: Uint8Array;
}

// mqtt requires a packet id on subscribe and on qos 1 publish; mqtt-packet types it as optional
function packetId(id: number | undefined): number {
  if (id === undefined) throw new Error("packet without an id");

  return id;
}

// stands in for iot core: aedes speaks only mqtt 3.1.1 and the aws client speaks mqtt 5.
describeTransportContract("aws", async () => {
  const server = new WebSocketServer({ port: 0 });
  const retained = new Map<string, Uint8Array>();
  const subscribers = new Map<WebSocket, Set<string>>();
  const waiting: ((message: Received) => void)[] = [];

  function send(socket: WebSocket, packet: Packet): void {
    socket.send(generate(packet, MQTT5));
  }

  function deliver(socket: WebSocket, topic: string, payload: Uint8Array, retain: boolean): void {
    send(socket, { cmd: "publish", topic, payload: Buffer.from(payload), qos: 0, retain, dup: false });
  }

  function handle(socket: WebSocket, packet: Packet): void {
    if (packet.cmd === "connect") {
      send(socket, { cmd: "connack", sessionPresent: false, reasonCode: REASON_SUCCESS });

      return;
    }

    if (packet.cmd === "subscribe") {
      const topics = packet.subscriptions.map(({ topic }) => topic);

      subscribers.set(socket, new Set(topics));
      send(socket, { cmd: "suback", messageId: packetId(packet.messageId), granted: topics.map(() => QOS_AT_LEAST_ONCE) });

      for (const topic of topics) {
        const payload = retained.get(topic);

        if (payload !== undefined) deliver(socket, topic, payload, true);
      }

      return;
    }

    if (packet.cmd === "publish") {
      if (packet.qos > 0) send(socket, { cmd: "puback", messageId: packetId(packet.messageId), reasonCode: REASON_SUCCESS });

      const payload = new Uint8Array(Buffer.from(packet.payload));

      waiting.shift()?.({ suffix: packet.topic.slice(TOPIC_PREFIX.length), payload });

      return;
    }

    if (packet.cmd === "pingreq") send(socket, { cmd: "pingresp" });
  }

  await once(server, "listening");
  server.on("connection", (socket) => {
    const packets = parser(MQTT5);

    packets.on("packet", (packet) => handle(socket, packet));
    socket.on("message", (data: Buffer) => packets.parse(data));
    socket.on("close", () => subscribers.delete(socket));
  });

  // SAFETY: a ws server listening on a port reports an AddressInfo, not a pipe name.
  const { port } = server.address() as AddressInfo;

  return {
    transport: new AwsTransport({
      deviceId: DEVICE_ID,
      identityId: "eu-west-1:test-identity",
      clientConfig: (clientId) => ({
        hostName: "127.0.0.1",
        port,
        connectProperties: { clientId, keepAliveIntervalSeconds: 60 },
        websocketOptions: { urlFactoryOptions: { urlFactory: mqtt5.Mqtt5WebsocketUrlFactoryType.Ws } },
      }),
      refreshCredentials: async () => undefined,
    }),
    node: {
      publish: async (suffix, payload, retain) => {
        const topic = `${TOPIC_PREFIX}${suffix}`;

        if (retain) retained.set(topic, payload);

        for (const [socket, topics] of subscribers) {
          if (topics.has(topic)) deliver(socket, topic, payload, false);
        }
      },
      received: () => new Promise((resolve) => waiting.push(resolve)),
      settle: () => sleep(100),
    },
  };
});
