// @vitest-environment node
import { Aedes } from "aedes";
import { once } from "node:events";
import { createServer, type AddressInfo } from "node:net";
import { setTimeout as sleep } from "node:timers/promises";
import { afterAll, beforeAll } from "vitest";

import { MqttTransport } from "../src/transport/mqtt.ts";
import { describeTransportContract } from "./transport-contract.ts";

const DEVICE_ID = "lumen-aabbccddeeff";
let broker: Aedes;
let port: number;

beforeAll(async () => {
  broker = await Aedes.createBroker();

  const server = createServer(broker.handle);

  server.listen(0);
  await once(server, "listening");

  // SAFETY: a net server listening on a port reports an AddressInfo, not a pipe name.
  port = (server.address() as AddressInfo).port;
});

afterAll(() => broker.close());

describeTransportContract("mqtt", async () => {
  const topicPrefix = `lumen/${DEVICE_ID}/`;

  return {
    transport: new MqttTransport({ url: `mqtt://127.0.0.1:${port}`, deviceId: DEVICE_ID, credentials: undefined }),
    node: {
      publish: (suffix, payload, retained) =>
        new Promise<void>((resolve, reject) =>
          broker.publish(
            { cmd: "publish", topic: `${topicPrefix}${suffix}`, payload: Buffer.from(payload), qos: 0, retain: retained, dup: false },
            (error) => (error ? reject(error) : resolve()),
          ),
        ),
      received: () =>
        new Promise((resolve) => {
          broker.on("publish", (packet, client) => {
            if (client !== null) resolve({ suffix: packet.topic.slice(topicPrefix.length), payload: new Uint8Array(Buffer.from(packet.payload)) });
          });
        }),
      settle: () => sleep(100),
    },
  };
});
