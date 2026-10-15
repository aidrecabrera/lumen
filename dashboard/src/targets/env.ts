import * as v from "valibot";

const DeviceId = v.pipe(v.string(), v.regex(/^lumen-[0-9a-f]{12}$/, "VITE_DEVICE_ID must look like lumen-aabbccddeeff"));

const BrokerEnv = v.object({
  VITE_DEVICE_ID: DeviceId,
  VITE_BROKER_URL: v.pipe(v.string(), v.url()),
});

const DevBrokerEnv = v.object({
  ...BrokerEnv.entries,
  VITE_BROKER_USERNAME: v.optional(v.string()),
  VITE_BROKER_PASSWORD: v.optional(v.string()),
});

const CLOUD_TELEMETRY_PERIOD_MS = 30_000;

// three missed telemetry periods; the node sends telemetry every 30 s on the broker path.
export const BROKER_STALE_AFTER_MS = 3 * CLOUD_TELEMETRY_PERIOD_MS;

export type BrokerSettings = v.InferOutput<typeof BrokerEnv>;

export type DevBrokerSettings = v.InferOutput<typeof DevBrokerEnv>;

export function brokerSettings(): BrokerSettings {
  return v.parse(BrokerEnv, import.meta.env);
}

export function devBrokerSettings(): DevBrokerSettings {
  return v.parse(DevBrokerEnv, import.meta.env);
}
