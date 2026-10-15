import { encode } from "@msgpack/msgpack";

import type { Ack, AckReason, Command, Status } from "../core/contract.ts";
import { FakeTransport, type SentCommand } from "../transport/fake.ts";
import { devPanelView } from "../ui/components/dev-panel.ts";
import type { Target } from "./types.ts";

// a simulated node for the clickable prototype. it follows the firmware rules that the ui
// depends on: manual lock, share total of 100, and acks that carry the applied state.
class SimulatedNode {
  readonly #transport: FakeTransport;
  readonly #startedAt = Date.now();
  #status: Status = {
    timestamp_ms: 0,
    mode: "autonomous",
    degraded_tier: "none",
    uptime_sec: 0,
    led: {
      power: true,
      brightness_pct: 50,
      red_enabled: true,
      blue_enabled: true,
      far_red_enabled: true,
      red_dist_pct: 40,
      blue_dist_pct: 35,
      far_red_dist_pct: 25,
    },
    auto_state: "active",
    cloud: "not_configured",
    thresholds: { temp_min_c: 18, temp_max_c: 28, humidity_min_pct: 60, humidity_max_pct: 80 },
    lux_ceiling: 10000,
  };
  #online = true;
  #rejectNext = false;
  #dropNextAck = false;
  #sequence = 0;
  #totalWh = 120;

  constructor(transport: FakeTransport) {
    this.#transport = transport;
    transport.onCommand((command) => this.#handle(command));
    setInterval(() => this.#tick(), 2000);
    setInterval(() => this.#publishEnergy(), 10_000);
  }

  start(): void {
    this.#publish("availability", new TextEncoder().encode("online"), true);
    this.#publishStatus();
  }

  goOffline(): void {
    this.#online = false;
    this.#transport.deliver("availability", new TextEncoder().encode("offline"));
  }

  comeBack(): void {
    this.#online = true;
    this.start();
  }

  rejectNext(): void {
    this.#rejectNext = true;
  }

  dropNextAck(): void {
    this.#dropNextAck = true;
  }

  nightOff(): void {
    this.#status = { ...this.#status, auto_state: "night_off", led: { ...this.#status.led, brightness_pct: 0 } };
    this.#publishStatus();
  }

  #handle({ commandId, command }: SentCommand): void {
    if (!this.#online) return;

    const reason = this.#apply(command);

    const ack: Ack = {
      command_id: commandId,
      result: reason.endsWith("applied") ? "applied" : "rejected",
      timestamp_ms: this.#uptimeMs(),
      mode: this.#status.mode,
      reason,
      led: this.#status.led,
    };

    setTimeout(() => {
      if (this.#dropNextAck) {
        this.#dropNextAck = false;

        return;
      }

      this.#publish(`ack/${commandId}`, encode({ ...ack, result: ack.result === "applied" ? 0 : 1, mode: ack.mode === "autonomous" ? 0 : 1 }));
      this.#publishStatus();
    }, 400);
  }

  #apply(command: Command): AckReason {
    if (this.#rejectNext) {
      this.#rejectNext = false;

      return "persist failed";
    }

    switch (command.kind) {
      case "mode":
        this.#status = { ...this.#status, mode: command.mode };

        return "mode applied";
      case "config":
        this.#status = { ...this.#status, thresholds: command.thresholds, lux_ceiling: command.lux_ceiling };

        return "config applied";
      case "led":
        if (this.#status.mode === "autonomous") return "manual locked";

        this.#status = { ...this.#status, led: command.led };

        return "led applied";
    }
  }

  #tick(): void {
    if (!this.#online) return;

    const lux = 300 + 200 * Math.sin(Date.now() / 60_000);

    this.#sequence += 1;
    this.#publish(
      "telemetry",
      encode({ timestamp_ms: this.#uptimeMs(), light_lux: lux, temperature_c: 23 + Math.random(), humidity_pct: 65 + Math.random() * 3, sequence: this.#sequence }),
    );
  }

  #publishEnergy(): void {
    if (!this.#online) return;

    this.#totalWh += 0.02;

    const uptime = Math.round(this.#uptimeMs() / 1000);

    this.#publish("energy", encode({ timestamp_ms: this.#uptimeMs(), total_wh: this.#totalWh, session_wh: this.#totalWh - 120, light_on_sec: uptime, max_brightness_sec: 0 }));
  }

  #publishStatus(): void {
    const uptimeMs = this.#uptimeMs();

    const wire = {
      ...this.#status,
      timestamp_ms: uptimeMs,
      uptime_sec: Math.round(uptimeMs / 1000),
      mode: this.#status.mode === "autonomous" ? 0 : 1,
      degraded_tier: 0,
    };

    this.#publish("status", encode(wire), true);
  }

  #publish(suffix: string, payload: Uint8Array, retained = false): void {
    if (this.#online) this.#transport.deliver(suffix, payload, retained);
  }

  #uptimeMs(): number {
    return Date.now() - this.#startedAt;
  }
}

export async function createTarget(): Promise<Target> {
  const transport = new FakeTransport();
  const node = new SimulatedNode(transport);

  transport.onConnectionChange((link) => {
    if (link.kind === "connected") node.start();
  });

  return {
    transport,
    staleAfterMs: 6000,
    extras: () =>
      devPanelView({
        goOffline: () => node.goOffline(),
        comeBack: () => node.comeBack(),
        rejectNext: () => node.rejectNext(),
        dropNextAck: () => node.dropNextAck(),
        nightOff: () => node.nightOff(),
      }),
  };
}
