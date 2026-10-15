import { App } from 'aws-cdk-lib';

import { LumenStack } from '../lib/lumen-stack.ts';

const app = new App();

function context(key: string): string {
  return String(app.node.tryGetContext(key) ?? '').trim();
}

const deviceId = context('deviceId');
const budgetEmail = context('budgetEmail');

if (!/^lumen-[0-9a-f]{12}$/.test(deviceId)) throw new Error('pass -c deviceId=lumen-<12 hex mac digits>');

if (!budgetEmail.includes('@')) throw new Error('pass -c budgetEmail=you@example.com for the $1 budget alert');

new LumenStack(app, 'LumenStack', {
  deviceId,
  budgetEmail,
  dashboardDir: context('dashboardDir') || '../dashboard/dist-aws',
  callbackUrls: context('callbackUrls').split(',').filter((url) => url !== ''),
});
