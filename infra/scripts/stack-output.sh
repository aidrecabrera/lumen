#!/usr/bin/env bash
# prints one LumenStack output value.
set -euo pipefail

aws cloudformation describe-stacks --stack-name LumenStack \
  --query "Stacks[0].Outputs[?OutputKey=='$1'].OutputValue" --output text
