#!/bin/bash
# ============================================================
# DynaCore macOS Installer Builder
#
# Builds Release targets (VST3, AUv2) and packages them
# into a .pkg installer with selectable components.
#
# Usage:
#   cd iPlug2/Examples/DynaCore
#   bash installer/makedist-mac.sh
#
# Output:
#   installer/DynaCore-1.0.0-macOS.pkg
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
XCODEPROJ="$PROJECT_DIR/projects/DynaCore-macOS.xcodeproj"

PLUGIN_NAME="DynaCore"
VERSION="1.0.0"
IDENTIFIER_BASE="com.ucm.dynacore"

BUILD_CONFIG="Release"
STAGING_DIR="$SCRIPT_DIR/_staging"
OUTPUT_DIR="$SCRIPT_DIR"
PKG_NAME="${PLUGIN_NAME}-${VERSION}-macOS.pkg"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ------------------------------------------------------------------
# 1. Clean previous staging
# ------------------------------------------------------------------
info "Cleaning staging directory..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

# ------------------------------------------------------------------
# 2. Build Release targets (VST3 + AUv2)
# ------------------------------------------------------------------
info "Building VST3 (Release)..."
xcodebuild -project "$XCODEPROJ" -scheme "macOS-VST3" \
  -configuration "$BUILD_CONFIG" build -quiet \
  || error "VST3 build failed"

info "Building AUv2 (Release)..."
xcodebuild -project "$XCODEPROJ" -scheme "macOS-AUv2" \
  -configuration "$BUILD_CONFIG" build -quiet \
  || error "AUv2 build failed"

# ------------------------------------------------------------------
# 3. Locate built artefacts
# ------------------------------------------------------------------
VST3_SRC="$HOME/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"
AU_SRC="$HOME/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"

[ -d "$VST3_SRC" ] || error "VST3 not found at $VST3_SRC"
[ -d "$AU_SRC"   ] || error "AUv2 not found at $AU_SRC"

# ------------------------------------------------------------------
# 4. Build individual component packages
# ------------------------------------------------------------------
info "Packaging VST3..."
pkgbuild \
  --component "$VST3_SRC" \
  --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier "${IDENTIFIER_BASE}.vst3" \
  --version "$VERSION" \
  "$STAGING_DIR/${PLUGIN_NAME}-vst3.pkg"

info "Packaging AUv2..."
pkgbuild \
  --component "$AU_SRC" \
  --install-location "/Library/Audio/Plug-Ins/Components" \
  --identifier "${IDENTIFIER_BASE}.au" \
  --version "$VERSION" \
  "$STAGING_DIR/${PLUGIN_NAME}-au.pkg"

# ------------------------------------------------------------------
# 5. Create distribution XML with selectable components
# ------------------------------------------------------------------
info "Creating distribution XML with component choices..."
cat > "$STAGING_DIR/distribution.xml" << 'DISTXML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>DynaCore 1.0.0</title>
    <welcome mime-type="text/plain"><![CDATA[
DynaCore — Real-Time Multi-Module Audio Effects Plugin

Select the plugin formats you want to install.
Both VST3 and Audio Unit are selected by default.
    ]]></welcome>
    <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3"
            title="VST3"
            description="Install DynaCore.vst3 to /Library/Audio/Plug-Ins/VST3 (for Studio One, Ableton Live, Cubase, etc.)"
            selected="true">
        <pkg-ref id="com.ucm.dynacore.vst3"/>
    </choice>
    <choice id="au"
            title="Audio Unit (AUv2)"
            description="Install DynaCore.component to /Library/Audio/Plug-Ins/Components (for Logic Pro, GarageBand, etc.)"
            selected="true">
        <pkg-ref id="com.ucm.dynacore.au"/>
    </choice>
    <pkg-ref id="com.ucm.dynacore.vst3" version="1.0.0">DynaCore-vst3.pkg</pkg-ref>
    <pkg-ref id="com.ucm.dynacore.au" version="1.0.0">DynaCore-au.pkg</pkg-ref>
</installer-gui-script>
DISTXML

# ------------------------------------------------------------------
# 6. Build final combined installer
# ------------------------------------------------------------------
info "Building final installer..."
productbuild \
  --distribution "$STAGING_DIR/distribution.xml" \
  --package-path "$STAGING_DIR" \
  "$OUTPUT_DIR/$PKG_NAME"

# ------------------------------------------------------------------
# 7. Clean up staging
# ------------------------------------------------------------------
rm -rf "$STAGING_DIR"

info "Done! Installer: $OUTPUT_DIR/$PKG_NAME"
echo ""
echo "Install locations:"
echo "  VST3 -> /Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"
echo "  AU   -> /Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
