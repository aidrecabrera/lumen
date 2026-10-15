import { decodeCommand, type AddressedCommand, type CommandSuffix } from "../core/contract.ts";
import type { Link, MessageHandler, Transport } from "./transport.ts";

export interface SentCommand extends AddressedCommand {
  suffix: CommandSuffix;
}

// an in-memory transport: tests and the prototype play the node through deliver() and setLink().
export class FakeTransport implements Transport {
  readonly sent: SentCommand[] = [];
  #messageHandlers: MessageHandler[] = [];
  #linkHandlers: ((link: Link) => void)[] = [];
  #commandHandlers: ((command: SentCommand) => void)[] = [];
  #connected = false;

  async connect(): Promise<void> {
    this.setLink({ kind: "connected" });
  }

  disconnect(): void {
    this.setLink({ kind: "connecting" });
  }

  onMessage(handler: MessageHandler): void {
    this.#messageHandlers.push(handler);
  }

  onConnectionChange(handler: (link: Link) => void): void {
    this.#linkHandlers.push(handler);
  }

  sendCommand(suffix: CommandSuffix, payload: Uint8Array): void {
    if (!this.#connected) {
      throw new Error("not connected");
    }

    const decoded = decodeCommand(suffix, payload);

    if (decoded === undefined) {
      throw new Error(`fake transport got an invalid ${suffix} payload`);
    }

    const sent = { suffix, ...decoded };

    this.sent.push(sent);

    for (const handler of this.#commandHandlers) {
      handler(sent);
    }
  }

  onCommand(handler: (command: SentCommand) => void): void {
    this.#commandHandlers.push(handler);
  }

  deliver(suffix: string, payload: Uint8Array, retained = false): void {
    for (const handler of this.#messageHandlers) {
      handler(suffix, payload, retained);
    }
  }

  setLink(link: Link): void {
    this.#connected = link.kind === "connected";

    for (const handler of this.#linkHandlers) {
      handler(link);
    }
  }

  lastCommand(): SentCommand {
    const last = this.sent.at(-1);

    if (last === undefined) {
      throw new Error("no command sent");
    }

    return last;
  }
}
