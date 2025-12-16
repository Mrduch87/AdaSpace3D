# Build Server Quick Start

## Prerequisites

- **Node.js** installed ([Download here](https://nodejs.org))
- Device connected via USB

## Usage

### 1. Start the Build Server

Double-click `start-server.bat` or run:
```bash
.\start-server.bat
```

The server will:
- Check for Node.js
- Install dependencies (first run only)
- Start on http://localhost:3000

### 2. Open Configurator

Open in your browser:
```
http://localhost:3000/configurator.html
```

Or just open the local file:
```
file:///C:/Users/duch/Downloads/AdaSpace3D-main/configurator.html
```

### 3. Build & Flash

1. Click **"🔨 Build & Flash Firmware"** button
2. Progress bar will show:
   - Bootloader reset
   - Compilation progress
   - Automatic flashing
3. Wait for "✅ Complete!" message

## How It Works

### Bootloader Reset
Two methods (automatic fallback):
1. **Software**: Sends `BOOTLOADER` command via serial
2. **Hardware**: 1200 baud serial reset from server

### Build Process
- Executes `builder.ps1` PowerShell script
- Streams output via Server-Sent Events (SSE)
- Real-time progress in HTML

### Auto-Flash
- Waits for RPI-RP2 drive to appear
- Copies UF2 file automatically
- Device reboots with new firmware

## Endpoints

- `POST /api/build` - Start firmware compilation (SSE stream)
- `POST /api/bootloader` - Trigger UF2 bootloader mode
- `POST /api/flash` - Copy UF2 to device
- `GET /api/health` - Server status check

## Troubleshooting

### Server won't start
```
ERROR: Node.js not found!
```
**Solution**: Install Node.js from https://nodejs.org

### Build fails
- Check `AdaSpace3D.ino` for syntax errors
- Review build log in HTML for details

### Device not entering bootloader
1. Manually enter bootloader:
   - Hold BOOT button
   - Press RESET
   - Release BOOT
2. Try build again

### Flash fails
```
Bootloader drive (RPI-RP2) not found
```
**Solution**: 
- Ensure device entered bootloader (check for RPI-RP2 drive)
- Wait 10 seconds and try again

## Manual Compilation

If you prefer the traditional method:
```bash
.\FLASH.bat
```

## Server Port

Default: `http://localhost:3000`

To change port, edit `build-server.js`:
```javascript
const PORT = 3000; // Change this
```
