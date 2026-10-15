import mqtt, { type MqttClient } from "mqtt";

import { SUBSCRIBED_SUFFIXES, suffixFor, topicFor, type CommandSuffix } from "../core/contract.ts";
import { newCommandId } from "../core/commands.ts";
import type { Link, MessageHandler, Transport } from "./transport.ts";

export interface MqttSettings {
  url: string;
  deviceId: string;
  credentials: { username: string; password: string } | undefined;
}

// mqtt 3.1.1 connack codes 4 and 5: bad credentials and not authorized.
const AUTH_FAILURE_CODES = new Set([4, 5]);

export class MqttTransport implements Transport {
  readonly #settings: MqttSettings;
  #client: MqttClient | undefined;
  #messageHandlers: MessageHandler[] = [];
  #linkHandlers: ((link: Link) => void)[] = [];

  constructor(settings: MqttSettings) {
    this.#settings = settings;
  }

  async connect(): Promise<void> {
    const { url, deviceId, credentials } = this.#settings;

    const client = mqtt.connect(url, {
      clientId: `lumen-dashboard-${newCommandId()}`,
      ...credentials,
      reconnectPeriod: 2000,
      connectTimeout: 10_000,
    });

    this.#client = client;
    this.#emitLink({ kind: "connecting" });

    const settled = new Promise<void>((resolve) => {
      this.onConnectionChange((link) => {
        if (link.kind === "connected" || link.kind === "failed") {
          resolve();
        }
      });
    });

    client.on("connect", () => void this.#subscribe(client));
    client.on("reconnect", () => this.#emitLink({ kind: "reconnecting" }));
    client.on("offline", () => this.#emitLink({ kind: "reconnecting" }));
    client.on("error", (error) => {
      if (!("code" in error) || !AUTH_FAILURE_CODES.has(Number(error.code))) {
        return;
      }

      client.end(true);
      this.#emitLink({ kind: "failed", reason: "The broker refused the username or password." });
    });
    client.on("message", (topic, payload, packet) => {
      const suffix = suffixFor(deviceId, topic);

      if (suffix === undefined) {
        return;
      }

      for (const handler of this.#messageHandlers) {
        handler(suffix, payload, packet.retain);
      }
    });

    await settled;
  }

  disconnect(): void {
    this.#client?.end(true);
    this.#client = undefined;
  }

  onMessage(handler: MessageHandler): void {
    this.#messageHandlers.push(handler);
  }

  onConnectionChange(handler: (link: Link) => void): void {
    this.#linkHandlers.push(handler);
  }

  sendCommand(suffix: CommandSuffix, payload: Uint8Array): void {
    const client = this.#client;

    if (client === undefined || !client.connected) {
      throw new Error("not connected to the broker");
    }

    // SAFETY: mqtt-packet sizes the payload with Buffer.byteLength and sends it with stream.write,
    // which both take any Uint8Array; browsers have no global Buffer to convert to.
    client.publish(topicFor(this.#settings.deviceId, suffix), payload as Buffer, { qos: 1 });
  }

  // a retained message reaches only exact-topic subscriptions on aws iot, so no wildcard here.
  async #subscribe(client: MqttClient): Promise<void> {
    const topics = SUBSCRIBED_SUFFIXES.map((suffix) => topicFor(this.#settings.deviceId, suffix));

    await client.subscribeAsync(topics, { qos: 1 });
    this.#emitLink({ kind: "connected" });
  }

  #emitLink(link: Link): void {
    for (const handler of this.#linkHandlers) {
      handler(link);
    }
  }
}
