import { html, type TemplateResult } from "lit-html";
import { ref } from "lit-html/directives/ref.js";
import uPlot from "uplot";

import type { HistoryPoint } from "../../core/state.ts";

export interface HistoryProps {
  history: readonly HistoryPoint[];
  onExport: () => void;
}

interface Series {
  key: "lux" | "temperature" | "humidity";
  title: string;
  unit: string;
  color: string;
}

const SERIES: readonly Series[] = [
  { key: "lux", title: "Daylight", unit: "lux", color: "--color-chart-lux" },
  { key: "temperature", title: "Temperature", unit: "°C", color: "--color-chart-temp" },
  { key: "humidity", title: "Humidity", unit: "%", color: "--color-chart-humidity" },
];

const charts = new WeakMap<Element, uPlot>();

export function historyView({ history, onExport }: HistoryProps): TemplateResult {
  const empty = history.length < 2;

  return html`<section class="panel history" aria-labelledby="history-title">
    <div class="history__header">
      <h2 class="panel__title" id="history-title">Since this page opened</h2>
      <button type="button" ?disabled=${history.length === 0} @click=${onExport}>Export CSV</button>
    </div>
    ${empty
      ? html`<p class="history__empty">Charts fill while this page is open.</p>`
      : SERIES.map(
          (series) => html`<figure class="history__chart">
            <figcaption class="history__caption">${series.title} (${series.unit})</figcaption>
            <div class="history__plot" role="img" aria-label=${`${series.title} chart`} ${ref((element) => drawChart(element, series, history))}></div>
          </figure>`,
        )}
  </section>`;
}

function drawChart(element: Element | undefined, series: Series, history: readonly HistoryPoint[]): void {
  if (!(element instanceof HTMLElement)) {
    return;
  }

  // lit calls ref before it inserts new nodes; styles and width exist only once attached.
  if (!element.isConnected) {
    queueMicrotask(() => drawChart(element, series, history));

    return;
  }

  const data: uPlot.AlignedData = [history.map((point) => point.at / 1000), history.map((point) => point[series.key])];
  const existing = charts.get(element);

  if (existing !== undefined) {
    existing.setData(data);

    return;
  }

  const style = getComputedStyle(element);
  const stroke = style.getPropertyValue(series.color).trim();
  const axis = { stroke: style.getPropertyValue("--color-text-muted").trim(), grid: { stroke: style.getPropertyValue("--color-border").trim() } };

  const options: uPlot.Options = {
    width: element.clientWidth,
    height: 160,
    legend: { show: false },
    series: [{}, { label: series.title, stroke, width: 2 }],
    axes: [axis, { ...axis, size: 56 }],
  };

  charts.set(element, new uPlot(options, data, element));
}
