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
