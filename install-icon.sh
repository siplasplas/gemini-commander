#!/bin/bash
# Install Gemini Commander desktop integration

# Determine installation prefix
if [ "$1" = "--system" ]; then
    PREFIX="/usr"
    SUDO="sudo"
else
    PREFIX="$HOME/.local"
    SUDO=""
fi

ICON_DIR="$PREFIX/share/icons/hicolor"
APP_DIR="$PREFIX/share/applications"

echo "Installing Gemini Commander desktop integration to $PREFIX..."

# Create directories
$SUDO mkdir -p "$ICON_DIR"/{16x16,32x32,48x48,64x64,128x128,256x256}/apps
$SUDO mkdir -p "$APP_DIR"

# Install icons
for size in 16 32 48 64 128 256; do
    $SUDO cp "res/icons/gemini-commander-${size}.png" \
            "$ICON_DIR/${size}x${size}/apps/gemini-commander.png"
    echo "  Installed ${size}x${size} icon"
done

# Install .desktop file
$SUDO cp "res/gemini-commander.desktop" "$APP_DIR/"
echo "  Installed .desktop file"

# Update icon cache
if [ "$1" = "--system" ]; then
    $SUDO gtk-update-icon-cache -f -t "$ICON_DIR" 2>/dev/null || true
    $SUDO update-desktop-database "$APP_DIR" 2>/dev/null || true
else
    gtk-update-icon-cache -f -t "$ICON_DIR" 2>/dev/null || true
    update-desktop-database "$APP_DIR" 2>/dev/null || true
fi

echo "Done! Gemini Commander icon should now appear in your application menu."
echo "You may need to log out and log back in to see the changes."