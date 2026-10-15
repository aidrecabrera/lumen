import { html, type TemplateResult } from "lit-html";
import { live } from "lit-html/directives/live.js";

import type { SettingsField, SettingsForm } from "../../core/actions.ts";
import { LUX_CEILING_MAX, LUX_CEILING_MIN } from "../../core/contract.ts";
import { feedbackLine, isPending, type Feedback } from "./feedback.ts";

export interface SettingsProps {
  form: SettingsForm | undefined;
  errors: Partial<Record<SettingsField, string>>;
  currentLux: number | undefined;
  feedback: Feedback;
  onInput: (field: SettingsField, value: string) => void;
  onSave: () => void;
}

export function settingsView({ form, errors, currentLux, feedback, onInput, onSave }: SettingsProps): TemplateResult {
  if (form === undefined) {
    return html`<section class="panel settings" aria-labelledby="settings-title">
      <h2 class="panel__title" id="settings-title">Auto mode settings</h2>
      <p class="settings__note">Settings unlock when the node reports its state.</p>
    </section>`;
  }

  const field = (name: SettingsField, label: string, unit: string, limits: { min?: number; max?: number } = {}): TemplateResult =>
    html`<label class="settings__field">
      <span class="settings__label">${label}</span>
      <span class="settings__input-row">
        <input
          class="settings__input"
          type="number"
          inputmode="decimal"
          step="any"
          min=${limits.min ?? ""}
          max=${limits.max ?? ""}
          .value=${live(form[name])}
          aria-invalid=${errors[name] === undefined ? "false" : "true"}
          aria-describedby=${`${name}-error`}
          @input=${(event: Event) => onInput(name, event.currentTarget instanceof HTMLInputElement ? event.currentTarget.value : "")}
        />
        <span class="settings__unit">${unit}</span>
      </span>
      <span class="settings__error" id=${`${name}-error`}>${errors[name] ?? ""}</span>
    </label>`;

  return html`<section class="panel settings" aria-labelledby="settings-title">
    <h2 class="panel__title" id="settings-title">Auto mode settings</h2>
    <form
      class="settings__form"
      aria-busy=${isPending(feedback) ? "true" : "false"}
      @submit=${(event: Event) => {
        event.preventDefault();
        onSave();
      }}
    >
      <fieldset class="settings__group">
        <legend class="settings__legend">Auto mode turns the lights off when daylight reaches</legend>
        ${field("lux_ceiling", "Daylight", "lux", { min: LUX_CEILING_MIN, max: LUX_CEILING_MAX })}
        <p class="settings__hint">Daylight now: ${currentLux === undefined ? "unknown" : `${Math.round(currentLux).toLocaleString()} lux`}</p>
      </fieldset>
      <fieldset class="settings__group">
        <legend class="settings__legend">Auto mode pauses when temperature or humidity leaves this range.</legend>
        ${field("temp_min_c", "Lowest temperature", "°C")} ${field("temp_max_c", "Highest temperature", "°C")}
        ${field("humidity_min_pct", "Lowest humidity", "%")} ${field("humidity_max_pct", "Highest humidity", "%")}
      </fieldset>
      <button type="submit" data-variant="primary" ?disabled=${isPending(feedback)}>Save settings</button>
    </form>
    ${feedbackLine(feedback)}
  </section>`;
}
