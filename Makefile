.DEFAULT_GOAL := build

MAKE_DEV_DIR := build/make-dev
MAKE_RELEASE_DIR := build/make-release
MAKE_ASAN_DIR := build/make-asan
MAKE_GUI_DIR := build/make-gui
CXX_SOURCES := $(shell find src apps tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print | sort)
BOSCH_EXAMPLE_DIR ?= examples/example_v_10
BOSCH_SCENARIO ?= shared_cloud
BOSCH_STOP_TICK ?=
BOSCH_FMU_LIBRARY := ./$(MAKE_DEV_DIR)/LateralMotionControl.so

.PHONY: \
	build \
	debug \
	release \
	configure \
	test \
	conformance \
	fmi-test \
	functional-test \
	bosch-example \
	bosch-examples \
	gui \
	run-gui \
	run \
	asan \
	format \
	format-check \
	clean \
	help

configure:
	cmake --preset make-dev

build: configure
	$(MAKE) --no-print-directory -C $(MAKE_DEV_DIR)

debug: build

release:
	cmake --preset make-release
	$(MAKE) --no-print-directory -C $(MAKE_RELEASE_DIR)
	ctest --preset make-release --output-on-failure

test: build
	ctest --preset make-dev --output-on-failure

conformance: build
	./$(MAKE_DEV_DIR)/cpssim_bosch_conformance

fmi-test: build
	ctest --test-dir $(MAKE_DEV_DIR) --output-on-failure -R '^fmi2:'

functional-test: build
	ctest --test-dir $(MAKE_DEV_DIR) --output-on-failure -R '^functional:'

bosch-example: build
	./$(MAKE_DEV_DIR)/cpssim_bosch_example "$(BOSCH_EXAMPLE_DIR)" \
		"$(BOSCH_FMU_LIBRARY)" "$(BOSCH_SCENARIO)" $(BOSCH_STOP_TICK)

bosch-examples: build
	./$(MAKE_DEV_DIR)/cpssim_bosch_example examples/example_v_10 \
		"$(BOSCH_FMU_LIBRARY)" "$(BOSCH_SCENARIO)"
	./$(MAKE_DEV_DIR)/cpssim_bosch_example examples/example_v_12_5 \
		"$(BOSCH_FMU_LIBRARY)" "$(BOSCH_SCENARIO)"
	./$(MAKE_DEV_DIR)/cpssim_bosch_example examples/example_v_15 \
		"$(BOSCH_FMU_LIBRARY)" "$(BOSCH_SCENARIO)"

gui:
	cmake --preset make-gui
	$(MAKE) --no-print-directory -C $(MAKE_GUI_DIR) cpssim_gui cpssim_tests

run-gui: gui
	./$(MAKE_GUI_DIR)/cpssim_gui

run: build
	./$(MAKE_DEV_DIR)/cpssim_cli

asan:
	cmake --preset make-asan
	$(MAKE) --no-print-directory -C $(MAKE_ASAN_DIR)
	ctest --preset make-asan --output-on-failure

format:
	clang-format -i $(CXX_SOURCES)

format-check:
	clang-format --dry-run --Werror $(CXX_SOURCES)

clean:
	cmake -E rm -rf $(MAKE_DEV_DIR) $(MAKE_RELEASE_DIR) $(MAKE_ASAN_DIR) $(MAKE_GUI_DIR)

help:
	@echo "CPSSim Make targets:"
	@echo "  make              Configure and build the GCC Debug tree"
	@echo "  make debug        Explicit alias for the normal Debug build"
	@echo "  make release      Build and test the optimized Release tree"
	@echo "  make test         Build and run the smoke tests"
	@echo "  make conformance  Compare both T12 MATLAB timing references"
	@echo "  make fmi-test     Run only the T15 Bosch FMI lifecycle tests"
	@echo "  make functional-test Run T16 mock, Bosch, and replay tests"
	@echo "  make bosch-example Run one Bosch example dataset through CPSSim"
	@echo "  make bosch-examples Run all three full Bosch example datasets"
	@echo "  make gui          Build the optional T17 Dear ImGui executable"
	@echo "  make run-gui      Build and launch the T17 GUI"
	@echo "  make run          Build and run cpssim_cli"
	@echo "  make asan         Build and test with ASan/UBSan"
	@echo "  make format       Format project C++ files"
	@echo "  make format-check Check project C++ formatting"
	@echo "  make clean        Remove only Makefile-generator build trees"
	@echo "  make help         Show this help"
