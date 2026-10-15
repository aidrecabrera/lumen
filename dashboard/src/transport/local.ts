import { TOPIC, type CommandSuffix } from "../core/contract.ts";
import { decodeFrame, encodeFrame } from "./frame.ts";
import type { Link, MessageHandler, Transport } from "./transport.ts";

const RECONNECT_DELAYS_MS = [1000, 2000, 5000] as const;
const ONLINE = new TextEncoder().encode("online");

// the node itself serves this socket, so an open socket means the node is up; a powered-off
// node that leaves the socket open shows as "not responding" through the stale check.
export class LocalTransport implements Transport {
  readonly #url: string;
  #socket: WebSocket | undefined;
  #attempt = 0;
  #stopped = false;
  #messageHandlers: MessageHandler[] = [];
  #linkHandlers: ((link: Link) => void)[] = [];

  constructor(url: string) {
    this.#url = url;
  }

  connect(): Promise<void> {
    this.#stopped = false;
    this.#emitLink({ kind: "connecting" });

    return new Promise((resolve) => this.#open(resolve));
  }

  disconnect(): void {
    this.#stopped = true;
    this.#socket?.close();
    this.#socket = undefined;
  }

  onMessage(handler: MessageHandler): void {
    this.#messageHandlers.push(handler);
  }

  onConnectionChange(handler: (link: Link) => void): void {
    this.#linkHandlers.push(handler);
  }

  sendCommand(suffix: CommandSuffix, payload: Uint8Array): void {
    const socket = this.#socket;

    if (socket === undefined || socket.readyState !== WebSocket.OPEN) throw new Error("not connected to the node");

    socket.send(encodeFrame(suffix, payload).slice());
  }

  #open(onFirstResult: () => void): void {
    const socket = new WebSocket(this.#url);

    socket.binaryType = "arraybuffer";
    socket.addEventListener("open", () => {
      this.#attempt = 0;
      this.#emitLink({ kind: "connected" });
      this.#deliver(TOPIC.availability, ONLINE);
      onFirstResult();
    });
    socket.addEventListener("message", (event) => this.#receive(event));
    socket.addEventListener("close", () => {
      onFirstResult();

      if (this.#stopped) return;

      this.#emitLink({ kind: "reconnecting" });
      setTimeout(() => this.#open(() => undefined), this.#nextDelay());
    });
    this.#socket = socket;
  }

  #receive(event: MessageEvent): void {
    if (!(event.data instanceof ArrayBuffer)) return;

    const frame = decodeFrame(new Uint8Array(event.data));

    if (frame !== undefined) this.#deliver(frame.t, frame.p);
  }

  #deliver(suffix: string, payload: Uint8Array): void {
    for (const handler of this.#messageHandlers) handler(suffix, payload, false);
  }

  #nextDelay(): number {
    const delay = RECONNECT_DELAYS_MS[Math.min(this.#attempt, RECONNECT_DELAYS_MS.length - 1)] ?? 5000;

    this.#attempt += 1;

    return delay;
  }

  #emitLink(link: Link): void {
    for (const handler of this.#linkHandlers) handler(link);
  }
}
