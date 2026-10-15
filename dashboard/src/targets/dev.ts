import { nothing } from "lit-html";

import { MqttTransport } from "../transport/mqtt.ts";
import { BROKER_STALE_AFTER_MS, devBrokerSettings } from "./env.ts";
import type { Target } from "./types.ts";

export async function createTarget(): Promise<Target> {
  const env = devBrokerSettings();
  const username = env.VITE_BROKER_USERNAME;
  const password = env.VITE_BROKER_PASSWORD ?? "";

  return {
    transport: new MqttTransport({
      url: env.VITE_BROKER_URL,
      deviceId: env.VITE_DEVICE_ID,
      credentials: username === undefined ? undefined : { username, password },
    }),
    staleAfterMs: BROKER_STALE_AFTER_MS,
    extras: () => nothing,
  };
}
