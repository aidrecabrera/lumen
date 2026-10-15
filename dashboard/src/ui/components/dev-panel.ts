import { html, type TemplateResult } from "lit-html";

export interface DevPanelHandlers {
  goOffline(): void;
  comeBack(): void;
  rejectNext(): void;
  dropNextAck(): void;
  nightOff(): void;
}

export function devPanelView(on: DevPanelHandlers): TemplateResult {
  return html`<aside class="dev-panel" aria-labelledby="dev-title">
    <h2 class="dev-panel__title" id="dev-title">Prototype controls (simulated node)</h2>
    <div class="dev-panel__actions">
      <button type="button" @click=${on.goOffline}>Node goes offline</button>
      <button type="button" @click=${on.comeBack}>Node comes back</button>
      <button type="button" @click=${on.rejectNext}>Reject next command</button>
      <button type="button" @click=${on.dropNextAck}>Drop next ack</button>
      <button type="button" @click=${on.nightOff}>Send night_off</button>
    </div>
  </aside>`;
}
