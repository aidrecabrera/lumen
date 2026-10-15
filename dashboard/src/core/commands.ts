import { commandSuffix, encodeCommand, type Ack, type AckReason, type Command, type LedState } from "./contract.ts";
import type { Transport } from "../transport/transport.ts";

export const ACK_TIMEOUT_MS = 10_000;

export type Outcome =
  | { kind: "applied"; ack: Ack }
  | { kind: "deferred"; ack: Ack }
  | { kind: "rejected"; ack: Ack }
  | { kind: "timeout" }
  | { kind: "not_sent"; reason: string };

export const ACK_MESSAGES: Record<AckReason, string> = {
  "mode applied": "Mode changed.",
  "config applied": "Settings saved.",
  "led applied": "Lights updated.",
  "not ready": "The node is still starting. Try again in a moment.",
  "persist failed": "The node could not save this change, so it kept the old setting.",
  "manual locked": "Auto mode controls the lights. Switch to manual first.",
  "invalid payload": "The node rejected these values.",
  "command queue full": "The node is busy. Try again.",
};

export function describeOutcome(outcome: Outcome): string {
  switch (outcome.kind) {
    case "applied":
    case "deferred":
    case "rejected":
      return ACK_MESSAGES[outcome.ack.reason];
    case "timeout":
      return "No answer from the node";
    case "not_sent":
      return `Not sent: ${outcome.reason}`;
  }
}

export class CommandSender {
  readonly #pending = new Map<string, { settle: (outcome: Outcome) => void; timer: ReturnType<typeof setTimeout> }>();
  readonly #transport: Pick<Transport, "sendCommand">;

  constructor(transport: Pick<Transport, "sendCommand">) {
    this.#transport = transport;
  }

  send(command: Command): Promise<Outcome> {
    const id = newCommandId();

    const outcome = new Promise<Outcome>((settle) => {
      const timer = setTimeout(() => this.#settle(id, { kind: "timeout" }), ACK_TIMEOUT_MS);
      this.#pending.set(id, { settle, timer });
    });

    try {
      this.#transport.sendCommand(commandSuffix(command), encodeCommand(id, command));
    } catch (error) {
      this.#settle(id, { kind: "not_sent", reason: error instanceof Error ? error.message : String(error) });
    }

    return outcome;
  }

  receiveAck(ack: Ack): void {
    this.#settle(ack.command_id, { kind: ack.result, ack });
  }

  #settle(id: string, outcome: Outcome): void {
    const pending = this.#pending.get(id);

    if (pending === undefined) {
      return;
    }

    this.#pending.delete(id);
    clearTimeout(pending.timer);
    pending.settle(outcome);
  }
}

export type Step = { label: string; command: (led: LedState) => Command };

export type SequenceResult =
  | { kind: "completed"; last: Outcome }
  | { kind: "stopped"; stepNumber: number; step: Step; outcome: Outcome };

// each step waits for its ack, and the next led step builds on the led state that ack reported.
export async function runSequence(sender: CommandSender, steps: readonly Step[], led: LedState): Promise<SequenceResult> {
  let currentLed = led;
  let last: Outcome = { kind: "not_sent", reason: "empty sequence" };

  for (const [index, step] of steps.entries()) {
    last = await sender.send(step.command(currentLed));

    if (last.kind !== "applied") {
      return { kind: "stopped", stepNumber: index + 1, step, outcome: last };
    }

    currentLed = last.ack.led;
  }

  return { kind: "completed", last };
}

// crypto.randomUUID needs a secure context, and the local dashboard is served over plain http.
export function newCommandId(): string {
  const bytes = crypto.getRandomValues(new Uint8Array(16));
  bytes[6] = ((bytes[6] ?? 0) & 0x0f) | 0x40;
  bytes[8] = ((bytes[8] ?? 0) & 0x3f) | 0x80;
  const hex = Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");

  return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
}
