#!/usr/bin/env bash
# shadert0y — Linux installer
set -euo pipefail

PLUGIN_FILE="shadert0y.so"
XML_FILE="shadert0y.xml"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SELF="$SCRIPT_DIR/$(basename -- "${BASH_SOURCE[0]}")"

if [ -z "${PLUGIN_DIR:-}" ]; then
    if [ -d /usr/lib/frei0r-1 ]; then
        PLUGIN_DIR="/usr/lib/frei0r-1"
    elif [ -d /usr/lib64/frei0r-1 ]; then
        PLUGIN_DIR="/usr/lib64/frei0r-1"
    else
        PLUGIN_DIR="/usr/lib/frei0r-1"
    fi
fi

EFFECTS_DIR="${EFFECTS_DIR:-/usr/share/kdenlive/effects}"


# Need root to write into system directories
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        echo "Administrator privileges required; re-running with sudo."
        exec sudo env PLUGIN_DIR="$PLUGIN_DIR" EFFECTS_DIR="$EFFECTS_DIR" "$SELF" "$@"
    else
        echo "Error: must run as root (sudo not found). Try: sudo $0" >&2
        exit 1
    fi
fi

have_lib() {
    command -v ldconfig >/dev/null 2>&1 || return 1
    ldconfig -p 2>/dev/null | grep -q "$1"
}

install_deps() {
    if have_lib "libEGL.so.1" && have_lib "libGL.so.1"; then
        echo "OpenGL/EGL runtime libraries already present; skipping dependency install."
        return 0
    fi

    echo "OpenGL/EGL runtime libraries missing; attempting automatic install..."

    local ok=0

    if command -v apt-get >/dev/null 2>&1; then
        if apt-get update -y && apt-get install -y libegl1 libgl1; then
            ok=1
        fi
    elif command -v dnf >/dev/null 2>&1; then
        if dnf install -y mesa-libEGL mesa-libGL; then
            ok=1
        fi
    elif command -v yum >/dev/null 2>&1; then
        if yum install -y mesa-libEGL mesa-libGL; then
            ok=1
        fi
    elif command -v pacman >/dev/null 2>&1; then
        if pacman -S --needed --noconfirm libglvnd; then
            ok=1
        fi
    elif command -v zypper >/dev/null 2>&1; then
        if zypper --non-interactive install libEGL1 libGL1; then
            ok=1
        fi
    else
        echo "Warning: no supported package manager found." >&2
    fi

    if [ "$ok" -ne 1 ]; then
        echo "Warning: could not auto-install dependencies." >&2
        echo "If the plugin fails to load, install libEGL and libGL manually." >&2
    fi

    return 0
}


# Locate the .so
SO=""
for candidate in \
    "$SCRIPT_DIR/$PLUGIN_FILE" \
    "$SCRIPT_DIR/build/$PLUGIN_FILE"; do
    if [ -f "$candidate" ]; then
        SO="$candidate"
        break
    fi
done

if [ -z "$SO" ]; then
    echo "Error: $PLUGIN_FILE not found next to the installer." >&2
    exit 1
fi

if command -v file >/dev/null 2>&1; then
    case "$(uname -m)" in
        x86_64)
            expect="x86-64"
            ;;
        aarch64)
            expect="aarch64"
            ;;
        *)
            expect=""
            ;;
    esac

    if [ -n "$expect" ]; then
        info="$(file -b "$SO" 2>/dev/null || true)"
        if ! printf '%s' "$info" | grep -qi "$expect"; then
            echo "Warning: $PLUGIN_FILE may not match this CPU ($(uname -m))." >&2
            echo "Detected: $info" >&2
        fi
    fi
fi


# Install
install_deps

echo "Plugin dir:  $PLUGIN_DIR"
echo "Effects dir: $EFFECTS_DIR"
echo

echo "Installing plugin: $SO -> $PLUGIN_DIR/$PLUGIN_FILE"
mkdir -p "$PLUGIN_DIR"
install -m755 "$SO" "$PLUGIN_DIR/$PLUGIN_FILE"

if [ -f "$SCRIPT_DIR/$XML_FILE" ]; then
    echo "Installing XML:    $SCRIPT_DIR/$XML_FILE -> $EFFECTS_DIR/$XML_FILE"
    mkdir -p "$EFFECTS_DIR"
    install -m644 "$SCRIPT_DIR/$XML_FILE" "$EFFECTS_DIR/$XML_FILE"
else
    echo "WARNING: $XML_FILE not found; skipping. Please run uninstaller and check the GitHub repo for the XML file to generate Kdenlive UI."
fi

echo
echo "Installed. Restart Kdenlive and search effects for Shadert0y."
