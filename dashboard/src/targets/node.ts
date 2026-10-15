import { nothing } from "lit-html";

import { LocalTransport } from "../transport/local.ts";
import type { Target } from "./types.ts";

// three missed telemetry periods; the node sends telemetry every 2 s on the local path.
const LOCAL_STALE_AFTER_MS = 6000;

export async function createTarget(): Promise<Target> {
  const scheme = location.protocol === "https:" ? "wss" : "ws";

  return {
    transport: new LocalTransport(`${scheme}://${location.host}/ws`),
    staleAfterMs: LOCAL_STALE_AFTER_MS,
    extras: () => nothing,
  };
}
