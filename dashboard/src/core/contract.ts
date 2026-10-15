import { decode, encode } from "@msgpack/msgpack";

// mirrors include/lumen_app_types.h and include/lumen_board_config.h; fixtures in
// test/fixtures/contract keep both sides honest.

export const TOPIC_ROOT = "lumen";

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

export type DeviceMode = "autonomous" | "manual";
const DEVICE_MODES: readonly DeviceMode[] = ["autonomous", "manual"];

export type DegradedTier = "none" | "wifi_retrying" | "wifi_retry_limit";
const DEGRADED_TIERS: readonly DegradedTier[] = ["none", "wifi_retrying", "wifi_retry_limit"];

export type AckResult = "applied" | "rejected" | "deferred";
const ACK_RESULTS: readonly AckResult[] = ["applied", "rejected", "deferred"];

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
export type AckReason = (typeof ACK_REASONS)[number];

export const AUTO_STATES = ["active", "night_off", "paused_out_of_range"] as const;
export type AutoState = (typeof AUTO_STATES)[number];

export const CLOUD_STATES = ["connected", "connecting", "not_configured", "invalid_config"] as const;
export type CloudState = (typeof CLOUD_STATES)[number];

export const LUX_CEILING_MIN = 100;
export const LUX_CEILING_MAX = 65535;

export interface LedState {
  power: boolean;
  brightness_pct: number;
  red_enabled: boolean;
  blue_enabled: boolean;
  far_red_enabled: boolean;
  red_dist_pct: number;
  blue_dist_pct: number;
  far_red_dist_pct: number;
}

export interface Thresholds {
  temp_min_c: number;
  temp_max_c: number;
  humidity_min_pct: number;
  humidity_max_pct: number;
}

export interface Telemetry {
  timestamp_ms: number;
  light_lux: number;
  temperature_c: number;
  humidity_pct: number;
  sequence: number;
}

export interface Status {
  timestamp_ms: number;
  mode: DeviceMode;
  degraded_tier: DegradedTier;
  uptime_sec: number;
  led: LedState;
  auto_state: AutoState;
  cloud: CloudState;
  thresholds: Thresholds;
  lux_ceiling: number;
}

export interface Energy {
  timestamp_ms: number;
  total_wh: number;
  session_wh: number;
  light_on_sec: number;
  max_brightness_sec: number;
}

export interface Ack {
  command_id: string;
  result: AckResult;
  timestamp_ms: number;
  mode: DeviceMode;
  reason: AckReason;
  led: LedState;
}

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

class ContractError extends Error {}

type Fields = Record<string, unknown>;

function asMap(value: unknown, what: string): Fields {
  if (typeof value !== "object" || value === null || Array.isArray(value) || value instanceof Uint8Array) {
    throw new ContractError(`${what}: expected a map`);
  }
  return value as Fields;
}

function num(fields: Fields, key: string): number {
  const value = fields[key];
  if (typeof value === "bigint") return Number(value);
  if (typeof value !== "number" || !Number.isFinite(value)) {
    throw new ContractError(`${key}: expected a finite number`);
  }
  return value;
}

function uint(fields: Fields, key: string, max: number): number {
  const value = num(fields, key);
  if (!Number.isInteger(value) || value < 0 || value > max) {
    throw new ContractError(`${key}: expected an integer 0..${max}`);
  }
  return value;
}

function bool(fields: Fields, key: string): boolean {
  const value = fields[key];
  if (typeof value !== "boolean") throw new ContractError(`${key}: expected a boolean`);
  return value;
}

function str(fields: Fields, key: string): string {
  const value = fields[key];
  if (typeof value !== "string") throw new ContractError(`${key}: expected a string`);
  return value;
}

function oneOf<T extends string>(fields: Fields, key: string, allowed: readonly T[]): T {
  const value = str(fields, key);
  const found = allowed.find((item) => item === value);
  if (found === undefined) throw new ContractError(`${key}: unknown value "${value}"`);
  return found;
}

function indexed<T>(fields: Fields, key: string, allowed: readonly T[]): T {
  const found = allowed[uint(fields, key, 255)];
  if (found === undefined) throw new ContractError(`${key}: unknown value`);
  return found;
}

function readLed(value: unknown): LedState {
  const fields = asMap(value, "led");
  return {
    power: bool(fields, "power"),
    brightness_pct: uint(fields, "brightness_pct", 100),
    red_enabled: bool(fields, "red_enabled"),
    blue_enabled: bool(fields, "blue_enabled"),
    far_red_enabled: bool(fields, "far_red_enabled"),
    red_dist_pct: uint(fields, "red_dist_pct", 100),
    blue_dist_pct: uint(fields, "blue_dist_pct", 100),
    far_red_dist_pct: uint(fields, "far_red_dist_pct", 100),
  };
}

function readThresholds(value: unknown): Thresholds {
  const fields = asMap(value, "thresholds");
  return {
    temp_min_c: num(fields, "temp_min_c"),
    temp_max_c: num(fields, "temp_max_c"),
    humidity_min_pct: num(fields, "humidity_min_pct"),
    humidity_max_pct: num(fields, "humidity_max_pct"),
  };
}

export function readTelemetry(value: unknown): Telemetry {
  const fields = asMap(value, "telemetry");
  return {
    timestamp_ms: num(fields, "timestamp_ms"),
    light_lux: num(fields, "light_lux"),
    temperature_c: num(fields, "temperature_c"),
    humidity_pct: num(fields, "humidity_pct"),
    sequence: uint(fields, "sequence", 0xffffffff),
  };
}

export function readStatus(value: unknown): Status {
  const fields = asMap(value, "status");
  return {
    timestamp_ms: num(fields, "timestamp_ms"),
    mode: indexed(fields, "mode", DEVICE_MODES),
    degraded_tier: indexed(fields, "degraded_tier", DEGRADED_TIERS),
    uptime_sec: num(fields, "uptime_sec"),
    led: readLed(fields["led"]),
    auto_state: oneOf(fields, "auto_state", AUTO_STATES),
    cloud: oneOf(fields, "cloud", CLOUD_STATES),
    thresholds: readThresholds(fields["thresholds"]),
    lux_ceiling: num(fields, "lux_ceiling"),
  };
}

export function readEnergy(value: unknown): Energy {
  const fields = asMap(value, "energy");
  return {
    timestamp_ms: num(fields, "timestamp_ms"),
    total_wh: num(fields, "total_wh"),
    session_wh: num(fields, "session_wh"),
    light_on_sec: num(fields, "light_on_sec"),
    max_brightness_sec: num(fields, "max_brightness_sec"),
  };
}

export function readAck(value: unknown): Ack {
  const fields = asMap(value, "ack");
  return {
    command_id: str(fields, "command_id"),
    result: indexed(fields, "result", ACK_RESULTS),
    timestamp_ms: num(fields, "timestamp_ms"),
    mode: indexed(fields, "mode", DEVICE_MODES),
    reason: oneOf(fields, "reason", ACK_REASONS),
    led: readLed(fields["led"]),
  };
}

function decodeMsgPack(payload: Uint8Array): unknown {
  try {
    return decode(payload);
  } catch (error) {
    throw new ContractError(`bad msgpack: ${String(error)}`);
  }
}

export function decodeInbound(suffix: string, payload: Uint8Array): DecodeResult {
  try {
    return { ok: true, message: decodeInboundOrThrow(suffix, payload) };
  } catch (error) {
    if (error instanceof ContractError) return { ok: false, error: `${suffix}: ${error.message}` };
    throw error;
  }
}

function decodeInboundOrThrow(suffix: string, payload: Uint8Array): Inbound {
  if (suffix === TOPIC.availability) {
    const text = new TextDecoder().decode(payload);
    if (text !== "online" && text !== "offline") throw new ContractError(`unknown availability "${text}"`);
    return { kind: "availability", online: text === "online" };
  }
  if (suffix === TOPIC.telemetry) return { kind: "telemetry", value: readTelemetry(decodeMsgPack(payload)) };
  if (suffix === TOPIC.status) return { kind: "status", value: readStatus(decodeMsgPack(payload)) };
  if (suffix === TOPIC.energy) return { kind: "energy", value: readEnergy(decodeMsgPack(payload)) };
  if (suffix.startsWith(ACK_PREFIX)) {
    const ack = readAck(decodeMsgPack(payload));
    if (suffix !== `${ACK_PREFIX}${ack.command_id}`) throw new ContractError("ack topic does not match command_id");
    return { kind: "ack", value: ack };
  }
  throw new ContractError("unknown suffix");
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

export function sharesTotal(led: Pick<LedState, "red_dist_pct" | "blue_dist_pct" | "far_red_dist_pct">): number {
  return led.red_dist_pct + led.blue_dist_pct + led.far_red_dist_pct;
}

export function topicFor(deviceId: string, suffix: string): string {
  return `${TOPIC_ROOT}/${deviceId}/${suffix}`;
}

export function suffixFor(deviceId: string, topic: string): string | undefined {
  const prefix = `${TOPIC_ROOT}/${deviceId}/`;
  return topic.startsWith(prefix) ? topic.slice(prefix.length) : undefined;
}
