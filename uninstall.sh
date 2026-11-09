#!/usr/bin/env bash
set -euo pipefail

APP_NAME="avora"
BIN_TARGET="aed_tree_debugger"

if [ "$(id -u)" -eq 0 ]; then
    PREFIX="${PREFIX:-/usr/local}"
    BIN_DIR="$PREFIX/bin"
    SHARE_DIR="$PREFIX/share/avora"
    APPS_DIR="/usr/share/applications"
    ICON_BASE="/usr/share/icons/hicolor"
    PIXMAPS_DIR="/usr/share/pixmaps"
else
    PREFIX="${PREFIX:-$HOME/.local}"
    BIN_DIR="$PREFIX/bin"
    SHARE_DIR="$PREFIX/share/avora"
    APPS_DIR="$PREFIX/share/applications"
    ICON_BASE="$PREFIX/share/icons/hicolor"
    PIXMAPS_DIR="$PREFIX/share/pixmaps"
fi

echo "-> Removendo AVORA de $PREFIX..."
rm -f "$BIN_DIR/$APP_NAME" "$BIN_DIR/$BIN_TARGET"
rm -rf "$SHARE_DIR"
rm -f "$APPS_DIR/$APP_NAME.desktop"
rm -f "$PIXMAPS_DIR/$APP_NAME.png"

for sz in 256 128 64 48; do
    rm -f "$ICON_BASE/${sz}x${sz}/apps/$APP_NAME.png"
done

echo "-> AVORA desinstalado com sucesso."
