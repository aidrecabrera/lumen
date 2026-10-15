#!/usr/bin/env bash
# self sign-up is off; cognito emails the new user a temporary password.
set -euo pipefail

email=$1

aws cognito-idp admin-create-user \
  --user-pool-id "$("$(dirname "$0")/stack-output.sh" UserPoolId)" \
  --username "$email" \
  --user-attributes Name=email,Value="$email" Name=email_verified,Value=true \
  --desired-delivery-mediums EMAIL
