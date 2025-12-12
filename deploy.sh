#!/bin/bash
set -e

# ============================================================
# AdaSpace3D - One-Click Deploy (Linux/macOS)
# ============================================================
# A self-contained build system that:
# 1. Downloads Arduino CLI (local ./tools)
# 2. Installs RP2040 Core & Libraries
# 3. Compiles the Golden Image
# 4. Auto-flashes to RPI-RP2
# ============================================================

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
TOOLS_DIR="$SCRIPT_DIR/tools"
CLI="$TOOLS_DIR/arduino-cli"
BUILD_DIR="$SCRIPT_DIR/build_temp"
OUTPUT_DIR="$SCRIPT_DIR/output"

# Configuration
BOARD_FQBN="rp2040:rp2040:adafruit_qtpy:usbstack=tinyusb"
USB_VID="0x256f"
USB_PID="0xc631"
USB_PRODUCT="SpaceMouse Pro Wireless"
USB_MANUFACTURER="3Dconnexion"

log() { echo -e "${CYAN}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# ============================================================
# 1. Setup Environment
# ============================================================
setup_cli() {
    log "Checking Arduino CLI..."
    mkdir -p "$TOOLS_DIR"

    if [ ! -f "$CLI" ]; then
        log "Downloading Arduino CLI..."
        # Detect OS
        if [[ "$OSTYPE" == "darwin"* ]]; then
            URL="https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_macOS_64bit.tar.gz"
        else
            URL="https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz"
        fi
        
        curl -L "$URL" | tar xz -C "$TOOLS_DIR"
        chmod +x "$CLI"
        success "Arduino CLI installed."
    else
        success "Arduino CLI found."
    fi

    # Config
    if [ ! -f "$TOOLS_DIR/arduino-cli.yaml" ]; then
        "$CLI" config init --dest-dir "$TOOLS_DIR" > /dev/null 2>&1 || true
    fi

    # Always enforce the correct URL and update index
    "$CLI" config set board_manager.additional_urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json > /dev/null
    log "Updating core index..."
    "$CLI" core update-index > /dev/null
}

install_deps() {
    log "Installing Dependencies..."
    "$CLI" core install rp2040:rp2040 > /dev/null
    
    libs=("Adafruit TinyUSB Library" "XENSIV 3D Magnetic Sensor TLx493D" "Adafruit NeoPixel" "Adafruit seesaw Library" "ArduinoJson")
    for lib in "${libs[@]}"; do
        "$CLI" lib install "$lib" > /dev/null 2>&1 || true
    done
    success "Dependencies ready."
}

# ============================================================
# 2. Compile Golden Image
# ============================================================
compile_firmware() {
    log "Compiling Firmware..."
    mkdir -p "$OUTPUT_DIR"
    
    # Flags using array to handle spaces correctly
    FLAGS=(
        "--build-property" "build.vid=$USB_VID"
        "--build-property" "build.pid=$USB_PID"
        "--build-property" "build.usbvid=-DUSBD_VID=$USB_VID"
        "--build-property" "build.usbpid=-DUSBD_PID=$USB_PID"
        "--build-property" "build.usb_product=\"$USB_PRODUCT\""
        "--build-property" "build.usb_manufacturer=\"$USB_MANUFACTURER\""
    )

    "$CLI" compile --fqbn "$BOARD_FQBN" "${FLAGS[@]}" --output-dir "$OUTPUT_DIR" "$SCRIPT_DIR/AdaSpace3D.ino"
    
    if [ -f "$OUTPUT_DIR/AdaSpace3D.ino.uf2" ]; then
        success "Build Complete: $OUTPUT_DIR/AdaSpace3D.ino.uf2"
    else
        error "Compilation failed."
    fi
}

# ============================================================
# 3. Flash to Device
# ============================================================
flash_device() {
    log "Looking for RPI-RP2 bootloader volume..."
    
    MOUNT_POINT=""
    
    # macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if [ -d "/Volumes/RPI-RP2" ]; then
            MOUNT_POINT="/Volumes/RPI-RP2"
        fi
    # Linux
    else 
        # Look in common mount locations
        if [ -d "/media/$USER/RPI-RP2" ]; then
            MOUNT_POINT="/media/$USER/RPI-RP2"
        elif [ -d "/run/media/$USER/RPI-RP2" ]; then
             MOUNT_POINT="/run/media/$USER/RPI-RP2"
        fi
    fi

    if [ -n "$MOUNT_POINT" ]; then
        log "Found device at: $MOUNT_POINT"
        log "Flashing..."
        cp "$OUTPUT_DIR/AdaSpace3D.ino.uf2" "$MOUNT_POINT/"
        success "Flashed successfully! Device will reboot."
    else
        warn "Device NOT found in bootloader mode."
        echo -e "${YELLOW}MANUAL STEP:${NC}"
        echo "1. Hold BOOT button on device"
        echo "2. Press RESET button"
        echo "3. Drag '$OUTPUT_DIR/AdaSpace3D.ino.uf2' to the RPI-RP2 drive manually."
    fi
}

# ============================================================
# Main
# ============================================================
echo "======================================"
echo " AdaSpace3D Deploy Tool (Linux/Mac)"
echo "======================================"

setup_cli
install_deps
compile_firmware
flash_device
