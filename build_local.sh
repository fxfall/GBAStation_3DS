#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:/mingw64/bin:/ucrt64/bin:${PATH:-}"

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
if [[ "$SCRIPT_DIR" == "${BASH_SOURCE[0]}" ]]; then
    SCRIPT_DIR=.
fi
ROOT=$(cd -- "$SCRIPT_DIR" && pwd)

SWITCHVK_HELPER="$ROOT/../switchVK/switchvk-version.sh"
if [[ -f "$SWITCHVK_HELPER" ]]; then
    # shellcheck disable=SC1090
    source "$SWITCHVK_HELPER"
fi

VARIANT=release
INCREMENTAL=0
REBUILD_SWITCHVK=0
SWITCHVK_JOBS=${SWITCHVK_JOBS:-$(switchvk_default_jobs 2>/dev/null || echo 1)}
while (( $# > 0 )); do
    case "$1" in
        --diagnostic)
            VARIANT=diagnostic
            shift
            ;;
        --incremental)
            INCREMENTAL=1
            shift
            ;;
        --rebuild-switchvk)
            REBUILD_SWITCHVK=1
            shift
            ;;
        -j|--jobs)
            if (( $# < 2 )) || [[ ! "$2" =~ ^[1-9][0-9]*$ ]]; then
                echo "ERROR: --jobs requires a positive integer" >&2
                exit 2
            fi
            SWITCHVK_JOBS=$2
            shift 2
            ;;
        -h|--help)
            echo "Usage: bash build_local.sh [--incremental] [--diagnostic] [--rebuild-switchvk] [-j JOBS]"
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

if [[ ! -f "$ROOT/externals/dynarmic/CMakeLists.txt" ]]; then
    echo "ERROR: required source dependencies are missing; initialize submodules first" >&2
    exit 1
fi

if [[ -z "${SWITCH_NVK_ROOT:-}" ]]; then
    for sibling in "$ROOT/../switchVK" "$ROOT/../switch-nvk"; do
        [[ -d "$sibling" ]] || continue
        driver_script="$sibling/build_local.sh"
        candidate=
        if declare -F switchvk_find_sdk >/dev/null 2>&1; then
            candidate=$(switchvk_find_sdk "$sibling" "$VARIANT" || true)
        fi

        if (( REBUILD_SWITCHVK )) || [[ -z "$candidate" ]]; then
            if [[ -f "$driver_script" ]]; then
                driver_args=(-j "$SWITCHVK_JOBS")
                if [[ "$VARIANT" == diagnostic ]]; then
                    driver_args+=(--diagnostic)
                fi
                if (( REBUILD_SWITCHVK )); then
                    driver_args+=(--rebuild)
                fi
                echo "Building sibling switchVK SDK first ..."
                bash "$driver_script" "${driver_args[@]}"
                if declare -F switchvk_find_sdk >/dev/null 2>&1; then
                    candidate=$(switchvk_find_sdk "$sibling" "$VARIANT" || true)
                fi
            fi
        fi

        if [[ -n "$candidate" ]]; then
            SWITCH_NVK_ROOT="$candidate"
            export SWITCH_NVK_ROOT
            break
        fi
    done
fi

if [[ -z "${SWITCH_NVK_ROOT:-}" ]] ||
   [[ ! -f "$SWITCH_NVK_ROOT/lib/libvulkan.a" ]] ||
   [[ ! -f "$SWITCH_NVK_ROOT/include/vulkan/vulkan.h" ]] ||
   [[ ! -f "$SWITCH_NVK_ROOT/include/vk_video/vulkan_video_codec_h264std.h" ]]; then
    echo "ERROR: no complete sibling switchVK SDK was found" >&2
    echo "Keep switchVK and GBAStation_3DS in the same parent directory," >&2
    echo "or export SWITCH_NVK_ROOT before running this script." >&2
    exit 1
fi

build_args=()
if (( ! INCREMENTAL )); then
    build_args+=(clean)
fi
build_args+=("$VARIANT")

echo "Using switchVK SDK: $SWITCH_NVK_ROOT"
bash "$ROOT/build_switch_nro.sh" "${build_args[@]}"

if [[ "$VARIANT" == diagnostic ]]; then
    output="$ROOT/GBAStation3DSStub-diagnostic.nro"
else
    output="$ROOT/GBAStation3DSStub.nro"
fi
if [[ ! -f "$output" ]]; then
    echo "ERROR: build completed without producing $output" >&2
    exit 1
fi

echo "OK: GBAStation 3DS local build completed"
sha256sum "$output"
