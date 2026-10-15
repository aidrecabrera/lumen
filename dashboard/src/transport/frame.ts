import { decode, encode } from "@msgpack/msgpack";
import * as v from "valibot";

const FrameSchema = v.object({ t: v.string(), p: v.instance(Uint8Array) });

export type Frame = v.InferOutput<typeof FrameSchema>;

export function encodeFrame(suffix: string, payload: Uint8Array): Uint8Array {
  return encode({ t: suffix, p: payload });
}

export function decodeFrame(bytes: Uint8Array): Frame | undefined {
  try {
    return v.parse(FrameSchema, decode(bytes));
  } catch {
    return undefined;
  }
}
