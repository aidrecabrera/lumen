SRCS := $(wildcard src/*.cpp src/*.h include/*.h)

.PHONY: format
format:
	clang-format -i $(SRCS)