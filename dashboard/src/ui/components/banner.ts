import { html, type TemplateResult } from "lit-html";

import type { Banner } from "../../core/state.ts";

export function bannerView(banner: Banner): TemplateResult {
  return html`<p class="banner" role="status" aria-live="polite" data-tone=${banner.tone}>${banner.text}</p>`;
}
