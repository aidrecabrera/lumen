import { html, type TemplateResult } from "lit-html";

import { PRESETS, type Preset } from "../../core/actions.ts";
import { feedbackLine, isPending, type Feedback } from "./feedback.ts";

export interface PresetsProps {
  enabled: boolean;
  feedback: Feedback;
  offerBackToAuto: boolean;
  onRun: (preset: Preset) => void;
  onBackToAuto: () => void;
}

export function presetsView({ enabled, feedback, offerBackToAuto, onRun, onBackToAuto }: PresetsProps): TemplateResult {
  const disabled = !enabled || isPending(feedback);

  return html`<section class="panel presets" aria-labelledby="presets-title">
    <h2 class="panel__title" id="presets-title">Presets</h2>
    <div class="presets__list" aria-busy=${isPending(feedback) ? "true" : "false"}>
      ${PRESETS.map((preset) => html`<button type="button" ?disabled=${disabled} @click=${() => onRun(preset)}>${preset.label}</button>`)}
    </div>
    ${feedbackLine(feedback)}
    ${offerBackToAuto && !isPending(feedback) ? html`<button type="button" @click=${onBackToAuto}>Back to auto</button>` : null}
  </section>`;
}
