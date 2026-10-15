# Lumen dashboard

One web app, four builds. The build target picks the transport; there is no runtime switch.

| Script | Mode | Transport | Goes to |
|---|---|---|---|
| `npm run build:local` | `node` | WebSocket to the node | `data/www` on the node (`make fs-image`) |
| `npm run build:aws` | `aws` | AWS IoT Core over WSS (Cognito login) | S3 and CloudFront (`make deploy`) |
| `npm run build:emqx` | `emqx` | EMQX over WSS (broker login) | Cloudflare Workers (`make deploy-fallback`) |
| `npm run dev` | `dev` | Mosquitto over WebSocket | your laptop |
| `npm run dev:fake` | `fake` | a simulated node with prototype controls | your laptop |

Checks: `npm run typecheck` and `npm test`. The contract fixtures in `../test/fixtures/contract`
come from `npm run fixtures`; the firmware native tests decode the same files.

## Development against the real node

The node's remote path speaks MQTT, so a Mosquitto broker on a laptop stands in for the cloud.

1. Mosquitto 2.x listens on localhost only and refuses anonymous clients until configured.
   For a trusted home network, `mosquitto.conf`:

   ```conf
   listener 1883
   listener 9001
   protocol websockets
   allow_anonymous true
   ```

2. Point the node at it with `provision/cloud.json` (plain TCP is accepted only with password
   auth, and only for development):

   ```json
   {
     "endpoint": "192.168.1.50",
     "port": 1883,
     "tls": false,
     "auth": { "type": "password", "username": "node", "password": "dev-only" }
   }
   ```

   Then `make fs-image` and `pio run -e lumen_esp32 --target uploadfs`.

3. Copy `.env.example` to `.env.dev.local`, set `VITE_DEVICE_ID` and
   `VITE_BROKER_URL=ws://192.168.1.50:9001`, and run `npm run dev`.

On the broker path the node sends telemetry every 30 s, so the dashboard updates more slowly
than on the local path, which sends every reading (2 s).
