#!/usr/bin/env bash
# the device certificate was made outside cdk, so it goes first; then cdk destroy removes the rest.
set -euo pipefail

out_dir=$1
device_id=$2
here=$(dirname "$0")

if [ -f "$out_dir/certificate-arn" ]; then
  cert_arn=$(cat "$out_dir/certificate-arn")
  cert_id=${cert_arn##*/}
  aws iot detach-policy --policy-name "$("$here/stack-output.sh" NodePolicyName)" --target "$cert_arn"
  aws iot detach-thing-principal --thing-name "$device_id" --principal "$cert_arn"
  aws iot update-certificate --certificate-id "$cert_id" --new-status INACTIVE
  aws iot delete-certificate --certificate-id "$cert_id"
  rm -f "$out_dir/certificate-arn"
fi

cd "$here/.."
npx cdk destroy --force
