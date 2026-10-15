import axe from "axe-core";
import { nothing } from "lit-html";
import { expect, it } from "vitest";

import { FakeTransport } from "../src/transport/fake.ts";
import * as fixtures from "../scripts/fixtures.ts";

// jsdom lacks matchMedia, which uplot reads when it loads.
window.matchMedia = (query: string): MediaQueryList => ({
  media: query,
  matches: false,
  onchange: null,
  addListener: () => undefined,
  removeListener: () => undefined,
  addEventListener: () => undefined,
  removeEventListener: () => undefined,
  dispatchEvent: () => false,
});

const { Dashboard } = await import("../src/ui/dashboard.ts");

it("renders the live dashboard with no axe violations", async () => {
  const root = document.createElement("main");
  const transport = new FakeTransport();

  document.body.append(root);
  await new Dashboard({ transport, staleAfterMs: 6000, extras: () => nothing }, root).start();
  transport.deliver("availability", fixtures.files["availability-online.txt"], true);
  transport.deliver("status", fixtures.files["status.msgpack"], true);
  transport.deliver("telemetry", fixtures.files["telemetry.msgpack"]);
  transport.deliver("energy", fixtures.files["energy.msgpack"]);

  expect(root.querySelector(".banner")?.textContent).toBe("Live");
  expect(root.querySelector<HTMLInputElement>('input[role="switch"]')?.checked).toBe(true);

  // jsdom has no layout, so contrast cannot be measured here.
  const results = await axe.run(root, { rules: { "color-contrast": { enabled: false } } });

  expect(results.violations.map((violation) => `${violation.id}: ${violation.help}`)).toEqual([]);
});
