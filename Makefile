SRCS := $(wildcard src/*.cpp src/*.h include/*.h)

.PHONY: format
format:
	clang-format -i $(SRCS)

.PHONY: check-format
check-format:
	clang-format --dry-run --Werror $(SRCS)

.PHONY: analyze
analyze:
	pio check -e lumen_esp32 --severity=high

.PHONY: build
build:
	pio run -e lumen_esp32

.PHONY: release
release:
	pio run -e lumen_esp32_release

.PHONY: clean
clean:
	pio run --target clean

.PHONY: flash
flash:
	pio run -e lumen_esp32 --target upload

.PHONY: monitor
monitor:
	pio device monitor --baud 115200

.PHONY: flash-monitor
flash-monitor: flash monitor

SLIDES_SRC := docs/slides/lumen.md
SLIDES_OUT := docs

.PHONY: slides slides-html slides-pdf

slides: slides-html slides-pdf

slides-html:
	bunx --bun @marp-team/marp-cli $(SLIDES_SRC) --html --allow-local-files -o $(SLIDES_OUT)/index.html

slides-pdf:
	bunx --bun @marp-team/marp-cli $(SLIDES_SRC) --pdf --allow-local-files -o $(SLIDES_OUT)/lumen.pdf
# ---- dashboard, cloud and node provisioning ----
# make setup DEVICE_ID=lumen-aabbccddeeff BUDGET_EMAIL=you@example.com
# DEVICE_ID is printed at boot ("config loaded for lumen-...").

PROVISION_DIR := provision
CDK_PARAMETERS = --parameters DeviceId=$(DEVICE_ID) --parameters BudgetEmail=$(BUDGET_EMAIL)

.PHONY: require-device require-budget setup deploy fs-image provision-node teardown add-user deploy-fallback

require-device:
	@test -n "$(DEVICE_ID)" || (echo "set DEVICE_ID=lumen-<mac>" && exit 1)

require-budget:
	@test -n "$(BUDGET_EMAIL)" || (echo "set BUDGET_EMAIL=you@example.com" && exit 1)

setup: require-device require-budget
	cd infra && npm ci && npx cdk bootstrap
	$(MAKE) deploy provision-node flash

deploy: require-device require-budget
	cd dashboard && npm ci && npm run build:aws
	cd infra && npm ci && npx cdk deploy --require-approval never $(CDK_PARAMETERS)

# uploadfs replaces the whole ffat partition, so the image always holds the dashboard and the
# cloud config together. the web server serves the .gz files directly.
fs-image:
	cd dashboard && npm ci && npm run build:local
	rm -rf data && mkdir -p data/www
	cp -r dashboard/dist-node/. data/www/
	find data/www -type f -exec gzip -9 {} \;
	if [ -f $(PROVISION_DIR)/cloud.json ]; then cp $(PROVISION_DIR)/cloud.json data/cloud.json; fi

provision-node: require-device
	infra/scripts/provision-node.sh $(DEVICE_ID) $(PROVISION_DIR)
	$(MAKE) fs-image
	pio run -e lumen_esp32 --target uploadfs

teardown: require-device
	infra/scripts/teardown.sh $(PROVISION_DIR) $(DEVICE_ID)

add-user:
	@test -n "$(EMAIL)" || (echo "set EMAIL=person@example.com" && exit 1)
	infra/scripts/add-user.sh $(EMAIL)

deploy-fallback:
	cd dashboard && npm ci && npm run build:emqx && npx wrangler@4.151.0 deploy
