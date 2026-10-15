import { html, type TemplateResult } from "lit-html";
import { live } from "lit-html/directives/live.js";

import { CHANNEL_NAMES, CHANNELS, ENABLED_KEY, SHARE_KEY, type Channel, type Shares } from "../../core/actions.ts";
import type { LedState, Status } from "../../core/contract.ts";
import { feedbackLine, isPending, type Feedback } from "./feedback.ts";

export interface LightsHandlers {
  power(on: boolean): void;
  brightness(pct: number): void;
  channel(channel: Channel, enabled: boolean): void;
  shareInput(channel: Channel, pct: number): void;
  shareRelease(): void;
  switchToManual(): void;
}

export interface LightsProps {
  status: Status | undefined;
  shares: Shares;
  feedback: Feedback;
  announcement: string;
  on: LightsHandlers;
}

export function lightsView({ status, shares, feedback, announcement, on }: LightsProps): TemplateResult {
  if (status === undefined) {
    return html`<section class="panel lights" aria-labelledby="lights-title">
      <h2 class="panel__title" id="lights-title">Lights</h2>
      <p class="lights__note">Controls unlock when the node reports its state.</p>
    </section>`;
  }

  const locked = status.mode === "autonomous";
  const disabled = locked || isPending(feedback);

  return html`<section class="panel lights" aria-labelledby="lights-title">
    <h2 class="panel__title" id="lights-title">Lights</h2>
    ${locked
      ? html`<div class="lights__lock">
          <p class="lights__note">Auto mode controls the lights</p>
          <button type="button" @click=${on.switchToManual}>Switch to manual</button>
        </div>`
      : null}
    <fieldset class="lights__controls" aria-disabled=${locked ? "true" : "false"} aria-busy=${isPending(feedback) ? "true" : "false"}>
      <legend class="visually-hidden">Light controls</legend>
      ${powerSwitch(status.led, disabled, on)} ${brightnessSlider(status.led, disabled, on)}
      <fieldset class="lights__group">
        <legend class="lights__legend">Channels</legend>
        ${CHANNELS.map((channel) => channelSwitch(channel, status.led, disabled, on))}
      </fieldset>
      <fieldset class="lights__group">
        <legend class="lights__legend">Light mix</legend>
        ${CHANNELS.map((channel) => shareSlider(channel, shares, disabled, on))}
        <p class="visually-hidden" aria-live="polite">${announcement}</p>
      </fieldset>
    </fieldset>
    ${feedbackLine(feedback)}
  </section>`;
}

function powerSwitch(led: LedState, disabled: boolean, on: LightsHandlers): TemplateResult {
  return html`<label class="switch">
    <input
      class="switch__input"
      type="checkbox"
      role="switch"
      .checked=${live(led.power)}
      ?disabled=${disabled}
      @change=${(event: Event) => on.power(checkedOf(event))}
    />
    <span class="switch__label">Power</span>
  </label>`;
}

function brightnessSlider(led: LedState, disabled: boolean, on: LightsHandlers): TemplateResult {
  return html`<label class="slider">
    <span class="slider__label">Brightness <output class="slider__value">${led.brightness_pct}%</output></span>
    <input
      class="slider__input"
      type="range"
      min="0"
      max="100"
      step="1"
      .value=${live(String(led.brightness_pct))}
      aria-valuetext=${`${led.brightness_pct} percent`}
      ?disabled=${disabled}
      @change=${(event: Event) => on.brightness(valueOf(event))}
    />
  </label>`;
}

function channelSwitch(channel: Channel, led: LedState, disabled: boolean, on: LightsHandlers): TemplateResult {
  return html`<label class="switch" data-channel=${channel}>
    <input
      class="switch__input"
      type="checkbox"
      role="switch"
      .checked=${live(led[ENABLED_KEY[channel]])}
      ?disabled=${disabled}
      @change=${(event: Event) => on.channel(channel, checkedOf(event))}
    />
    <span class="switch__label">${CHANNEL_NAMES[channel]}</span>
  </label>`;
}

function shareSlider(channel: Channel, shares: Shares, disabled: boolean, on: LightsHandlers): TemplateResult {
  const pct = shares[SHARE_KEY[channel]];

  return html`<label class="slider" data-channel=${channel}>
    <span class="slider__label">${CHANNEL_NAMES[channel]} <output class="slider__value">${pct}%</output></span>
    <input
      class="slider__input"
      type="range"
      min="0"
      max="100"
      step="1"
      .value=${live(String(pct))}
      aria-valuetext=${`${CHANNEL_NAMES[channel]}, ${pct} percent of the mix`}
      ?disabled=${disabled}
      @input=${(event: Event) => on.shareInput(channel, valueOf(event))}
      @change=${on.shareRelease}
    />
  </label>`;
}

export function shareAnnouncement(moved: readonly Channel[], shares: Shares): string {
  return moved
    .map((channel, index) => {
      const name = index === 0 ? CHANNEL_NAMES[channel] : CHANNEL_NAMES[channel].toLowerCase();

      return `${name} ${shares[SHARE_KEY[channel]]}%`;
    })
    .join(", ");
}

function checkedOf(event: Event): boolean {
  return event.currentTarget instanceof HTMLInputElement && event.currentTarget.checked;
}

function valueOf(event: Event): number {
  return event.currentTarget instanceof HTMLInputElement ? Number(event.currentTarget.value) : Number.NaN;
}
