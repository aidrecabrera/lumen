import { decode, encode } from "@msgpack/msgpack";
import * as v from "valibot";

// mirrors include/lumen_app_types.h and include/lumen_board_config.h; the fixtures in
// test/fixtures/contract fail on whichever side drifts.

export const TOPIC = {
  telemetry: "telemetry",
  status: "status",
  energy: "energy",
  availability: "availability",
  commandLed: "command/led",
  commandConfig: "command/config",
  commandMode: "command/mode",
} as const;

export const ACK_PREFIX = "ack/";

export const SUBSCRIBED_SUFFIXES = [
  TOPIC.availability,
  TOPIC.status,
  TOPIC.telemetry,
  TOPIC.energy,
  `${ACK_PREFIX}+`,
] as const;

export type CommandSuffix = typeof TOPIC.commandLed | typeof TOPIC.commandConfig | typeof TOPIC.commandMode;

export const LUX_CEILING_MIN = 100;
export const LUX_CEILING_MAX = 65535;

const DEVICE_MODES = ["autonomous", "manual"] as const;
const DEGRADED_TIERS = ["none", "wifi_retrying", "wifi_retry_limit"] as const;
const ACK_RESULTS = ["applied", "rejected", "deferred"] as const;

export const ACK_REASONS = [
  "mode applied",
  "config applied",
  "led applied",
  "not ready",
  "persist failed",
  "manual locked",
  "invalid payload",
  "command queue full",
] as const;

export const AUTO_STATES = ["active", "night_off", "paused_out_of_range"] as const;
export const CLOUD_STATES = ["connected", "connecting", "not_configured", "invalid_config"] as const;

// the firmware sends these enums as their index.
function indexOf<const T extends readonly string[]>(names: T): v.GenericSchema<number, T[number]> {
  return v.pipe(
    v.number(),
    v.integer(),
    v.minValue(0),
    v.maxValue(names.length - 1),
    // SAFETY: the integer and range checks above keep index inside names.
    v.transform((index) => names[index] as T[number]),
  );
}

const finite = v.pipe(v.number(), v.finite());
const percent = v.pipe(v.number(), v.integer(), v.minValue(0), v.maxValue(100));
const count = v.pipe(v.number(), v.integer(), v.minValue(0));

const LedStateSchema = v.object({
  power: v.boolean(),
  brightness_pct: percent,
  red_enabled: v.boolean(),
  blue_enabled: v.boolean(),
  far_red_enabled: v.boolean(),
  red_dist_pct: percent,
  blue_dist_pct: percent,
  far_red_dist_pct: percent,
});

const ThresholdsSchema = v.object({
  temp_min_c: finite,
  temp_max_c: finite,
  humidity_min_pct: finite,
  humidity_max_pct: finite,
});

const TelemetrySchema = v.object({
  timestamp_ms: count,
  light_lux: finite,
  temperature_c: finite,
  humidity_pct: finite,
  sequence: count,
});

const StatusSchema = v.object({
  timestamp_ms: count,
  mode: indexOf(DEVICE_MODES),
  degraded_tier: indexOf(DEGRADED_TIERS),
  uptime_sec: count,
  led: LedStateSchema,
  auto_state: v.picklist(AUTO_STATES),
  cloud: v.picklist(CLOUD_STATES),
  thresholds: ThresholdsSchema,
  lux_ceiling: finite,
});

const EnergySchema = v.object({
  timestamp_ms: count,
  total_wh: finite,
  session_wh: finite,
  light_on_sec: count,
  max_brightness_sec: count,
});

const AckSchema = v.object({
  command_id: v.string(),
  result: indexOf(ACK_RESULTS),
  timestamp_ms: count,
  mode: indexOf(DEVICE_MODES),
  reason: v.picklist(ACK_REASONS),
  led: LedStateSchema,
});

export type DeviceMode = (typeof DEVICE_MODES)[number];

export type AckReason = (typeof ACK_REASONS)[number];

export type AutoState = (typeof AUTO_STATES)[number];

export type CloudState = (typeof CLOUD_STATES)[number];

export type LedState = v.InferOutput<typeof LedStateSchema>;

export type Thresholds = v.InferOutput<typeof ThresholdsSchema>;

export type Telemetry = v.InferOutput<typeof TelemetrySchema>;

export type Status = v.InferOutput<typeof StatusSchema>;

export type Energy = v.InferOutput<typeof EnergySchema>;

export type Ack = v.InferOutput<typeof AckSchema>;

export type Inbound =
  | { kind: "telemetry"; value: Telemetry }
  | { kind: "status"; value: Status }
  | { kind: "energy"; value: Energy }
  | { kind: "availability"; online: boolean }
  | { kind: "ack"; value: Ack };

export type Command =
  | { kind: "led"; led: LedState }
  | { kind: "config"; thresholds: Thresholds; lux_ceiling: number }
  | { kind: "mode"; mode: DeviceMode };

export type DecodeResult = { ok: true; message: Inbound } | { ok: false; error: string };

export function decodeInbound(suffix: string, payload: Uint8Array): DecodeResult {
  if (suffix === TOPIC.availability) {
    return decodeAvailability(payload);
  }

  if (suffix === TOPIC.telemetry) {
    return decodeWith(TelemetrySchema, payload, (value) => ({ kind: "telemetry", value }));
  }

  if (suffix === TOPIC.status) {
    return decodeWith(StatusSchema, payload, (value) => ({ kind: "status", value }));
  }

  if (suffix === TOPIC.energy) {
    return decodeWith(EnergySchema, payload, (value) => ({ kind: "energy", value }));
  }

  if (suffix.startsWith(ACK_PREFIX)) {
    const commandId = suffix.slice(ACK_PREFIX.length);

    const ackFor = v.pipe(
      AckSchema,
      v.check((ack) => ack.command_id === commandId, "ack topic does not match command_id"),
    );

    return decodeWith(ackFor, payload, (value) => ({ kind: "ack", value }));
  }

  return { ok: false, error: `unknown suffix "${suffix}"` };
}

function decodeAvailability(payload: Uint8Array): DecodeResult {
  const text = new TextDecoder().decode(payload);

  if (text === "online") {
    return { ok: true, message: { kind: "availability", online: true } };
  }

  if (text === "offline") {
    return { ok: true, message: { kind: "availability", online: false } };
  }

  return { ok: false, error: `unknown availability "${text}"` };
}

function decodeWith<T>(
  schema: v.GenericSchema<unknown, T>,
  payload: Uint8Array,
  toMessage: (value: T) => Inbound,
): DecodeResult {
  const result = v.safeParse(schema, decodeOrUndefined(payload));

  if (!result.success) {
    return { ok: false, error: v.summarize(result.issues) };
  }

  return { ok: true, message: toMessage(result.output) };
}

function decodeOrUndefined(payload: Uint8Array): ReturnType<typeof decode> {
  try {
    return decode(payload);
  } catch {
    return undefined;
  }
}

export function commandSuffix(command: Command): CommandSuffix {
  switch (command.kind) {
    case "led":
      return TOPIC.commandLed;
    case "config":
      return TOPIC.commandConfig;
    case "mode":
      return TOPIC.commandMode;
  }
}

export function encodeCommand(commandId: string, command: Command): Uint8Array {
  switch (command.kind) {
    case "led":
      return encode({ command_id: commandId, ...command.led });
    case "config":
      return encode({ command_id: commandId, thresholds: command.thresholds, lux_ceiling: command.lux_ceiling });
    case "mode":
      return encode({ command_id: commandId, mode: DEVICE_MODES.indexOf(command.mode) });
  }
}

const commandId = { command_id: v.string() };

const COMMAND_SCHEMAS = {
  [TOPIC.commandLed]: v.pipe(
    v.object({ ...commandId, ...LedStateSchema.entries }),
    v.transform(({ command_id, ...led }) => ({ commandId: command_id, command: { kind: "led", led } as const })),
  ),
  [TOPIC.commandConfig]: v.pipe(
    v.object({ ...commandId, thresholds: ThresholdsSchema, lux_ceiling: finite }),
    v.transform(({ command_id, thresholds, lux_ceiling }) => ({
      commandId: command_id,
      command: { kind: "config", thresholds, lux_ceiling } as const,
    })),
  ),
  [TOPIC.commandMode]: v.pipe(
    v.object({ ...commandId, mode: indexOf(DEVICE_MODES) }),
    v.transform(({ command_id, mode }) => ({ commandId: command_id, command: { kind: "mode", mode } as const })),
  ),
};

export interface AddressedCommand {
  commandId: string;
  command: Command;
}

export function decodeCommand(suffix: CommandSuffix, payload: Uint8Array): AddressedCommand | undefined {
  const result = v.safeParse(COMMAND_SCHEMAS[suffix], decodeOrUndefined(payload));

  return result.success ? result.output : undefined;
}

export function topicFor(deviceId: string, suffix: string): string {
  return `lumen/${deviceId}/${suffix}`;
}

export function suffixFor(deviceId: string, topic: string): string | undefined {
  const prefix = topicFor(deviceId, "");

  return topic.startsWith(prefix) ? topic.slice(prefix.length) : undefined;
}
