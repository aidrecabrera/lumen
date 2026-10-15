import { App } from 'aws-cdk-lib';

import { LumenStack } from '../lib/lumen-stack.ts';

const app = new App();

function context(key: string): string {
  return String(app.node.tryGetContext(key) ?? '').trim();
}

new LumenStack(app, 'LumenStack', {
  dashboardDir: context('dashboardDir') || '../dashboard/dist-aws',
  callbackUrls: context('callbackUrls').split(',').filter((url) => url !== ''),
});
