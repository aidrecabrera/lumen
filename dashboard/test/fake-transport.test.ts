import { encodeCommand } from "../src/core/contract.ts";
import { FakeTransport } from "../src/transport/fake.ts";
import { describeTransportContract } from "./transport-contract.ts";

describeTransportContract("fake", async () => {
  const transport = new FakeTransport();

  return {
    transport,
    node: {
      publish: async (suffix, payload, retained) => transport.deliver(suffix, payload, retained),
      settle: async () => undefined,
      received: async () => {
        const { suffix, commandId, command } = transport.lastCommand();

        return { suffix, payload: encodeCommand(commandId, command) };
      },
    },
  };
});
