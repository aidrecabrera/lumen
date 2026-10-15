import type { Energy, Inbound, Status, Telemetry } from "./contract.ts";
import type { Link } from "../transport/transport.ts";

export const HISTORY_LIMIT = 1800;

export type Known<T> = { kind: "none" } | { kind: "known"; value: T; receivedAt: number; lastKnown: boolean };

export type Availability =
  | { kind: "unknown" }
  | { kind: "online" }
  | { kind: "offline"; since: number }
  | { kind: "offline_before_page" };

export interface HistoryPoint {
  at: number;
  lux: number;
  temperature: number;
  humidity: number;
}

export interface DashboardState {
  link: Link;
  availability: Availability;
  status: Known<Status>;
  telemetry: Known<Telemetry>;
  energy: Known<Energy>;
  history: readonly HistoryPoint[];
  lastMessageAt: number | undefined;
}

export type DashboardEvent =
  | { type: "link"; link: Link }
  | { type: "message"; message: Inbound; retained: boolean; at: number };

export const initialState: DashboardState = {
  link: { kind: "connecting" },
  availability: { kind: "unknown" },
  status: { kind: "none" },
  telemetry: { kind: "none" },
  energy: { kind: "none" },
  history: [],
  lastMessageAt: undefined,
};

export function reduce(state: DashboardState, event: DashboardEvent): DashboardState {
  if (event.type === "link") return { ...state, link: event.link };

  const { message, retained, at } = event;
  const seen = retained ? state : { ...state, lastMessageAt: at };

  switch (message.kind) {
    case "availability":
      return { ...seen, availability: availabilityFrom(message.online, retained, at) };
    case "telemetry":
      return {
        ...seen,
        telemetry: known(message.value, retained, at),
        history: retained ? seen.history : appendHistory(seen.history, message.value, at),
      };
    case "status":
      return { ...seen, status: known(message.value, retained, at) };
    case "energy":
      return { ...seen, energy: known(message.value, retained, at) };
    case "ack":
      if (seen.status.kind === "none") return seen;

      return {
        ...seen,
        status: { ...seen.status, value: { ...seen.status.value, mode: message.value.mode, led: message.value.led } },
      };
  }
}

function known<T>(value: T, retained: boolean, at: number): Known<T> {
  return { kind: "known", value, receivedAt: at, lastKnown: retained };
}

// a retained "offline" has no timestamp, so the tab cannot know when the node went down.
function availabilityFrom(online: boolean, retained: boolean, at: number): Availability {
  if (online) return { kind: "online" };

  return retained ? { kind: "offline_before_page" } : { kind: "offline", since: at };
}

function appendHistory(history: readonly HistoryPoint[], telemetry: Telemetry, at: number): readonly HistoryPoint[] {
  const point = { at, lux: telemetry.light_lux, temperature: telemetry.temperature_c, humidity: telemetry.humidity_pct };

  return [...history.slice(-(HISTORY_LIMIT - 1)), point];
}

export type Banner =
  | { tone: "live"; text: "Live" }
  | { tone: "busy"; text: string }
  | { tone: "offline"; text: string }
  | { tone: "error"; text: string };

export function describeConnection(state: DashboardState, now: number, staleAfterMs: number): Banner {
  switch (state.link.kind) {
    case "connecting":
      return { tone: "busy", text: "Connecting…" };
    case "reconnecting":
      return { tone: "busy", text: "Reconnecting…" };
    case "session_expired":
      return { tone: "error", text: "Session expired: sign in again" };
    case "failed":
      return { tone: "error", text: state.link.reason };
    case "connected":
      break;
  }

  if (state.availability.kind === "offline") {
    return { tone: "offline", text: `Node offline since ${clockTime(state.availability.since)}` };
  }

  if (state.availability.kind === "offline_before_page") {
    return { tone: "offline", text: "Node offline (last seen before this page opened)" };
  }

  if (state.lastMessageAt === undefined) return { tone: "busy", text: "Waiting for the node…" };

  if (now - state.lastMessageAt > staleAfterMs) return { tone: "offline", text: "Node not responding" };

  return { tone: "live", text: "Live" };
}

export function isStale<T>(reading: Known<T>, now: number, staleAfterMs: number): boolean {
  return reading.kind === "known" && (reading.lastKnown || now - reading.receivedAt > staleAfterMs);
}

export function clockTime(at: number): string {
  return new Date(at).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

export function ageText(at: number, now: number): string {
  const seconds = Math.max(0, Math.round((now - at) / 1000));

  if (seconds < 60) return `Updated ${seconds} s ago`;
  const minutes = Math.round(seconds / 60);

  if (minutes < 60) return `Updated ${minutes} min ago`;

  return `Updated at ${clockTime(at)}`;
}
