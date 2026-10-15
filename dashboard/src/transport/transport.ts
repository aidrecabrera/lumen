import type { CommandSuffix } from "../core/contract.ts";

export type Link =
  | { kind: "connecting" }
  | { kind: "connected" }
  | { kind: "reconnecting" }
  | { kind: "session_expired" }
  | { kind: "failed"; reason: string };

export type MessageHandler = (suffix: string, payload: Uint8Array, retained: boolean) => void;

export interface Transport {
  connect(): Promise<void>;
  disconnect(): void;
  onMessage(handler: MessageHandler): void;
  onConnectionChange(handler: (link: Link) => void): void;
  sendCommand(suffix: CommandSuffix, payload: Uint8Array): void;
}
