import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { PRESETS, parseSettings, rebalanceShares, switchToAutoSteps } from "../src/core/actions.ts";
import { ACK_MESSAGES, ACK_TIMEOUT_MS, CommandSender, describeOutcome, runSequence } from "../src/core/commands.ts";
import { ACK_REASONS, type Ack, type Inbound, type LedState } from "../src/core/contract.ts";
import { describeConnection, HISTORY_LIMIT, initialState, reduce, type DashboardState } from "../src/core/state.ts";
import { FakeTransport } from "../src/transport/fake.ts";
import * as fixtures from "../scripts/fixtures.ts";

const LED: LedState = fixtures.LED;

function receive(state: DashboardState, message: Inbound, at = 1000, retained = false): DashboardState {
  return reduce(state, { type: "message", message, retained, at });
}

function connectedFake(): FakeTransport {
  const transport = new FakeTransport();

  transport.setLink({ kind: "connected" });

  return transport;
}

const connected = reduce(initialState, { type: "link", link: { kind: "connected" } });

describe("dashboard state", () => {
  it("has no status until the first one arrives", () => {
    expect(initialState.status).toEqual({ kind: "none" });
    const state = receive(connected, { kind: "status", value: fixtures.status });

    expect(state.status).toEqual({ kind: "known", value: fixtures.status, receivedAt: 1000, lastKnown: false });
  });

  it("marks retained values as last known and does not count them as live", () => {
    const state = receive(connected, { kind: "telemetry", value: fixtures.telemetry }, 1000, true);

    expect(state.telemetry).toMatchObject({ lastKnown: true });
    expect(state.lastMessageAt).toBeUndefined();
    expect(state.history).toEqual([]);
  });

  it("uses the browser receive time, never the node uptime clock", () => {
    const state = receive(connected, { kind: "telemetry", value: fixtures.telemetry }, 5555);

    expect(state.history).toEqual([{ at: 5555, lux: 412.5, temperature: 23.5, humidity: 61.25 }]);
  });

  it("keeps history bounded", () => {
    let state = connected;

    for (let at = 0; at < HISTORY_LIMIT + 5; at += 1) {
      state = receive(state, { kind: "telemetry", value: fixtures.telemetry }, at);
    }

    expect(state.history).toHaveLength(HISTORY_LIMIT);
    expect(state.history[0]?.at).toBe(5);
  });

  it("tells offline-since apart from offline before the page opened", () => {
    const live = receive(connected, { kind: "availability", online: false }, Date.UTC(2026, 0, 1, 14, 2));
    const retained = receive(connected, { kind: "availability", online: false }, 1000, true);

    expect(describeConnection(live, 0, 6000).text).toMatch(/^Node offline since /);
    expect(describeConnection(retained, 0, 6000).text).toBe("Node offline (last seen before this page opened)");
  });

  it("reports a silent node after the stale window", () => {
    const state = receive(connected, { kind: "telemetry", value: fixtures.telemetry }, 1000);

    expect(describeConnection(state, 6000, 6000).text).toBe("Live");
    expect(describeConnection(state, 7001, 6000).text).toBe("Node not responding");
  });

  it("applies the led state and mode an ack reports", () => {
    const withStatus = receive(connected, { kind: "status", value: fixtures.status });
    const state = receive(withStatus, { kind: "ack", value: fixtures.ack });

    expect(state.status).toMatchObject({ value: { mode: "autonomous", led: fixtures.ack.led } });
  });
});

describe("commands", () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => vi.useRealTimers());

  function ackFor(commandId: string, result: Ack["result"], reason: Ack["reason"]): Ack {
    return { ...fixtures.ack, command_id: commandId, result, reason };
  }

  it("sends a command with a uuid and resolves on its ack", async () => {
    const transport = connectedFake();
    const sender = new CommandSender(transport);
    const outcome = sender.send({ kind: "mode", mode: "manual" });
    const sent = transport.lastCommand();

    expect(sent.suffix).toBe("command/mode");
    expect(sent.commandId).toMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/);
    sender.receiveAck(ackFor(sent.commandId, "applied", "mode applied"));
    await expect(outcome).resolves.toMatchObject({ kind: "applied" });
  });

  it("marks a command without an ack as timed out after 10 s", async () => {
    const sender = new CommandSender(connectedFake());
    const outcome = sender.send({ kind: "mode", mode: "manual" });

    vi.advanceTimersByTime(ACK_TIMEOUT_MS);
    await expect(outcome).resolves.toEqual({ kind: "timeout" });
    expect(describeOutcome({ kind: "timeout" })).toBe("No answer from the node");
  });

  it("reports a rejected command with a plain reason", async () => {
    const transport = connectedFake();
    const sender = new CommandSender(transport);
    const outcome = sender.send({ kind: "led", led: LED });

    sender.receiveAck(ackFor(transport.lastCommand().commandId, "rejected", "manual locked"));
    expect(describeOutcome(await outcome)).toBe(ACK_MESSAGES["manual locked"]);
  });

  it("has a message for every ack reason", () => {
    for (const reason of ACK_REASONS) expect(ACK_MESSAGES[reason]).not.toBe("");
  });

  it("stops a preset at the first failed step", async () => {
    const transport = connectedFake();
    const sender = new CommandSender(transport);
    const balanced = PRESETS.find((preset) => preset.id === "balanced");

    if (balanced === undefined) throw new Error("balanced preset missing");

    const result = runSequence(sender, balanced.steps, LED);

    await vi.waitFor(() => expect(transport.sent).toHaveLength(1));
    sender.receiveAck(ackFor(transport.lastCommand().commandId, "applied", "mode applied"));
    await vi.waitFor(() => expect(transport.sent).toHaveLength(2));
    expect(transport.lastCommand().command).toMatchObject({ led: { red_dist_pct: 60, blue_dist_pct: 30, far_red_dist_pct: 10 } });
    sender.receiveAck(ackFor(transport.lastCommand().commandId, "rejected", "persist failed"));
    await expect(result).resolves.toMatchObject({ kind: "stopped", stepNumber: 2 });
    expect(transport.sent).toHaveLength(2);
  });
});

describe("light actions", () => {
  it("keeps linked shares at 100 in proportion", () => {
    expect(rebalanceShares({ red_dist_pct: 60, blue_dist_pct: 30, far_red_dist_pct: 10 }, "red", 80)).toEqual({
      red_dist_pct: 80,
      blue_dist_pct: 15,
      far_red_dist_pct: 5,
    });
    expect(rebalanceShares({ red_dist_pct: 100, blue_dist_pct: 0, far_red_dist_pct: 0 }, "red", 51)).toEqual({
      red_dist_pct: 51,
      blue_dist_pct: 25,
      far_red_dist_pct: 24,
    });
  });

  it("turns power on before switching to auto when the lights are off", () => {
    expect(switchToAutoSteps({ ...LED, power: false }).map((step) => step.label)).toEqual([
      "Turn the lights on",
      "Switch to auto",
    ]);
    expect(switchToAutoSteps(LED).map((step) => step.label)).toEqual(["Switch to auto"]);
  });

  it("parses settings into a config command", () => {
    const form = { temp_min_c: "18", temp_max_c: "28", humidity_min_pct: "60", humidity_max_pct: "80", lux_ceiling: "9000" };

    expect(parseSettings(form)).toMatchObject({ ok: true, command: { kind: "config", lux_ceiling: 9000 } });
    expect(parseSettings({ ...form, temp_min_c: "30" })).toMatchObject({ ok: false, errors: { temp_max_c: expect.any(String) } });
    expect(parseSettings({ ...form, lux_ceiling: "5" })).toMatchObject({ ok: false, errors: { lux_ceiling: expect.any(String) } });
  });
});
