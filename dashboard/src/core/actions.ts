import { LUX_CEILING_MAX, LUX_CEILING_MIN, type Command, type DeviceMode, type LedState, type Thresholds } from "./contract.ts";
import type { Step } from "./commands.ts";

export type Channel = "red" | "blue" | "far_red";

export const CHANNELS: readonly Channel[] = ["red", "blue", "far_red"];
export const CHANNEL_NAMES: Record<Channel, string> = { red: "Red", blue: "Blue", far_red: "Far-red" };

type ShareKey = "red_dist_pct" | "blue_dist_pct" | "far_red_dist_pct";

type EnabledKey = "red_enabled" | "blue_enabled" | "far_red_enabled";

export type Shares = Pick<LedState, ShareKey>;

export const SHARE_KEY: Record<Channel, ShareKey> = {
  red: "red_dist_pct",
  blue: "blue_dist_pct",
  far_red: "far_red_dist_pct",
};

export const ENABLED_KEY: Record<Channel, EnabledKey> = {
  red: "red_enabled",
  blue: "blue_enabled",
  far_red: "far_red_enabled",
};

const OTHER_CHANNELS: Record<Channel, readonly [Channel, Channel]> = {
  red: ["blue", "far_red"],
  blue: ["red", "far_red"],
  far_red: ["red", "blue"],
};

export function ledCommand(led: LedState, change: Partial<LedState>): Command {
  return { kind: "led", led: { ...led, ...change } };
}

export function modeCommand(mode: DeviceMode): Command {
  return { kind: "mode", mode };
}

// the node rejects shares that do not total exactly 100, so the other two channels absorb the change.
export function rebalanceShares(shares: Shares, channel: Channel, requested: number): Shares {
  const value = Math.min(100, Math.max(0, Math.round(requested)));
  const [first, second] = OTHER_CHANNELS[channel];
  const remaining = 100 - value;
  const firstShare = shares[SHARE_KEY[first]];
  const othersTotal = firstShare + shares[SHARE_KEY[second]];
  const firstNext = othersTotal === 0 ? Math.ceil(remaining / 2) : Math.round((remaining * firstShare) / othersTotal);

  return {
    ...shares,
    [SHARE_KEY[channel]]: value,
    [SHARE_KEY[first]]: firstNext,
    [SHARE_KEY[second]]: remaining - firstNext,
  };
}

const toManual: Step = { label: "Switch to manual", command: () => modeCommand("manual") };
const toAuto: Step = { label: "Switch to auto", command: () => modeCommand("autonomous") };
const powerOn: Step = { label: "Turn the lights on", command: (led) => ledCommand(led, { power: true }) };

// auto mode does nothing while power is off, so switching to auto turns the lights on first.
export function switchToAutoSteps(led: LedState): readonly Step[] {
  return led.power ? [toAuto] : [powerOn, toAuto];
}

export interface Preset {
  id: "balanced" | "off";
  label: string;
  steps: readonly Step[];
}

export const PRESETS: readonly Preset[] = [
  {
    id: "balanced",
    label: "Balanced growth (starting point)",
    steps: [
      toManual,
      {
        label: "Set the light mix",
        command: (led) =>
          ledCommand(led, {
            power: true,
            red_enabled: true,
            blue_enabled: true,
            far_red_enabled: true,
            red_dist_pct: 60,
            blue_dist_pct: 30,
            far_red_dist_pct: 10,
          }),
      },
      toAuto,
    ],
  },
  {
    id: "off",
    label: "Lights off",
    steps: [toManual, { label: "Turn the lights off", command: (led) => ledCommand(led, { power: false }) }],
  },
];

export interface SettingsForm {
  temp_min_c: string;
  temp_max_c: string;
  humidity_min_pct: string;
  humidity_max_pct: string;
  lux_ceiling: string;
}

export type SettingsField = keyof SettingsForm;

export type SettingsParse =
  | { ok: true; command: Command }
  | { ok: false; errors: Partial<Record<SettingsField, string>> };

export function settingsForm(thresholds: Thresholds, luxCeiling: number): SettingsForm {
  return {
    temp_min_c: String(round1(thresholds.temp_min_c)),
    temp_max_c: String(round1(thresholds.temp_max_c)),
    humidity_min_pct: String(round1(thresholds.humidity_min_pct)),
    humidity_max_pct: String(round1(thresholds.humidity_max_pct)),
    lux_ceiling: String(Math.round(luxCeiling)),
  };
}

export function parseSettings(form: SettingsForm): SettingsParse {
  const errors: Partial<Record<SettingsField, string>> = {};

  const number = (field: SettingsField): number => {
    const value = Number(form[field]);

    if (form[field].trim() === "" || !Number.isFinite(value)) {
      errors[field] = "Enter a number.";
    }

    return value;
  };

  const thresholds: Thresholds = {
    temp_min_c: number("temp_min_c"),
    temp_max_c: number("temp_max_c"),
    humidity_min_pct: number("humidity_min_pct"),
    humidity_max_pct: number("humidity_max_pct"),
  };

  const luxCeiling = number("lux_ceiling");

  if (thresholds.temp_min_c > thresholds.temp_max_c) {
    errors.temp_max_c = "Must be at least the minimum.";
  }

  if (thresholds.humidity_min_pct > thresholds.humidity_max_pct) {
    errors.humidity_max_pct = "Must be at least the minimum.";
  }

  if (luxCeiling < LUX_CEILING_MIN || luxCeiling > LUX_CEILING_MAX) {
    errors.lux_ceiling = `Enter ${LUX_CEILING_MIN} to ${LUX_CEILING_MAX}.`;
  }

  if (Object.keys(errors).length > 0) {
    return { ok: false, errors };
  }

  return { ok: true, command: { kind: "config", thresholds, lux_ceiling: luxCeiling } };
}

function round1(value: number): number {
  return Math.round(value * 10) / 10;
}
