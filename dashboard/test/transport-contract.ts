import { describe, expect, it } from "vitest";

import type { CommandSuffix } from "../src/core/contract.ts";
import type { Link, Transport } from "../src/transport/transport.ts";
import * as fixtures from "../scripts/fixtures.ts";

export interface NodeSide {
  publish(suffix: string, payload: Uint8Array, retained: boolean): Promise<void>;
  received(): Promise<{ suffix: string; payload: Uint8Array }>;
}

export interface TransportHarness {
  transport: Transport;
  node: NodeSide;
}

export function describeTransportContract(name: string, setUp: () => Promise<TransportHarness>): void {
  describe(`${name} transport contract`, () => {
    it("reports connected after connect", async () => {
      const { transport } = await setUp();
      const links: Link["kind"][] = [];

      transport.onConnectionChange((link) => links.push(link.kind));
      await transport.connect();

      expect(links.at(-1)).toBe("connected");
      transport.disconnect();
    });

    it("delivers node messages with their suffix and retain flag", async () => {
      const { transport, node } = await setUp();
      const messages: { suffix: string; payload: Uint8Array; retained: boolean }[] = [];

      transport.onMessage((suffix, payload, retained) => messages.push({ suffix, payload, retained }));
      await transport.connect();
      await node.publish("telemetry", fixtures.files["telemetry.msgpack"], false);
      await node.publish("status", fixtures.files["status.msgpack"], true);

      expect(messages.map(({ suffix, retained }) => ({ suffix, retained }))).toEqual([
        { suffix: "telemetry", retained: false },
        { suffix: "status", retained: true },
      ]);
      expect(Array.from(messages[0]?.payload ?? [])).toEqual(Array.from(fixtures.files["telemetry.msgpack"]));
      transport.disconnect();
    });

    it("sends commands to the node unchanged", async () => {
      const { transport, node } = await setUp();
      const suffix: CommandSuffix = "command/mode";

      await transport.connect();
      transport.sendCommand(suffix, fixtures.files["command-mode.msgpack"]);

      const received = await node.received();

      expect(received.suffix).toBe(suffix);
      expect(Array.from(received.payload)).toEqual(Array.from(fixtures.files["command-mode.msgpack"]));
      transport.disconnect();
    });

    it("refuses to send while disconnected", async () => {
      const { transport } = await setUp();

      expect(() => transport.sendCommand("command/mode", fixtures.files["command-mode.msgpack"])).toThrow();
    });
  });
}
