import { decode } from "@msgpack/msgpack";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { describe, expect, it } from "vitest";
import { decodeInbound } from "../src/core/contract.ts";
import { decodeFrame } from "../src/transport/frame.ts";
import * as fixtures from "../scripts/fixtures.ts";

const dir = resolve(import.meta.dirname, "../../test/fixtures/contract");
const read = (name: string) => new Uint8Array(readFileSync(resolve(dir, name)));

describe("contract fixtures", () => {
  it("match the generator", () => {
    for (const [name, bytes] of Object.entries(fixtures.files)) {
      expect(Array.from(read(name)), name).toEqual(Array.from(bytes));
    }
  });

  it("decode telemetry", () => {
    expect(decodeInbound("telemetry", read("telemetry.msgpack"))).toEqual({
      ok: true,
      message: { kind: "telemetry", value: fixtures.telemetry },
    });
  });

  it("decode status", () => {
    expect(decodeInbound("status", read("status.msgpack"))).toEqual({
      ok: true,
      message: { kind: "status", value: fixtures.status },
    });
  });

  it("decode energy", () => {
    expect(decodeInbound("energy", read("energy.msgpack"))).toEqual({
      ok: true,
      message: { kind: "energy", value: fixtures.energy },
    });
  });

  it("decode ack", () => {
    expect(decodeInbound(`ack/${fixtures.ack.command_id}`, read("ack.msgpack"))).toEqual({
      ok: true,
      message: { kind: "ack", value: fixtures.ack },
    });
  });

  it("decode availability", () => {
    expect(decodeInbound("availability", read("availability-online.txt"))).toEqual({
      ok: true,
      message: { kind: "availability", online: true },
    });
    expect(decodeInbound("availability", read("availability-offline.txt"))).toEqual({
      ok: true,
      message: { kind: "availability", online: false },
    });
  });

  it("command fixtures carry the command id and fields the firmware reads", () => {
    expect(decode(read("command-mode.msgpack"))).toEqual({ command_id: fixtures.COMMAND_ID, mode: 1 });
    expect(decode(read("command-led.msgpack"))).toEqual({ command_id: fixtures.COMMAND_ID, ...fixtures.LED });
    expect(decode(read("command-config.msgpack"))).toEqual({
      command_id: fixtures.COMMAND_ID,
      thresholds: fixtures.commands.config.thresholds,
      lux_ceiling: 8000,
    });
  });

  it("frames wrap the topic suffix and payload bytes", () => {
    const frame = decodeFrame(read("frame-telemetry.msgpack"));
    expect(frame?.t).toBe("telemetry");
    expect(frame?.p).toEqual(read("telemetry.msgpack"));
  });

  it("reject wrong shapes", () => {
    expect(decodeInbound("status", read("telemetry.msgpack")).ok).toBe(false);
    expect(decodeInbound("ack/other-id", read("ack.msgpack")).ok).toBe(false);
    expect(decodeInbound("unknown", read("telemetry.msgpack")).ok).toBe(false);
    expect(decodeInbound("telemetry", new Uint8Array([0xc1])).ok).toBe(false);
    expect(decodeFrame(read("telemetry.msgpack"))).toBeUndefined();
  });
});
