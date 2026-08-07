#!/usr/bin/env bash
# shadert0y — Linux uninstaller

set -euo pipefail

PLUGIN_FILE="shadert0y.so"
XML_FILE="shadert0y.xml"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SELF="$SCRIPT_DIR/$(basename -- "${BASH_SOURCE[0]}")"

EFFECTS_DIR="${EFFECTS_DIR:-/usr/share/kdenlive/effects}"

# Need root for system directories
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        echo "Administrator privileges required; re-running with sudo."
        exec sudo env PLUGIN_DIR="${PLUGIN_DIR:-}" EFFECTS_DIR="$EFFECTS_DIR" "$SELF" "$@"
    else
        echo "Error: must run as root (sudo not found). Try: sudo $0" >&2
        exit 1
    fi
fi

# If PLUGIN_DIR is not overridden, check both common system locations.
if [ -n "${PLUGIN_DIR:-}" ]; then
    plugin_dirs=("$PLUGIN_DIR")
else
    plugin_dirs=(
        "/usr/lib/frei0r-1"
        "/usr/lib64/frei0r-1"
    )
fi

removed_plugin=0

for dir in "${plugin_dirs[@]}"; do
    if [ -f "$dir/$PLUGIN_FILE" ]; then
        echo "Removing plugin: $dir/$PLUGIN_FILE"
        rm -f "$dir/$PLUGIN_FILE"
        removed_plugin=1
    fi
done

if [ "$removed_plugin" -eq 0 ]; then
    echo "No $PLUGIN_FILE found in: ${plugin_dirs[*]}"
fi

# Remove XML
if [ -f "$EFFECTS_DIR/$XML_FILE" ]; then
    echo "Removing XML: $EFFECTS_DIR/$XML_FILE"
    rm -f "$EFFECTS_DIR/$XML_FILE"
else
    echo "No $XML_FILE found in $EFFECTS_DIR"
fi

echo
echo "Uninstalled."
echo "Restart Kdenlive."
