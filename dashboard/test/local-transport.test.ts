// @vitest-environment node
import { once } from "node:events";
import type { AddressInfo } from "node:net";
import { setTimeout as sleep } from "node:timers/promises";
import { WebSocketServer, type WebSocket } from "ws";

import { decodeFrame, encodeFrame } from "../src/transport/frame.ts";
import { LocalTransport } from "../src/transport/local.ts";
import { describeTransportContract } from "./transport-contract.ts";

// plays the node: like the firmware, it sends its latest state to each new client.
describeTransportContract("local", async () => {
  const server = new WebSocketServer({ port: 0 });
  const latest = new Map<string, Uint8Array>();
  const clients = new Set<WebSocket>();

  await once(server, "listening");
  server.on("connection", (client) => {
    clients.add(client);

    for (const [suffix, payload] of latest) {
      client.send(encodeFrame(suffix, payload));
    }
  });

  // SAFETY: a ws server listening on a port reports an AddressInfo, not a pipe name.
  const { port } = server.address() as AddressInfo;

  return {
    transport: new LocalTransport(`ws://127.0.0.1:${port}/ws`),
    node: {
      publish: async (suffix, payload, retained) => {
        if (retained) {
          latest.set(suffix, payload);
        }

        for (const client of clients) {
          client.send(encodeFrame(suffix, payload));
        }
      },
      received: async () => {
        const [client] = clients;

        if (client === undefined) {
          throw new Error("no client connected");
        }

        const [data] = await once(client, "message");
        const frame = decodeFrame(new Uint8Array(data));

        if (frame === undefined) {
          throw new Error("client sent an invalid frame");
        }

        return { suffix: frame.t, payload: frame.p };
      },
      settle: () => sleep(100),
    },
  };
});
