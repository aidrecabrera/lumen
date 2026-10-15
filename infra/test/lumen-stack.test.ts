import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';

import { App } from 'aws-cdk-lib';
import { Match, Template } from 'aws-cdk-lib/assertions';
import * as v from 'valibot';

import { LumenStack } from '../lib/lumen-stack.ts';

type Intrinsic = string | { Ref: string } | { 'Fn::GetAtt': [string, string] } | { 'Fn::Join': [string, Intrinsic[]] };

// renders cloudformation intrinsics as text, with references kept as {Name}
const RenderedSchema: v.GenericSchema<Intrinsic, string> = v.lazy(() =>
  v.union([
    v.string(),
    v.pipe(
      v.strictObject({ Ref: v.string() }),
      v.transform((value) => `{${value.Ref}}`),
    ),
    v.pipe(
      v.strictObject({ 'Fn::GetAtt': v.tuple([v.string(), v.string()]) }),
      v.transform((value) => `{${value['Fn::GetAtt'].join('.')}}`),
    ),
    v.pipe(
      v.strictObject({ 'Fn::Join': v.tuple([v.string(), v.array(RenderedSchema)]) }),
      v.transform((value) => value['Fn::Join'][1].join(value['Fn::Join'][0])),
    ),
  ]),
);

const PolicyDocumentSchema = v.object({
  Statement: v.array(
    v.object({
      Effect: v.literal('Allow'),
      Action: v.union([v.string(), v.array(v.string())]),
      Resource: v.union([RenderedSchema, v.array(RenderedSchema)]),
    }),
  ),
});

const TemplateSchema = v.object({
  Parameters: v.record(v.string(), v.object({ AllowedPattern: v.optional(v.string()) })),
  Resources: v.record(
    v.string(),
    v.object({ Type: v.string(), Properties: v.optional(v.object({ PolicyDocument: v.optional(v.unknown()) })) }),
  ),
});

type ParsedTemplate = v.InferOutput<typeof TemplateSchema>;

interface Synthesized {
  template: Template;
  parsed: ParsedTemplate;
}

const IOT_ARN_PREFIX = 'arn:{AWS::Partition}:iot:{AWS::Region}:{AWS::AccountId}:';

const RETAINED = ['availability', 'energy', 'status', 'telemetry'];

const COMMANDS = ['command/config', 'command/led', 'command/mode'];

function withoutIotPrefix(arn: string): string {
  return arn.startsWith(IOT_ARN_PREFIX) ? arn.slice(IOT_ARN_PREFIX.length) : arn;
}

function synth(): Synthesized {
  const dashboardDir = mkdtempSync(join(tmpdir(), 'lumen-dashboard-'));
  writeFileSync(join(dashboardDir, 'index.html'), '<!doctype html>');
  const app = new App();
  const stack = new LumenStack(app, 'TestStack', { dashboardDir, callbackUrls: [] });
  const template = Template.fromStack(stack);

  return { template, parsed: v.parse(TemplateSchema, template.toJSON()) };
}

function permissionsOf(parsed: ParsedTemplate, logicalIdPrefix: string): Map<string, string[]> {
  const matches = Object.entries(parsed.Resources).filter(([id]) => id.startsWith(logicalIdPrefix));
  assert.equal(matches.length, 1, `one resource named ${logicalIdPrefix}*`);
  const document = v.parse(PolicyDocumentSchema, matches[0]?.[1].Properties?.PolicyDocument);
  const permissions = new Map<string, string[]>();

  for (const statement of document.Statement) {
    const actions = [statement.Action].flat();
    const resources = [statement.Resource].flat().map(withoutIotPrefix);

    for (const action of actions) {
      permissions.set(action, [...(permissions.get(action) ?? []), ...resources].sort());
    }
  }

  return permissions;
}

function topics(suffixes: string[]): string[] {
  return suffixes.map((suffix) => `topic/lumen/{DeviceId}/${suffix}`).sort();
}

function filters(suffixes: string[]): string[] {
  return suffixes.map((suffix) => `topicfilter/lumen/{DeviceId}/${suffix}`).sort();
}

const { template, parsed } = synth();

const dashboardPermissions = new Map([
  ['iot:Connect', ['client/${cognito-identity.amazonaws.com:sub}-*']],
  ['iot:Subscribe', filters([...RETAINED, 'ack/+'])],
  ['iot:Receive', topics([...RETAINED, 'ack/*'])],
  ['iot:Publish', topics(COMMANDS)],
]);

test('the node may only use its own client id and topics', () => {
  const expected = new Map([
    ['iot:Connect', ['client/lumen-fw-{DeviceId}']],
    ['iot:Publish', topics([...RETAINED, 'ack/*'])],
    ['iot:RetainPublish', topics(RETAINED)],
    ['iot:Subscribe', filters(COMMANDS)],
    ['iot:Receive', topics(['command/*'])],
  ]);

  assert.deepEqual(permissionsOf(parsed, 'NodePolicy'), expected);
});

test('a dashboard login may read node topics but only publish commands', () => {
  assert.deepEqual(permissionsOf(parsed, 'DashboardPolicy'), dashboardPermissions);
});

test('the browser role matches the dashboard iot policy plus self-attach', () => {
  const expected = new Map([...dashboardPermissions, ['iot:AttachPolicy', ['*']]]);

  assert.deepEqual(permissionsOf(parsed, 'DashboardAuthRoleDefaultPolicy'), expected);
});

test('device id and budget email are checked by cloudformation', () => {
  const devicePattern = new RegExp(parsed.Parameters['DeviceId']?.AllowedPattern ?? '(?!)');
  const emailPattern = new RegExp(parsed.Parameters['BudgetEmail']?.AllowedPattern ?? '(?!)');

  assert.ok(devicePattern.test('lumen-aabbccddeeff'));
  assert.ok(!devicePattern.test('lumen-AABBCCDDEEFF'));
  assert.ok(!devicePattern.test('lumen-aabb'));
  assert.ok(emailPattern.test('owner@example.com'));
  assert.ok(!emailPattern.test('owner'));
});

test('only invited users can sign in, with no client secret and no guest identities', () => {
  template.hasResourceProperties('AWS::Cognito::UserPool', {
    AdminCreateUserConfig: { AllowAdminCreateUserOnly: true },
    UserPoolTier: 'LITE',
  });

  template.hasResourceProperties('AWS::Cognito::UserPoolClient', {
    GenerateSecret: false,
    AllowedOAuthFlows: ['code'],
  });

  template.hasResourceProperties('AWS::Cognito::IdentityPool', { AllowUnauthenticatedIdentities: false });
});

test('teardown removes stored data and logs stay one week at error level', () => {
  for (const type of ['AWS::Cognito::UserPool', 'AWS::S3::Bucket', 'AWS::Logs::LogGroup', 'AWS::Cognito::IdentityPool']) {
    template.hasResource(type, { DeletionPolicy: 'Delete' });
  }

  template.hasResourceProperties('AWS::Logs::LogGroup', { RetentionInDays: 7 });
  template.hasResourceProperties('AWS::IoT::Logging', { DefaultLogLevel: 'ERROR' });
  template.hasResourceProperties('AWS::Budgets::Budget', {
    Budget: Match.objectLike({ BudgetLimit: { Amount: 1, Unit: 'USD' } }),
  });
});
