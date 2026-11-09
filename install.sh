#!/usr/bin/env bash
set -euo pipefail

APP_NAME="avora"
BIN_TARGET="aed_tree_debugger"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# If running as root, install system-wide under /usr/local; otherwise XDG user install under ~/.local
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

echo "============================================================"
echo "  Instalador AVORA para Linux (Algoritmos & Estruturas)"
echo "============================================================"
echo "-> Compilando versão final otimizada (C11 + Raylib + GDB/MI)..."
make clean
make

echo "-> Criando diretórios de instalação em: $PREFIX"
mkdir -p "$BIN_DIR" "$SHARE_DIR" "$APPS_DIR" "$PIXMAPS_DIR"

echo "-> Instalando binários ($BIN_DIR/$APP_NAME e $BIN_DIR/$BIN_TARGET)..."
install -m 755 "$BIN_TARGET" "$BIN_DIR/$BIN_TARGET"
ln -sf "$BIN_DIR/$BIN_TARGET" "$BIN_DIR/$APP_NAME"

if [ -f "logo.jpg" ]; then
    install -m 644 "logo.jpg" "$SHARE_DIR/logo.jpg"

    echo "-> Redimensionando logo.jpg (Bicubic) para ícones do Linux (256x256, 128x128, 64x64, 48x48)..."
    TMP_RESIZER="/tmp/avora_icon_resizer_$$"
    cat << 'EOF' > "${TMP_RESIZER}.c"
#include <raylib.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 4) return 1;
    SetTraceLogLevel(LOG_WARNING);
    Image src = LoadImage(argv[1]);
    if (!src.data) return 1;
    int sz = atoi(argv[2]);
    if (sz <= 0) sz = 256;
    ImageResize(&src, sz, sz);
    bool ok = ExportImage(src, argv[3]);
    UnloadImage(src);
    return ok ? 0 : 1;
}
EOF
    gcc -std=c11 -O2 "${TMP_RESIZER}.c" -lraylib -lm -o "$TMP_RESIZER"

    for sz in 256 128 64 48; do
        ICON_DIR="$ICON_BASE/${sz}x${sz}/apps"
        mkdir -p "$ICON_DIR"
        "$TMP_RESIZER" "logo.jpg" "$sz" "$ICON_DIR/$APP_NAME.png"
    done

    cp "$ICON_BASE/256x256/apps/$APP_NAME.png" "$PIXMAPS_DIR/$APP_NAME.png"
    cp "$ICON_BASE/256x256/apps/$APP_NAME.png" "$SHARE_DIR/$APP_NAME.png"
    rm -f "$TMP_RESIZER" "${TMP_RESIZER}.c"
fi

echo "-> Gerando atalho do sistema ($APPS_DIR/$APP_NAME.desktop)..."
cat << EOF > "$APPS_DIR/$APP_NAME.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=AVORA
GenericName=Visualizador e Depurador de Árvores em C
Comment=Analisador Visual de Operações e Rastreamento de Árvores (Algoritmos & Estruturas de Dados)
Exec=$BIN_DIR/$APP_NAME
Icon=$SHARE_DIR/$APP_NAME.png
Terminal=false
Categories=Development;Education;IDE;Debugger;
Keywords=Tree;BinaryTree;AVL;RedBlack;BTree;LeetCode;GDB;C;Algoritmos;
StartupNotify=true
StartupWMClass=aed_tree_debugger
EOF

chmod 644 "$APPS_DIR/$APP_NAME.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$ICON_BASE" >/dev/null 2>&1 || true
fi

echo "============================================================"
echo "  Instalação concluída com sucesso!"
echo "  - Comando no terminal: $APP_NAME  (ou $BIN_DIR/$APP_NAME)"
echo "  - Atalho no menu de aplicativos: AVORA"
echo "  - Ícone gerado a partir de logo.jpg: $SHARE_DIR/$APP_NAME.png"
echo "============================================================"
