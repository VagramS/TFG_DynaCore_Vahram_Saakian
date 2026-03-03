#!/bin/zsh
set -e

SRC="/Users/vahram/Library/Developer/Xcode/DerivedData/IPlugEffect-fpymqzjynytqghcpwhcnfscxltmp/Build/Intermediates.noindex/UninstalledProducts/macosx/DynaCore.component"
DST="$HOME/Library/Audio/Plug-Ins/Components/DynaCore.component"

rm -rf "$DST"
cp -R "$SRC" "$DST"

codesign --force --deep --sign - "$DST" >/dev/null 2>&1 || true
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed: $DST"
ls -la "$DST/Contents/MacOS"
