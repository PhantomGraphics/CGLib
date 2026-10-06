#!/usr/bin/env bash
# Linux/macOS counterpart of run_install_consumer.ps1.
#
# Installs CGLib (CPU components, or also VulkanGraphics with --vulkan), MOVES the install
# prefix, deletes the build tree, then builds and runs an external consumer that only uses
# find_package(CGLib). Proves the package does not depend on the source or build tree.
#
#   tests/run_install_consumer.sh [--config Debug|Release] [--workdir DIR] [--vulkan]
set -euo pipefail

config=Debug
workdir=""
vulkan=OFF
while [ $# -gt 0 ]; do
    case "$1" in
        --config)  config="$2"; shift 2 ;;
        --workdir) workdir="$2"; shift 2 ;;
        --vulkan)  vulkan=ON; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ -z "$workdir" ]; then workdir="$(mktemp -d "${TMPDIR:-/tmp}/cglib_install_check.XXXXXX")"; fi
rm -rf "$workdir"
mkdir -p "$workdir"

gen=()
if command -v ninja >/dev/null 2>&1; then gen=(-G Ninja); fi

build="$workdir/build"
prefix="$workdir/prefix"
moved="$workdir/moved_prefix"
cons="$workdir/consumer_build"

cmake -S "$root" -B "$build" "${gen[@]}" -DCMAKE_BUILD_TYPE="$config" \
    -DCGLIB_ENABLE_VULKAN="$vulkan" -DCGLIB_BUILD_TESTING=OFF -DCGLIB_BUILD_VIEWERS=OFF
cmake --build "$build" -j
cmake --install "$build" --prefix "$prefix"

# The install tree must not mention the source or build directory.
if grep -rIlF --include='*.cmake' -e "$root" -e "$build" "$prefix"; then
    echo "install tree references the source/build tree" >&2
    exit 1
fi

# Relocate the prefix and remove the original build tree so nothing can fall back to it.
mv "$prefix" "$moved"
rm -rf "$build"

cmake -S "$root/tests/install_consumer" -B "$cons" "${gen[@]}" -DCMAKE_BUILD_TYPE="$config" \
    -DCMAKE_PREFIX_PATH="$moved" -DCGLIB_CONSUMER_VULKAN="$vulkan"
cmake --build "$cons" -j
"$cons/cglib_install_consumer"
echo "install consumer check: PASS"
