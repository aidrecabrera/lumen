import { decode, encode } from "@msgpack/msgpack";

export interface Frame {
  t: string;
  p: Uint8Array;
}

export function encodeFrame(suffix: string, payload: Uint8Array): Uint8Array {
  return encode({ t: suffix, p: payload });
}

export function decodeFrame(bytes: Uint8Array): Frame | undefined {
  let value: unknown;
  try {
    value = decode(bytes);
  } catch {
    return undefined;
  }
  if (typeof value !== "object" || value === null) return undefined;
  const { t, p } = value as Record<string, unknown>;
  if (typeof t !== "string" || !(p instanceof Uint8Array)) return undefined;
  return { t, p };
}
