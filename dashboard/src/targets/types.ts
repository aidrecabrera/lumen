import type { nothing, TemplateResult } from "lit-html";

import type { Transport } from "../transport/transport.ts";

export interface Target {
  transport: Transport;
  staleAfterMs: number;
  extras: () => TemplateResult | typeof nothing;
}
