import { html, type TemplateResult } from "lit-html";
import { live } from "lit-html/directives/live.js";

import type { AutoState, DeviceMode, Status } from "../../core/contract.ts";
import { feedbackLine, isPending, type Feedback } from "./feedback.ts";

export interface ModeProps {
  status: Status | undefined;
  feedback: Feedback;
  onChange: (mode: DeviceMode) => void;
}

const AUTO_STATE_TEXT: Record<AutoState, string> = {
  active: "Auto mode sets the brightness from the daylight.",
  night_off: "Auto mode keeps the lights off until daylight.",
  paused_out_of_range: "Auto mode is paused: temperature or humidity is outside your range.",
};

export function modeView({ status, feedback, onChange }: ModeProps): TemplateResult {
  const disabled = status === undefined || isPending(feedback);

  return html`<section class="panel mode" aria-labelledby="mode-title">
    <h2 class="panel__title" id="mode-title">Mode</h2>
    <fieldset class="mode__options" aria-busy=${isPending(feedback) ? "true" : "false"}>
      <legend class="visually-hidden">Mode</legend>
      ${option("autonomous", "Auto", status?.mode, disabled, onChange)} ${option("manual", "Manual", status?.mode, disabled, onChange)}
    </fieldset>
    ${status?.mode === "autonomous" ? html`<p class="mode__note">${AUTO_STATE_TEXT[status.auto_state]}</p>` : null}
    ${feedbackLine(feedback)}
  </section>`;
}

function option(
  mode: DeviceMode,
  label: string,
  current: DeviceMode | undefined,
  disabled: boolean,
  onChange: (mode: DeviceMode) => void,
): TemplateResult {
  return html`<label class="mode__option">
    <input
      class="mode__input"
      type="radio"
      name="mode"
      .checked=${live(current === mode)}
      ?disabled=${disabled}
      @change=${() => onChange(mode)}
    />
    <span class="mode__label">${label}</span>
  </label>`;
}
