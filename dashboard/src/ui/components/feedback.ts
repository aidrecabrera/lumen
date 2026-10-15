import { html, nothing, type TemplateResult } from "lit-html";

export type Feedback = { kind: "idle" } | { kind: "pending"; text: string } | { kind: "done"; ok: boolean; text: string };

export const IDLE: Feedback = { kind: "idle" };

export function feedbackLine(feedback: Feedback): TemplateResult | typeof nothing {
  switch (feedback.kind) {
    case "idle":
      return nothing;
    case "pending":
      return html`<p class="control__feedback" aria-busy="true"><span class="spinner" aria-hidden="true"></span> ${feedback.text}</p>`;
    case "done":
      return html`<p class="control__feedback" aria-invalid=${feedback.ok ? "false" : "true"}>${feedback.text}</p>`;
  }
}

export function isPending(feedback: Feedback): boolean {
  return feedback.kind === "pending";
}
