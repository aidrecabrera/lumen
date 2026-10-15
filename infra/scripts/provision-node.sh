#!/usr/bin/env bash
# creates the device certificate outside cdk, so the private key never enters cloudformation
# state, then writes the node's cloud.json into a gitignored folder.
set -euo pipefail

device_id=$1
out_dir=$2
here=$(dirname "$0")

mkdir -p "$out_dir"
chmod 700 "$out_dir"

cert_arn=$(aws iot create-keys-and-certificate --set-as-active \
  --certificate-pem-outfile "$out_dir/device.pem.crt" \
  --private-key-outfile "$out_dir/private.pem.key" \
  --query certificateArn --output text)
echo "$cert_arn" > "$out_dir/certificate-arn"

aws iot attach-thing-principal --thing-name "$device_id" --principal "$cert_arn"
aws iot attach-policy --policy-name "$("$here/stack-output.sh" NodePolicyName)" --target "$cert_arn"
curl -fsS https://www.amazontrust.com/repository/AmazonRootCA1.pem -o "$out_dir/AmazonRootCA1.pem"

python3 - "$out_dir" "$("$here/stack-output.sh" IotEndpoint)" <<'PY'
import json, pathlib, sys

out_dir, endpoint = pathlib.Path(sys.argv[1]), sys.argv[2]
config = {
    "endpoint": endpoint,
    "port": 8883,
    "ca_pem": (out_dir / "AmazonRootCA1.pem").read_text(),
    "auth": {
        "type": "x509",
        "cert_pem": (out_dir / "device.pem.crt").read_text(),
        "key_pem": (out_dir / "private.pem.key").read_text(),
    },
}
(out_dir / "cloud.json").write_text(json.dumps(config))
PY

chmod 600 "$out_dir"/*
echo "wrote $out_dir/cloud.json for $device_id"
