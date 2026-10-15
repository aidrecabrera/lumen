import { html, type TemplateResult } from "lit-html";

import type { Telemetry } from "../../core/contract.ts";
import { ageText, isStale, type Known } from "../../core/state.ts";

export function readingsView(telemetry: Known<Telemetry>, now: number, staleAfterMs: number): TemplateResult {
  if (telemetry.kind === "none") {
    return html`<section class="panel readings" aria-labelledby="readings-title">
      <h2 class="panel__title" id="readings-title">Readings</h2>
      <p class="readings__empty">Waiting for the first reading…</p>
    </section>`;
  }

  const { value, receivedAt, lastKnown } = telemetry;
  const age = lastKnown ? "Last known value" : ageText(receivedAt, now);

  return html`<section class="panel readings" aria-labelledby="readings-title" data-tone=${isStale(telemetry, now, staleAfterMs) ? "stale" : "live"}>
    <h2 class="panel__title" id="readings-title">Readings</h2>
    <dl class="readings__list">
      ${reading("Daylight", Math.round(value.light_lux).toLocaleString(), "lux")}
      ${reading("Temperature", value.temperature_c.toFixed(1), "°C")}
      ${reading("Humidity", Math.round(value.humidity_pct).toString(), "%")}
    </dl>
    <p class="readings__age">${age}</p>
  </section>`;
}

function reading(label: string, value: string, unit: string): TemplateResult {
  return html`<div class="readings__item">
    <dt class="readings__label">${label}</dt>
    <dd class="readings__value">${value} <span class="readings__unit">${unit}</span></dd>
  </div>`;
}
