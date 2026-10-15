import { Aws, CfnOutput, Duration, RemovalPolicy, Stack, type StackProps } from 'aws-cdk-lib';
import { CfnBudget } from 'aws-cdk-lib/aws-budgets';
import { Distribution, ViewerProtocolPolicy } from 'aws-cdk-lib/aws-cloudfront';
import { S3BucketOrigin } from 'aws-cdk-lib/aws-cloudfront-origins';
import {
  AccountRecovery,
  CfnIdentityPool,
  CfnIdentityPoolRoleAttachment,
  FeaturePlan,
  OAuthScope,
  UserPool,
  UserPoolClientIdentityProvider,
} from 'aws-cdk-lib/aws-cognito';
import { FederatedPrincipal, PolicyStatement, Role, ServicePrincipal } from 'aws-cdk-lib/aws-iam';
import { CfnLogging, CfnPolicy, CfnThing } from 'aws-cdk-lib/aws-iot';
import { LogGroup, RetentionDays } from 'aws-cdk-lib/aws-logs';
import { BlockPublicAccess, Bucket } from 'aws-cdk-lib/aws-s3';
import { BucketDeployment, Source } from 'aws-cdk-lib/aws-s3-deployment';
import { AwsCustomResource, AwsCustomResourcePolicy, PhysicalResourceId } from 'aws-cdk-lib/custom-resources';
import type { Construct } from 'constructs';

export interface LumenStackProps extends StackProps {
  deviceId: string;
  dashboardDir: string;
  callbackUrls: string[];
  budgetEmail: string;
}

const DEV_URL = 'http://localhost:5173/';

export class LumenStack extends Stack {
  constructor(scope: Construct, id: string, props: LumenStackProps) {
    super(scope, id, props);
    const { deviceId } = props;

    const arn = (resource: string) => `arn:${Aws.PARTITION}:iot:${Aws.REGION}:${Aws.ACCOUNT_ID}:${resource}`;
    const topic = (suffix: string) => arn(`topic/lumen/${deviceId}/${suffix}`);
    const filter = (suffix: string) => arn(`topicfilter/lumen/${deviceId}/${suffix}`);

    const iotDoc = (statements: { Action: string; Resource: string[] }[]) => ({
      Version: '2012-10-17',
      Statement: statements.map((s) => ({ Effect: 'Allow', ...s })),
    });

    const thing = new CfnThing(this, 'Thing', { thingName: deviceId });

    const retained = ['status', 'telemetry', 'energy', 'availability'];
    const commands = ['led', 'config', 'mode'];

    const nodePolicy = new CfnPolicy(this, 'NodePolicy', {
      policyDocument: iotDoc([
        { Action: 'iot:Connect', Resource: [arn(`client/lumen-fw-${deviceId}`)] },
        { Action: 'iot:Publish', Resource: [...retained.map(topic), topic('ack/*')] },
        { Action: 'iot:RetainPublish', Resource: retained.map(topic) },
        { Action: 'iot:Subscribe', Resource: commands.map((c) => filter(`command/${c}`)) },
        { Action: 'iot:Receive', Resource: [topic('command/*')] },
      ]),
    });

    const browserStatements = [
      { Action: 'iot:Connect', Resource: [arn('client/${cognito-identity.amazonaws.com:sub}-*')] },
      { Action: 'iot:Subscribe', Resource: [...retained, 'ack/+'].map(filter) },
      { Action: 'iot:Receive', Resource: [...retained, 'ack/*'].map(topic) },
      { Action: 'iot:Publish', Resource: commands.map((c) => topic(`command/${c}`)) },
    ];

    const dashboardPolicy = new CfnPolicy(this, 'DashboardPolicy', { policyDocument: iotDoc(browserStatements) });

    const bucket = new Bucket(this, 'DashboardBucket', {
      blockPublicAccess: BlockPublicAccess.BLOCK_ALL,
      enforceSSL: true,
      autoDeleteObjects: true,
      removalPolicy: RemovalPolicy.DESTROY,
    });

    const distribution = new Distribution(this, 'Distribution', {
      defaultRootObject: 'index.html',
      defaultBehavior: {
        origin: S3BucketOrigin.withOriginAccessControl(bucket),
        viewerProtocolPolicy: ViewerProtocolPolicy.REDIRECT_TO_HTTPS,
      },
    });

    const dashboardUrl = `https://${distribution.distributionDomainName}/`;

    // lite serves the classic hosted ui; the newer managed login pages need the paid essentials plan.
    const userPool = new UserPool(this, 'UserPool', {
      selfSignUpEnabled: false,
      signInAliases: { email: true },
      autoVerify: { email: true },
      accountRecovery: AccountRecovery.EMAIL_ONLY,
      featurePlan: FeaturePlan.LITE,
      removalPolicy: RemovalPolicy.DESTROY,
    });

    const domain = userPool.addDomain('Domain', {
      cognitoDomain: { domainPrefix: `${domainSafe(deviceId)}-${Aws.ACCOUNT_ID}` },
    });

    const redirectUrls = [...new Set([dashboardUrl, DEV_URL, ...props.callbackUrls])];

    const client = userPool.addClient('DashboardClient', {
      generateSecret: false,
      // rotation rejects ALLOW_REFRESH_TOKEN_AUTH, so pin the explicit flows instead of taking the defaults
      authFlows: { userSrp: true },
      oAuth: {
        flows: { authorizationCodeGrant: true },
        scopes: [OAuthScope.OPENID, OAuthScope.EMAIL, OAuthScope.PROFILE],
        callbackUrls: redirectUrls,
        logoutUrls: redirectUrls,
      },
      supportedIdentityProviders: [UserPoolClientIdentityProvider.COGNITO],
      preventUserExistenceErrors: true,
      enableTokenRevocation: true,
      refreshTokenRotationGracePeriod: Duration.seconds(10),
    });

    const oauthDomain = `${domain.domainName}.auth.${Aws.REGION}.amazoncognito.com`;

    const identityPool = new CfnIdentityPool(this, 'IdentityPool', {
      allowUnauthenticatedIdentities: false,
      cognitoIdentityProviders: [
        { clientId: client.userPoolClientId, providerName: userPool.userPoolProviderName },
      ],
    });

    identityPool.applyRemovalPolicy(RemovalPolicy.DESTROY);

    const authRole = new Role(this, 'DashboardAuthRole', {
      assumedBy: new FederatedPrincipal(
        'cognito-identity.amazonaws.com',
        {
          StringEquals: { 'cognito-identity.amazonaws.com:aud': identityPool.ref },
          'ForAnyValue:StringLike': { 'cognito-identity.amazonaws.com:amr': 'authenticated' },
        },
        'sts:AssumeRoleWithWebIdentity',
      ),
    });

    for (const s of browserStatements) {
      authRole.addToPolicy(new PolicyStatement({ actions: [s.Action], resources: s.Resource }));
    }

    // iot:AttachPolicy only scopes to cert or thinggroup resources, never the policy, so it cannot be narrowed
    authRole.addToPolicy(new PolicyStatement({ actions: ['iot:AttachPolicy'], resources: ['*'] }));
    new CfnIdentityPoolRoleAttachment(this, 'IdentityPoolRoles', {
      identityPoolId: identityPool.ref,
      roles: { authenticated: authRole.roleArn },
    });

    const endpoint = new AwsCustomResource(this, 'IotEndpointLookup', {
      onUpdate: {
        service: 'Iot',
        action: 'describeEndpoint',
        parameters: { endpointType: 'iot:Data-ATS' },
        physicalResourceId: PhysicalResourceId.of('iot-data-ats'),
      },
      policy: AwsCustomResourcePolicy.fromSdkCalls({ resources: AwsCustomResourcePolicy.ANY_RESOURCE }),
      installLatestAwsSdk: false,
    });

    const iotEndpoint = endpoint.getResponseField('endpointAddress');

    new BucketDeployment(this, 'DashboardDeployment', {
      destinationBucket: bucket,
      sources: [
        Source.asset(props.dashboardDir),
        Source.jsonData('config.json', {
          region: Aws.REGION,
          userPoolId: userPool.userPoolId,
          userPoolClientId: client.userPoolClientId,
          identityPoolId: identityPool.ref,
          oauthDomain,
          redirectUrl: dashboardUrl,
          iotEndpoint,
          deviceId,
          dashboardPolicyName: dashboardPolicy.ref,
        }),
      ],
      distribution,
      distributionPaths: ['/*'],
    });

    // created here so teardown removes it; iot v2 logging writes to this fixed name
    const logGroup = new LogGroup(this, 'IotLogs', {
      logGroupName: 'AWSIotLogsV2',
      retention: RetentionDays.ONE_WEEK,
      removalPolicy: RemovalPolicy.DESTROY,
    });

    const loggingRole = new Role(this, 'IotLoggingRole', { assumedBy: new ServicePrincipal('iot.amazonaws.com') });
    loggingRole.addToPolicy(
      new PolicyStatement({
        actions: ['logs:CreateLogGroup', 'logs:CreateLogStream', 'logs:PutLogEvents', 'logs:PutMetricFilter', 'logs:PutRetentionPolicy'],
        resources: [logGroup.logGroupArn],
      }),
    );

    const logging = new CfnLogging(this, 'IotLogging', {
      accountId: Aws.ACCOUNT_ID,
      defaultLogLevel: 'ERROR',
      roleArn: loggingRole.roleArn,
    });

    logging.node.addDependency(loggingRole, logGroup);

    // an alert does not stop charges; it only emails the owner.
    new CfnBudget(this, 'Budget', {
      budget: { budgetType: 'COST', timeUnit: 'MONTHLY', budgetLimit: { amount: 1, unit: 'USD' } },
      notificationsWithSubscribers: [
        {
          notification: {
            notificationType: 'ACTUAL',
            comparisonOperator: 'GREATER_THAN',
            threshold: 100,
            thresholdType: 'PERCENTAGE',
          },
          subscribers: [{ subscriptionType: 'EMAIL', address: props.budgetEmail }],
        },
      ],
    });

    const outputs = {
      DashboardUrl: dashboardUrl,
      UserPoolId: userPool.userPoolId,
      UserPoolClientId: client.userPoolClientId,
      IdentityPoolId: identityPool.ref,
      IotEndpoint: iotEndpoint,
      NodePolicyName: nodePolicy.ref,
      DashboardPolicyName: dashboardPolicy.ref,
      ThingName: thing.ref,
      OauthDomain: oauthDomain,
    } satisfies Record<string, string>;

    for (const [key, value] of Object.entries(outputs)) new CfnOutput(this, key, { value });
  }
}

function domainSafe(value: string): string {
  return value.toLowerCase().replace(/[^a-z0-9-]/g, '-').replace(/^-+|-+$/g, '').slice(0, 40) || 'lumen';
}
