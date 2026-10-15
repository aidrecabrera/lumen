import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";

const TARGETS = ["local", "aws", "emqx", "dev", "fake"] as const;

type Target = (typeof TARGETS)[number];

function targetFor(mode: string): Target {
  const found = TARGETS.find((target) => target === mode);

  // vitest runs in "test" mode and never imports the target module.
  return found ?? "fake";
}

export default defineConfig(({ mode }) => {
  const target = targetFor(mode);

  return {
    base: "./",
    resolve: {
      alias: {
        "virtual:target": fileURLToPath(new URL(`./src/targets/${target}.ts`, import.meta.url)),
      },
    },
    build: {
      outDir: `dist-${target}`,
      emptyOutDir: true,
      assetsInlineLimit: 0,
      target: "es2022",
    },
    test: {
      environment: "jsdom",
      include: ["test/**/*.test.ts"],
    },
  };
});
