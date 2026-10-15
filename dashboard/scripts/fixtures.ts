import { encode } from "@msgpack/msgpack";
import { encodeCommand, type Ack, type Command, type Energy, type Status, type Telemetry } from "../src/core/contract";
import { encodeFrame } from "../src/transport/frame";

// float fields use non-integer values: the firmware writes every float as float32, while an
// integer-valued js number would encode as an int.
const f32 = { forceFloat32: true } as const;

export const LED = {
  power: true,
  brightness_pct: 72,
  red_enabled: true,
  blue_enabled: true,
  far_red_enabled: false,
  red_dist_pct: 60,
  blue_dist_pct: 30,
  far_red_dist_pct: 10,
};

export const telemetry: Telemetry = {
  timestamp_ms: 123456,
  light_lux: 412.5,
  temperature_c: 23.5,
  humidity_pct: 61.25,
  sequence: 42,
};

const statusWire = {
  timestamp_ms: 123500,
  mode: 1,
  degraded_tier: 0,
  uptime_sec: 123,
  led: LED,
  auto_state: "night_off",
  cloud: "not_configured",
  thresholds: { temp_min_c: 18.5, temp_max_c: 28.5, humidity_min_pct: 55.5, humidity_max_pct: 80.5 },
  lux_ceiling: 9500.5,
};

export const status: Status = {
  ...statusWire,
  mode: "manual",
  degraded_tier: "none",
  auto_state: "night_off",
  cloud: "not_configured",
};

export const energy: Energy = {
  timestamp_ms: 130000,
  total_wh: 12.5,
  session_wh: 0.75,
  light_on_sec: 3600,
  max_brightness_sec: 120,
};

const ackWire = {
  command_id: "0f8fad5b-d9cb-469f-a165-70867728950e",
  result: 1,
  timestamp_ms: 123600,
  mode: 0,
  reason: "manual locked",
  led: LED,
};

export const ack: Ack = { ...ackWire, result: "rejected", mode: "autonomous", reason: "manual locked" };

export const COMMAND_ID = "7c9e6679-7425-40de-944b-e07fc1f90ae7";

export const commands = {
  led: { kind: "led", led: LED },
  config: {
    kind: "config",
    thresholds: { temp_min_c: 18.5, temp_max_c: 28, humidity_min_pct: 55, humidity_max_pct: 80.5 },
    lux_ceiling: 8000,
  },
  mode: { kind: "mode", mode: "manual" },
} satisfies Record<string, Command>;

const telemetryBytes = encode(telemetry, f32);

export const files: Record<string, Uint8Array> = {
  "telemetry.msgpack": telemetryBytes,
  "status.msgpack": encode(statusWire, f32),
  "energy.msgpack": encode(energy, f32),
  "ack.msgpack": encode(ackWire, f32),
  "availability-online.txt": new TextEncoder().encode("online"),
  "availability-offline.txt": new TextEncoder().encode("offline"),
  "command-led.msgpack": encodeCommand(COMMAND_ID, commands.led),
  "command-config.msgpack": encodeCommand(COMMAND_ID, commands.config),
  "command-mode.msgpack": encodeCommand(COMMAND_ID, commands.mode),
  "frame-telemetry.msgpack": encodeFrame("telemetry", telemetryBytes),
  "frame-command-mode.msgpack": encodeFrame("command/mode", encodeCommand(COMMAND_ID, commands.mode)),
};
