#!/usr/bin/env sh
# Wraps the built game (build/daytona, or another build directory's) in a
# macOS application bundle, to start it from the Finder, the Dock or
# Launchpad. Settings and saves stay in the same user data folder.
#
#   scripts/make_mac_app.sh [BUILD_DIR] [DEST]
#
# BUILD_DIR defaults to build; DEST to "BUILD_DIR/Daytona USA.app". The
# bundle holds a copy of the game, so run this again after rebuilding. It
# is for this Mac only: never distribute it (it contains code recompiled
# from your ROM set).
set -eu
cd "$(dirname "$0")/.."

BUILD=${1:-build}
APP=${2:-"$BUILD/Daytona USA.app"}
EXE="$BUILD/daytona"
if [ ! -x "$EXE" ]; then
    echo "make_mac_app.sh: $EXE not found; build the game first (./setup.sh with your ROM set in roms/)" >&2
    exit 1
fi
case "$(basename "$BUILD")" in
    build-daytona) NAME="Daytona USA (Rev A)"; ID=com.daytona-recomp.daytona ;;
    *)             NAME="Daytona USA";         ID=com.daytona-recomp.daytona93 ;;
esac

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$EXE" "$APP/Contents/MacOS/daytona"
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>$NAME</string>
    <key>CFBundleDisplayName</key><string>$NAME</string>
    <key>CFBundleIdentifier</key><string>$ID</string>
    <key>CFBundleExecutable</key><string>daytona</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>0.1</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>11.0</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>GCSupportsControllerUserInteraction</key><true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key><false/>
</dict>
</plist>
EOF
# An ad-hoc signature, so macOS runs the copy on Apple Silicon.
codesign --force --sign - "$APP" >/dev/null 2>&1 || true
echo "Made $APP (open it from the Finder, or: open \"$APP\")"
