import { nothing } from "lit-html";

import { MqttTransport } from "../transport/mqtt.ts";
import { askForCredentials } from "../ui/components/login.ts";
import { BROKER_STALE_AFTER_MS, brokerSettings } from "./env.ts";
import type { Target } from "./types.ts";

export async function createTarget(root: HTMLElement): Promise<Target> {
  const env = brokerSettings();
  const credentials = await askForCredentials(root, "Sign in to Lumen");

  return {
    transport: new MqttTransport({ url: env.VITE_BROKER_URL, deviceId: env.VITE_DEVICE_ID, credentials }),
    staleAfterMs: BROKER_STALE_AFTER_MS,
    extras: () => nothing,
  };
}
