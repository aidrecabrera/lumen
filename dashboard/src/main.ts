import { createTarget } from "virtual:target";

import { Dashboard } from "./ui/dashboard.ts";

const root = document.querySelector<HTMLElement>("#app");

if (root === null) {
  throw new Error("missing #app");
}

const target = await createTarget(root);

await new Dashboard(target, root).start();
