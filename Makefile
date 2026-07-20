.DEFAULT_GOAL := all

NORMAL_PRESET := make-dev
NORMAL_BUILD_DIR := build/make-dev
GENERATED_BUILD_DIRS := \
	build/make-dev \
	build/dev \
	build/release \
	build/asan \
	build/clang \
	build/tidy \
	build/gui \
	build/make-release \
	build/make-asan \
	build/make-gui \
	build/_deps

.PHONY: all run-cli run-gui test clean help

all:
	cmake --preset $(NORMAL_PRESET)
	cmake --build --preset $(NORMAL_PRESET) \
		--target cpssim_cli cpssim_gui cpssim_bosch_fmu_linux

run-cli:
	cmake --preset $(NORMAL_PRESET)
	cmake --build --preset $(NORMAL_PRESET) \
		--target cpssim_cli cpssim_bosch_fmu_linux
	./$(NORMAL_BUILD_DIR)/cpssim_cli

run-gui:
	cmake --preset $(NORMAL_PRESET)
	cmake --build --preset $(NORMAL_PRESET) --target cpssim_gui
	./$(NORMAL_BUILD_DIR)/cpssim_gui

test:
	@./scripts/verify.sh

clean:
	cmake -E rm -rf $(GENERATED_BUILD_DIRS)

help:
	@echo "CPSSim commands:"
	@echo "  make          Build CPSSim and its normal user applications"
	@echo "  make run-cli  Launch the interactive terminal interface"
	@echo "  make run-gui  Launch the graphical workbench"
	@echo "  make test     Open the verification interface"
	@echo "  make clean    Remove documented generated build directories"
	@echo "  make help     Show this help"
