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