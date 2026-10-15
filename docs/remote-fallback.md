# Remote fallback: EMQX Serverless and Cloudflare

Use this when the AWS remote path should stop, for example when the AWS budget ends. The node
needs a new `cloud.json` and the dashboard a different build. There is no reflash and no code
change. The local dashboard keeps working throughout.

Create the EMQX account only when you need it. A free deployment pauses after 5 days with no
clients; check the current limits in the EMQX console before switching.

## 1. EMQX deployment, users and ACL rules

1. Create an EMQX Serverless deployment (free tier).
2. On its Overview page, note the MQTT over TLS address (port 8883), the WebSocket over TLS
   address (port 8084) and download the CA certificate.
3. Under Access Control → Authentication, add two users:
   - `node-<device id>` with a long random password, for the node;
   - `dashboard` with a password for the people who use the remote dashboard.
4. Under Access Control → Authorization, add these rules, then a last rule that denies all
   users on `#`:

   | Username | Allow publish | Allow subscribe |
   |---|---|---|
   | `node-<id>` | `lumen/<id>/telemetry`, `status`, `energy`, `availability`, `ack/+` | `lumen/<id>/command/+` |
   | `dashboard` | `lumen/<id>/command/+` | `lumen/<id>/availability`, `status`, `telemetry`, `energy`, `ack/+` |

## 2. Node cloud config

Write `provision/cloud.json` (the folder is gitignored):

```json
{
  "endpoint": "<deployment>.emqxsl.com",
  "port": 8883,
  "ca_pem": "<contents of the downloaded CA certificate>",
  "auth": { "type": "password", "username": "node-<id>", "password": "<node password>" }
}
```

Then build and upload the filesystem image over USB:

```sh
make fs-image
pio run -e lumen_esp32 --target uploadfs
```

## 3. Dashboard

Put the broker address in `dashboard/.env.emqx.local`:

```sh
VITE_DEVICE_ID=lumen-<id>
VITE_BROKER_URL=wss://<deployment>.emqxsl.com:8084/mqtt
```

Then deploy to Cloudflare Workers static assets:

```sh
make deploy-fallback
```

## 4. Remove the AWS path

```sh
make teardown DEVICE_ID=lumen-<id> BUDGET_EMAIL=<email>
```

## 5. Tell the users

Send the new Cloudflare address to the 1–3 users. The CloudFront address stops working.
