import { html, type TemplateResult } from "lit-html";

import type { CloudState, Energy, Status } from "../../core/contract.ts";
import type { Known } from "../../core/state.ts";

const CLOUD_TEXT: Record<CloudState, string> = {
  connected: "Connected",
  connecting: "Connecting",
  not_configured: "Not set up",
  invalid_config: "Setup file is invalid",
};

const WIFI_TEXT: Record<Status["degraded_tier"], string> = {
  none: "Connected",
  wifi_retrying: "Reconnecting",
  wifi_retry_limit: "Reconnecting (many failed attempts)",
};

export function energyView(energy: Known<Energy>, status: Status | undefined): TemplateResult {
  return html`<section class="panel energy" aria-labelledby="energy-title">
    <h2 class="panel__title" id="energy-title">Energy and node</h2>
    <dl class="energy__list">
      ${energy.kind === "known"
        ? html`${item("Total use", wattHours(energy.value.total_wh))} ${item("Since the node started", wattHours(energy.value.session_wh))}
          ${item("Lights on since the node started", duration(energy.value.light_on_sec))}`
        : item("Energy", "Waiting for the first report…")}
      ${status === undefined
        ? null
        : html`${item("Node running for", duration(status.uptime_sec))} ${item("Node Wi-Fi", WIFI_TEXT[status.degraded_tier])}
          ${item("Remote access", CLOUD_TEXT[status.cloud])}`}
    </dl>
  </section>`;
}

function item(label: string, value: string): TemplateResult {
  return html`<div class="energy__item">
    <dt class="energy__label">${label}</dt>
    <dd class="energy__value">${value}</dd>
  </div>`;
}

function wattHours(wh: number): string {
  return wh >= 1000 ? `${(wh / 1000).toFixed(2)} kWh` : `${wh.toFixed(1)} Wh`;
}

function duration(totalSeconds: number): string {
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);

  return hours > 0 ? `${hours} h ${minutes} min` : `${minutes} min`;
}
