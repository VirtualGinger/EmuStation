
#!/bin/bash

set -e

PROJECT_DIR="$HOME/EmuStation/EmuStation"
API_DIR="$PROJECT_DIR/api"
USB_DEVICE="/dev/sda1"
USB_MOUNT="/mnt/emustation-usb"

echo "EmuStation Start"

# Mount USB drive
echo "[1/4] Checking USB drive"

sudo mkdir -p "$USB_MOUNT"

if ! mountpoint -q "$USB_MOUNT"; then
    echo "Mounting USB..."
    sudo mount -t ntfs3 "$USB_DEVICE" "$USB_MOUNT"
fi

# Verify game directory
echo "[2/4] Checking game"

GAME_DIR="$USB_MOUNT/Metal Gear Solid - Peace Walker - HD Edition (Japan)"

if [ ! -f "$GAME_DIR/PS3_GAME/PARAM.SFO" ]; then
    echo "ERROR: game not found on the USB drive."
    exit 1
fi

echo "found game"

# Start API
echo "[3/4] activate venv"

cd "$API_DIR"

if [ -d ".venv" ]; then
    source .venv/bin/activate
elif [ -d "venv" ]; then
    source venv/bin/activate
else
    echo "ERROR: Python virtual environment not found."
    exit 1
fi

# Check port
if sudo ss -ltnp | grep -q ':8000 '; then
    echo "ERROR: Port 8000 is already in use."
    echo "Stop the existing API before continuing."
    exit 1
fi

echo "[4/4] Start api"
echo "API: http://127.0.0.1:8000"
echo "Press Ctrl+C to stop."

exec uvicorn main:app --host 127.0.0.1 --port 8000
