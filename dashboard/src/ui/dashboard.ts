import { html, render } from "lit-html";

import {
  CHANNELS,
  ENABLED_KEY,
  ledCommand,
  modeCommand,
  parseSettings,
  rebalanceShares,
  settingsForm,
  switchToAutoSteps,
  type Channel,
  type Preset,
  type SettingsField,
  type SettingsForm,
  type Shares,
} from "../core/actions.ts";
import { CommandSender, describeOutcome, runSequence, type SequenceResult, type Step } from "../core/commands.ts";
import { decodeInbound, type Command, type DeviceMode, type LedState, type Status } from "../core/contract.ts";
import { describeConnection, historyCsv, initialState, reduce, type DashboardEvent } from "../core/state.ts";
import type { Target } from "../targets/types.ts";
import { bannerView } from "./components/banner.ts";
import { energyView } from "./components/energy.ts";
import { IDLE, type Feedback } from "./components/feedback.ts";
import { historyView } from "./components/history.ts";
import { lightsView, shareAnnouncement } from "./components/lights.ts";
import { modeView } from "./components/mode.ts";
import { presetsView } from "./components/presets.ts";
import { readingsView } from "./components/readings.ts";
import { settingsView } from "./components/settings.ts";

type Panel = "lights" | "mode" | "settings" | "presets";

const MODE_NAMES: Record<DeviceMode, string> = { autonomous: "auto", manual: "manual" };

export class Dashboard {
  readonly #target: Target;
  readonly #root: HTMLElement;
  readonly #sender: CommandSender;
  #state = initialState;
  #feedback: Record<Panel, Feedback> = { lights: IDLE, mode: IDLE, settings: IDLE, presets: IDLE };
  #shareDraft: { shares: Shares; moved: Channel } | undefined;
  #announcement = "";
  #settings: { form: SettingsForm; dirty: boolean } | undefined;
  #settingsErrors: Partial<Record<SettingsField, string>> = {};
  #offerBackToAuto = false;

  constructor(target: Target, root: HTMLElement) {
    this.#target = target;
    this.#root = root;
    this.#sender = new CommandSender(target.transport);
  }

  async start(): Promise<void> {
    const { transport } = this.#target;

    transport.onConnectionChange((link) => this.#apply({ type: "link", link }));
    transport.onMessage((suffix, payload, retained) => this.#receive(suffix, payload, retained));
    setInterval(() => this.#render(), 1000);
    this.#render();
    await transport.connect();
  }

  #receive(suffix: string, payload: Uint8Array, retained: boolean): void {
    const decoded = decodeInbound(suffix, payload);

    if (!decoded.ok) {
      console.warn(`ignored message: ${decoded.error}`);

      return;
    }

    this.#apply({ type: "message", message: decoded.message, retained, at: Date.now() });

    if (decoded.message.kind === "ack") this.#sender.receiveAck(decoded.message.value);
  }

  #apply(event: DashboardEvent): void {
    this.#state = reduce(this.#state, event);

    const status = this.#status();

    if (status !== undefined && this.#settings?.dirty !== true) {
      this.#settings = { form: settingsForm(status.thresholds, status.lux_ceiling), dirty: false };
    }

    this.#render();
  }

  #status(): Status | undefined {
    return this.#state.status.kind === "known" ? this.#state.status.value : undefined;
  }

  #led(): LedState {
    const status = this.#status();

    if (status === undefined) throw new Error("led controls are disabled until the first status");

    return status.led;
  }

  async #send(panel: Panel, command: Command): Promise<void> {
    this.#setFeedback(panel, { kind: "pending", text: "Sending…" });

    const outcome = await this.#sender.send(command);

    this.#setFeedback(panel, { kind: "done", ok: outcome.kind === "applied", text: describeOutcome(outcome) });
  }

  async #runSteps(panel: Panel, steps: readonly Step[], pendingText: string): Promise<SequenceResult> {
    this.#setFeedback(panel, { kind: "pending", text: pendingText });

    const result = await runSequence(this.#sender, steps, this.#led());

    this.#setFeedback(panel, this.#sequenceFeedback(result));

    return result;
  }

  #sequenceFeedback(result: SequenceResult): Feedback {
    if (result.kind === "stopped") {
      const mode = this.#status()?.mode;
      const where = mode === undefined ? "" : ` The node is in ${MODE_NAMES[mode]} mode.`;

      return { kind: "done", ok: false, text: `Stopped at step ${result.stepNumber} (${result.step.label}): ${describeOutcome(result.outcome)}${where}` };
    }

    if (this.#status()?.auto_state === "night_off") {
      return { kind: "done", ok: true, text: "Applied. Auto mode keeps the lights off until daylight." };
    }

    return { kind: "done", ok: true, text: "Applied." };
  }

  #setFeedback(panel: Panel, feedback: Feedback): void {
    this.#feedback = { ...this.#feedback, [panel]: feedback };
    this.#render();
  }

  #changeLed(change: Partial<LedState>): void {
    void this.#send("lights", ledCommand(this.#led(), change));
  }

  #previewShares(channel: Channel, pct: number): void {
    const current = this.#shareDraft?.shares ?? this.#led();

    this.#shareDraft = { shares: rebalanceShares(current, channel, pct), moved: channel };
    this.#render();
  }

  async #commitShares(): Promise<void> {
    const draft = this.#shareDraft;

    if (draft === undefined) return;

    const others = CHANNELS.filter((channel) => channel !== draft.moved);

    this.#announcement = shareAnnouncement(others, draft.shares);
    await this.#send("lights", ledCommand(this.#led(), draft.shares));
    this.#shareDraft = undefined;
    this.#render();
  }

  #changeMode(mode: DeviceMode): void {
    if (mode === "manual") {
      void this.#send("mode", modeCommand("manual"));

      return;
    }

    void this.#runSteps("mode", switchToAutoSteps(this.#led()), "Switching to auto…");
  }

  #editSettings(field: SettingsField, value: string): void {
    if (this.#settings === undefined) return;

    this.#settings = { form: { ...this.#settings.form, [field]: value }, dirty: true };
    this.#render();
  }

  async #saveSettings(): Promise<void> {
    if (this.#settings === undefined) return;

    const parsed = parseSettings(this.#settings.form);

    this.#settingsErrors = parsed.ok ? {} : parsed.errors;

    if (!parsed.ok) {
      this.#render();

      return;
    }

    await this.#send("settings", parsed.command);
    this.#settings = { ...this.#settings, dirty: false };
  }

  async #runPreset(preset: Preset): Promise<void> {
    this.#offerBackToAuto = false;

    const result = await this.#runSteps("presets", preset.steps, "Applying preset…");

    this.#offerBackToAuto = result.kind === "stopped" && this.#status()?.mode === "manual";
    this.#render();
  }

  #exportCsv(): void {
    const blob = new Blob([historyCsv(this.#state.history)], { type: "text/csv" });
    const link = document.createElement("a");

    link.href = URL.createObjectURL(blob);
    link.download = `lumen-readings-${new Date().toISOString().slice(0, 19).replace(/:/g, "-")}.csv`;
    link.click();
    URL.revokeObjectURL(link.href);
  }

  #render(): void {
    const now = Date.now();
    const state = this.#state;
    const status = this.#status();
    const { staleAfterMs } = this.#target;

    render(
      html`<header class="app__header">
          <h1 class="app__title">Lumen</h1>
          ${bannerView(describeConnection(state, now, staleAfterMs))}
        </header>
        ${readingsView(state.telemetry, now, staleAfterMs)}
        <div class="app__grid">
          ${modeView({ status, feedback: this.#feedback.mode, onChange: (mode) => this.#changeMode(mode) })}
          ${lightsView({
            status,
            shares: this.#shareDraft?.shares ?? status?.led ?? { red_dist_pct: 0, blue_dist_pct: 0, far_red_dist_pct: 0 },
            feedback: this.#feedback.lights,
            announcement: this.#announcement,
            on: {
              power: (on) => this.#changeLed({ power: on }),
              brightness: (pct) => this.#changeLed({ brightness_pct: pct }),
              channel: (channel, enabled) => this.#changeLed({ [ENABLED_KEY[channel]]: enabled }),
              shareInput: (channel, pct) => this.#previewShares(channel, pct),
              shareRelease: () => void this.#commitShares(),
              switchToManual: () => this.#changeMode("manual"),
            },
          })}
          ${presetsView({
            enabled: status !== undefined,
            feedback: this.#feedback.presets,
            offerBackToAuto: this.#offerBackToAuto,
            onRun: (preset) => void this.#runPreset(preset),
            onBackToAuto: () => this.#changeMode("autonomous"),
          })}
          ${settingsView({
            form: this.#settings?.form,
            errors: this.#settingsErrors,
            currentLux: state.telemetry.kind === "known" ? state.telemetry.value.light_lux : undefined,
            feedback: this.#feedback.settings,
            onInput: (field, value) => this.#editSettings(field, value),
            onSave: () => void this.#saveSettings(),
          })}
          ${energyView(state.energy, status)}
        </div>
        ${historyView({ history: state.history, onExport: () => this.#exportCsv() })} ${this.#target.extras()}`,
      this.#root,
    );
  }
}
