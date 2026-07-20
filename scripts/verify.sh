#!/bin/sh

# CPSSim's public verification selector. CMake/CTest retain all build and test
# logic; this script only selects presets, labels, and formatting operations.

set -eu

MODULE_LABELS="core config kernel scheduler network functional fmi bosch conformance gui cli"

usage() {
    cat <<'EOF'
Usage: scripts/verify.sh [MODE]

Modes:
  quick                 Formatting check, Debug build, and all Debug tests
  all                   Build and run every normal Debug test
  full                  Formatting, Debug, ASan/UBSan, and Release verification
  module LABEL          Run Debug tests carrying one maintained module label
  list-modules          List maintained module labels
  format-check          Check C++ formatting without changing files
  format-apply          Apply C++ formatting
EOF
}

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Verification error: required tool '$1' is not available." >&2
        exit 2
    fi
}

cpp_sources() {
    find src apps tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print | sort
}

format_check() {
    require_tool clang-format
    # shellcheck disable=SC2046
    clang-format --dry-run --Werror $(cpp_sources)
}

format_apply() {
    require_tool clang-format
    # shellcheck disable=SC2046
    clang-format -i $(cpp_sources)
}

configure_build_test() {
    preset=$1
    require_tool cmake
    require_tool ctest
    cmake --preset "$preset"
    cmake --build --preset "$preset"
    ctest --preset "$preset" --output-on-failure
}

quick() {
    echo "== Formatting check =="
    format_check
    echo "== Debug build and tests =="
    configure_build_test dev
}

all_tests() {
    echo "== All Debug tests =="
    configure_build_test dev
}

full() {
    echo "== Formatting check =="
    format_check
    echo "== Debug build and tests =="
    configure_build_test dev
    echo "== ASan/UBSan build and tests =="
    configure_build_test asan
    echo "== Release build and tests =="
    configure_build_test release

    if command -v clang++ >/dev/null 2>&1; then
        echo "== Optional Clang build and tests =="
        configure_build_test clang
    else
        echo "Optional Clang verification skipped: clang++ is not available."
    fi

    if command -v clang-tidy >/dev/null 2>&1; then
        echo "== Optional clang-tidy build and tests =="
        configure_build_test tidy
    else
        echo "Optional clang-tidy verification skipped: clang-tidy is not available."
    fi
}

list_modules() {
    for label in $MODULE_LABELS; do
        echo "$label"
    done
}

known_module() {
    requested=$1
    for label in $MODULE_LABELS; do
        if [ "$requested" = "$label" ]; then
            return 0
        fi
    done
    return 1
}

module_test() {
    label=$1
    if ! known_module "$label"; then
        echo "Verification error: unknown test module '$label'." >&2
        echo "Available modules:" >&2
        list_modules >&2
        exit 2
    fi

    require_tool cmake
    require_tool ctest
    cmake --preset dev
    cmake --build --preset dev
    ctest --test-dir build/dev --output-on-failure --no-tests=error -L "^${label}$"
}

read_choice() {
    prompt=$1
    printf '%s' "$prompt"
    if ! IFS= read -r choice; then
        echo
        echo "Verification menu closed at end of input."
        exit 0
    fi
}

interactive_module() {
    echo "Available test modules:"
    list_modules | sed 's/^/  /'
    read_choice "Module: "
    module_test "$choice"
}

interactive_format() {
    cat <<'EOF'

Formatting

1. Check formatting
2. Apply formatting
3. Back
EOF
    read_choice "Selection: "
    case $choice in
        1) format_check ;;
        2) format_apply ;;
        3) return ;;
        *) echo "Verification error: unknown formatting selection '$choice'." >&2; exit 2 ;;
    esac
}

interactive() {
    while :; do
        cat <<'EOF'
CPSSim verification

1. Quick verification
2. Test a specific module
3. Run all tests
4. Run full verification
5. Formatting
6. List test modules
7. Exit
EOF
        read_choice "Selection: "
        case $choice in
            1) quick; return ;;
            2) interactive_module; return ;;
            3) all_tests; return ;;
            4) full; return ;;
            5) interactive_format ;;
            6) list_modules ;;
            7) return ;;
            *) echo "Unknown selection '$choice'. Choose 1 through 7." >&2 ;;
        esac
        echo
    done
}

if [ "$#" -eq 0 ]; then
    if [ -t 0 ]; then
        interactive
    else
        echo "Non-interactive input detected; running quick verification."
        quick
    fi
    exit 0
fi

mode=$1
shift
case $mode in
    quick)
        [ "$#" -eq 0 ] || { usage >&2; exit 2; }
        quick
        ;;
    all)
        [ "$#" -eq 0 ] || { usage >&2; exit 2; }
        all_tests
        ;;
    full)
        [ "$#" -eq 0 ] || { usage >&2; exit 2; }
        full
        ;;
    module)
        [ "$#" -eq 1 ] || { usage >&2; exit 2; }
        module_test "$1"
        ;;
    list-modules)
        [ "$#" -eq 0 ] || { usage >&2; exit 2; }
        list_modules
        ;;
    format-check)
        [ "$#" -eq 0 ] || { usage >&2; exit 2; }
        format_check
        ;;
    format-apply)
        [ "$#" -eq 0 ] || { usage >&2; exit 2; }
        format_apply
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        echo "Verification error: unknown mode '$mode'." >&2
        usage >&2
        exit 2
        ;;
esac
