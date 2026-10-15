// the package root resolves to the node build for typescript; the browser build has its own api.
import { auth, iot, mqtt5 } from "aws-iot-device-sdk-v2/dist/browser";

import { SUBSCRIBED_SUFFIXES, suffixFor, topicFor, type CommandSuffix } from "../core/contract.ts";
import { newCommandId } from "../core/commands.ts";
import type { Link, MessageHandler, Transport } from "./transport.ts";

export interface AwsIotSettings {
  deviceId: string;
  // the browser iot policy allows client ids of the form <identity id>-*.
  identityId: string;
  clientConfig(clientId: string): mqtt5.Mqtt5ClientConfig;
  // rejects once the login can no longer be refreshed.
  refreshCredentials(): Promise<void>;
}

export function sigv4ClientConfig(
  endpoint: string,
  region: string,
  credentials: auth.CredentialsProvider,
  clientId: string,
): mqtt5.Mqtt5ClientConfig {
  const builder = iot.AwsIotMqtt5ClientConfigBuilder.newWebsocketMqttBuilderWithSigv4Auth(endpoint, {
    credentialsProvider: credentials,
    region,
  });

  builder.withConnectProperties({ clientId, keepAliveIntervalSeconds: 60 });

  return builder.build();
}

export class AwsTransport implements Transport {
  readonly #settings: AwsIotSettings;
  #client: mqtt5.Mqtt5Client | undefined;
  #connected = false;
  #messageHandlers: MessageHandler[] = [];
  #linkHandlers: ((link: Link) => void)[] = [];

  constructor(settings: AwsIotSettings) {
    this.#settings = settings;
  }

  async connect(): Promise<void> {
    const { identityId, clientConfig, refreshCredentials } = this.#settings;

    await refreshCredentials();

    const client = new mqtt5.Mqtt5Client(clientConfig(`${identityId}-${newCommandId()}`));

    // settles once subscribed, so a caller never sends before the node topics are live
    const settled = new Promise<void>((resolve) => {
      this.onConnectionChange((link) => {
        if (link.kind !== "connecting") resolve();
      });
    });

    this.#client = client;
    this.#emitLink({ kind: "connecting" });

    client.on("connectionSuccess", () => {
      this.#subscribe(client).catch(() => this.#emitLink({ kind: "reconnecting" }));
    });
    client.on("connectionFailure", () => this.#emitLink({ kind: "reconnecting" }));
    // iot core closes websocket connections after 24 h; the client reconnects with fresh credentials.
    client.on("disconnection", () => {
      this.#connected = false;
      this.#emitLink({ kind: "reconnecting" });
      refreshCredentials().catch(() => {
        client.stop();
        this.#emitLink({ kind: "session_expired" });
      });
    });
    client.on("messageReceived", ({ message }) => this.#receive(message));
    client.start();

    await settled;
  }

  disconnect(): void {
    this.#client?.stop();
    this.#client = undefined;
    this.#connected = false;
  }

  onMessage(handler: MessageHandler): void {
    this.#messageHandlers.push(handler);
  }

  onConnectionChange(handler: (link: Link) => void): void {
    this.#linkHandlers.push(handler);
  }

  sendCommand(suffix: CommandSuffix, payload: Uint8Array): void {
    const client = this.#client;

    if (client === undefined || !this.#connected) throw new Error("not connected to aws iot");

    // the sdk sends a Uint8Array's whole backing buffer and ignores the view bounds, so hand it exact bytes
    const exact = payload.slice().buffer;

    void client.publish({ topicName: topicFor(this.#settings.deviceId, suffix), payload: exact, qos: mqtt5.QoS.AtLeastOnce });
  }

  // a retained message reaches only exact-topic subscriptions on aws iot, so no wildcard here.
  async #subscribe(client: mqtt5.Mqtt5Client): Promise<void> {
    const subscriptions = SUBSCRIBED_SUFFIXES.map((suffix) => ({
      topicFilter: topicFor(this.#settings.deviceId, suffix),
      qos: mqtt5.QoS.AtLeastOnce,
    }));

    await client.subscribe({ subscriptions });
    this.#connected = true;
    this.#emitLink({ kind: "connected" });
  }

  #receive(message: mqtt5.PublishPacket): void {
    const suffix = suffixFor(this.#settings.deviceId, message.topicName);
    const payload = message.payload;

    // the sdk decodes payloads as Buffer views over the received frame
    if (suffix === undefined || !ArrayBuffer.isView(payload)) return;

    const bytes = new Uint8Array(payload.buffer, payload.byteOffset, payload.byteLength);

    for (const handler of this.#messageHandlers) handler(suffix, bytes, message.retain ?? false);
  }

  #emitLink(link: Link): void {
    for (const handler of this.#linkHandlers) handler(link);
  }
}
