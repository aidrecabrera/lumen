import { AttachPolicyCommand, IoTClient } from "@aws-sdk/client-iot";
import { Amplify } from "aws-amplify";
import { fetchAuthSession, signInWithRedirect } from "aws-amplify/auth";
import "aws-amplify/auth/enable-oauth-listener";
import { cognitoUserPoolsTokenProvider } from "aws-amplify/auth/cognito";
import { CookieStorage } from "aws-amplify/utils";
import { auth } from "aws-iot-device-sdk-v2/dist/browser";
import { nothing } from "lit-html";
import * as v from "valibot";

import { AwsTransport, sigv4ClientConfig } from "../transport/aws.ts";
import { BROKER_STALE_AFTER_MS } from "./env.ts";
import type { Target } from "./types.ts";

// written by the cdk stack next to index.html, so one build serves any deployment.
const RuntimeConfig = v.object({
  region: v.string(),
  userPoolId: v.string(),
  userPoolClientId: v.string(),
  identityPoolId: v.string(),
  oauthDomain: v.string(),
  redirectUrl: v.pipe(v.string(), v.url()),
  iotEndpoint: v.string(),
  deviceId: v.string(),
  dashboardPolicyName: v.string(),
});

type RuntimeConfig = v.InferOutput<typeof RuntimeConfig>;

interface Session {
  identityId: string;
  credentials: {
    accessKeyId: string;
    secretAccessKey: string;
    sessionToken: string;
  };
}

class AmplifyCredentials extends auth.CredentialsProvider {
  readonly #region: string;
  #session: Session | undefined;

  constructor(region: string, session: Session) {
    super();
    this.#region = region;
    this.#session = session;
  }

  override getCredentials(): auth.AWSCredentials | undefined {
    const credentials = this.#session?.credentials;

    if (credentials === undefined) {
      return undefined;
    }

    return {
      aws_region: this.#region,
      aws_access_id: credentials.accessKeyId,
      aws_secret_key: credentials.secretAccessKey,
      aws_sts_token: credentials.sessionToken,
    };
  }

  // rejects once cognito can no longer refresh the login, so the transport can ask for a new one.
  override async refreshCredentials(): Promise<void> {
    const session = await currentSession();

    if (session === undefined) {
      throw new Error("session expired");
    }

    this.#session = session;
  }
}

export async function createTarget(): Promise<Target> {
  const config = v.parse(RuntimeConfig, await (await fetch("./config.json")).json());

  configureAmplify(config);

  const session = await currentSession();

  if (session === undefined) {
    await signInWithRedirect();

    return new Promise(() => undefined);
  }

  await attachDashboardPolicy(config, session);

  const credentials = new AmplifyCredentials(config.region, session);

  return {
    transport: new AwsTransport({
      deviceId: config.deviceId,
      identityId: session.identityId,
      clientConfig: (clientId) => sigv4ClientConfig(config.iotEndpoint, config.region, credentials, clientId),
      refreshCredentials: () => credentials.refreshCredentials(),
    }),
    staleAfterMs: BROKER_STALE_AFTER_MS,
    extras: () => nothing,
  };
}

function configureAmplify(config: RuntimeConfig): void {
  Amplify.configure({
    Auth: {
      Cognito: {
        userPoolId: config.userPoolId,
        userPoolClientId: config.userPoolClientId,
        identityPoolId: config.identityPoolId,
        loginWith: {
          oauth: {
            domain: config.oauthDomain,
            scopes: ["openid", "email", "profile"],
            redirectSignIn: [config.redirectUrl],
            redirectSignOut: [config.redirectUrl],
            responseType: "code",
          },
        },
      },
    },
  });
  // tokens live in cookies rather than the default localStorage.
  cognitoUserPoolsTokenProvider.setKeyValueStorage(new CookieStorage({ secure: true, sameSite: "strict" }));
}

// identity pool credentials are temporary, so a usable session always has a session token.
async function currentSession(): Promise<Session | undefined> {
  const { identityId, credentials } = await fetchAuthSession();

  if (identityId === undefined || credentials?.sessionToken === undefined) {
    return undefined;
  }

  const { accessKeyId, secretAccessKey, sessionToken } = credentials;

  return { identityId, credentials: { accessKeyId, secretAccessKey, sessionToken } };
}

// aws checks an iot policy on the cognito identity as well as the iam role; attaching the same
// policy again is accepted, so this runs on every start.
async function attachDashboardPolicy(config: RuntimeConfig, session: Session): Promise<void> {
  const client = new IoTClient({ region: config.region, credentials: session.credentials });

  await client.send(new AttachPolicyCommand({ policyName: config.dashboardPolicyName, target: session.identityId }));
}
