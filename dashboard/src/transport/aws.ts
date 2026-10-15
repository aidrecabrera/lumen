// the package root resolves to the node build for typescript; the browser build has its own api.
import { auth, iot, mqtt5 } from "aws-iot-device-sdk-v2/dist/browser";

import { SUBSCRIBED_SUFFIXES, suffixFor, topicFor, type CommandSuffix } from "../core/contract.ts";
import { newCommandId } from "../core/commands.ts";
import type { Link, MessageHandler, Transport } from "./transport.ts";

export interface AwsIotSettings {
  endpoint: string;
  region: string;
  deviceId: string;
  // the browser iot policy allows client ids of the form <identity id>-*.
  identityId: string;
  credentials: auth.CredentialsProvider;
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
    const { endpoint, region, identityId, credentials } = this.#settings;

    await credentials.refreshCredentials();

    const builder = iot.AwsIotMqtt5ClientConfigBuilder.newWebsocketMqttBuilderWithSigv4Auth(endpoint, {
      credentialsProvider: credentials,
      region,
    });

    builder.withConnectProperties({ clientId: `${identityId}-${newCommandId()}`, keepAliveIntervalSeconds: 60 });

    const client = new mqtt5.Mqtt5Client(builder.build());

    const settled = new Promise<void>((resolve) => {
      client.once("connectionSuccess", () => resolve());
      client.once("connectionFailure", () => resolve());
    });

    this.#client = client;
    this.#emitLink({ kind: "connecting" });

    client.on("connectionSuccess", () => void this.#subscribe(client));
    client.on("connectionFailure", () => this.#emitLink({ kind: "reconnecting" }));
    // iot core closes websocket connections after 24 h; the client reconnects with fresh credentials.
    client.on("disconnection", () => {
      this.#connected = false;
      this.#emitLink({ kind: "reconnecting" });
      credentials.refreshCredentials().catch(() => {
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

    void client.publish({ topicName: topicFor(this.#settings.deviceId, suffix), payload, qos: mqtt5.QoS.AtLeastOnce });
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

    if (suffix === undefined || !(payload instanceof ArrayBuffer)) return;

    for (const handler of this.#messageHandlers) handler(suffix, new Uint8Array(payload), message.retain ?? false);
  }

  #emitLink(link: Link): void {
    for (const handler of this.#linkHandlers) handler(link);
  }
}
