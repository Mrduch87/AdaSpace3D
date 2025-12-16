const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const { SerialPort } = require('serialport');

const app = express();
const PORT = 3000;

// Enable CORS for localhost
app.use(cors({
  origin: ['http://localhost', 'file://', 'null'],
  credentials: true
}));

app.use(express.json());

let buildProcess = null;
let buildClients = [];

// Serve static files (optional - for hosting configurator.html)
app.use(express.static(__dirname));

// ============================================================
// ENDPOINT: Build Firmware
// ============================================================
app.post('/api/build', async (req, res) => {
  if (buildProcess && !buildProcess.killed) {
    return res.status(409).json({
      success: false,
      error: 'Build already in progress'
    });
  }

  // Set response headers for Server-Sent Events
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.flushHeaders();

  const sendEvent = (event, data) => {
    res.write(`event: ${event}\n`);
    res.write(`data: ${JSON.stringify(data)}\n\n`);
  };

  sendEvent('start', { message: 'Build started...' });

  // Execute PowerShell builder script
  const scriptPath = path.join(__dirname, 'builder.ps1');

  buildProcess = spawn('powershell.exe', [
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', scriptPath
  ], {
    cwd: __dirname
  });

  let stdout = '';
  let stderr = '';

  buildProcess.stdout.on('data', (data) => {
    const output = data.toString();
    stdout += output;
    sendEvent('output', { text: output });
  });

  buildProcess.stderr.on('data', (data) => {
    const output = data.toString();
    stderr += output;
    sendEvent('error', { text: output });
  });

  buildProcess.on('close', (code) => {
    if (code === 0) {
      // Find the UF2 file
      const outputDir = path.join(__dirname, 'Build_Temp', 'output');
      let uf2File = null;

      try {
        const files = fs.readdirSync(outputDir);
        uf2File = files.find(f => f.endsWith('.uf2'));
      } catch (err) {
        sendEvent('complete', {
          success: false,
          error: 'Build output directory not found'
        });
        res.end();
        buildProcess = null;
        return;
      }

      if (uf2File) {
        const uf2Path = path.join(outputDir, uf2File);
        sendEvent('complete', {
          success: true,
          uf2File: uf2Path,
          message: `Build successful! Firmware: ${uf2File}`
        });
      } else {
        sendEvent('complete', {
          success: false,
          error: 'UF2 file not found after build'
        });
      }
    } else {
      sendEvent('complete', {
        success: false,
        error: `Build failed with exit code ${code}`,
        stderr: stderr
      });
    }

    res.end();
    buildProcess = null;
  });

  buildProcess.on('error', (err) => {
    sendEvent('complete', {
      success: false,
      error: `Failed to start build: ${err.message}`
    });
    res.end();
    buildProcess = null;
  });
});

// ============================================================
// ENDPOINT: Trigger Bootloader Mode (1200 baud reset)
// ============================================================
app.post('/api/bootloader', async (req, res) => {
  try {
    // List all serial ports
    const ports = await SerialPort.list();

    console.log('Available ports:', ports.map(p => ({
      path: p.path,
      manufacturer: p.manufacturer,
      vendorId: p.vendorId,
      productId: p.productId
    })));

    // Find RP2040 device - support multiple identifiers
    const rp2040Port = ports.find(port => {
      // Custom 3DConnexion VID (our modified firmware)
      if (port.vendorId === '256f' || port.vendorId === '0x256f') return true;

      // Raspberry Pi Pico VID
      if (port.vendorId === '2e8a' || port.vendorId === '0x2e8a') return true;

      // Adafruit VID
      if (port.vendorId === '239a' || port.vendorId === '0x239a') return true;

      // Check manufacturer string
      if (port.manufacturer) {
        const mfg = port.manufacturer.toLowerCase();
        if (mfg.includes('3dconnexion')) return true;
        if (mfg.includes('raspberry')) return true;
        if (mfg.includes('adafruit')) return true;
        if (mfg.includes('pico')) return true;
      }

      // Check product string
      if (port.productId) {
        // Common RP2040 PIDs
        if (port.productId === 'c631') return true; // SpaceMouse
        if (port.productId === '000a') return true; // Pico
        if (port.productId === '00c1') return true; // Adafruit QT Py
      }

      return false;
    });

    if (!rp2040Port) {
      console.log('No RP2040 device found in available ports');
      return res.json({
        success: false,
        error: 'RP2040 device not found. Please ensure device is connected.',
        availablePorts: ports.length,
        hint: 'Device may have already entered bootloader mode. Check for RPI-RP2 drive.'
      });
    }

    console.log(`Found RP2040 at ${rp2040Port.path}`);

    // Perform 1200 baud touch to trigger bootloader
    const port = new SerialPort({
      path: rp2040Port.path,
      baudRate: 1200
    });

    // Wait a moment then close
    setTimeout(() => {
      port.close((err) => {
        if (err) {
          console.error('Error closing port:', err);
          return res.json({
            success: false,
            error: `Failed to reset: ${err.message}`
          });
        }

        res.json({
          success: true,
          message: 'Bootloader reset triggered. Device should reboot to UF2 mode in 2-3 seconds.'
        });
      });
    }, 500);

  } catch (err) {
    console.error('Bootloader error:', err);
    res.status(500).json({
      success: false,
      error: err.message
    });
  }
});

// ============================================================
// ENDPOINT: Auto-flash firmware
// ============================================================
app.post('/api/flash', async (req, res) => {
  const { uf2Path } = req.body;

  if (!uf2Path || !fs.existsSync(uf2Path)) {
    return res.status(400).json({
      success: false,
      error: 'Invalid UF2 file path'
    });
  }

  // Wait for RPI-RP2 drive to appear
  const maxWait = 10000; // 10 seconds
  const startTime = Date.now();
  let bootloaderDrive = null;

  const checkForDrive = () => {
    return new Promise((resolve) => {
      const interval = setInterval(() => {
        // Check if RPI-RP2 drive exists (Windows drive letters)
        for (let letter of 'DEFGHIJKLMNOPQRSTUVWXYZ') {
          const drivePath = `${letter}:\\`;
          try {
            if (fs.existsSync(drivePath)) {
              const volumeLabel = path.basename(drivePath);
              // Try to read a file to confirm it's accessible
              const files = fs.readdirSync(drivePath);
              // RPI-RP2 bootloader has INFO_UF2.TXT
              if (files.includes('INFO_UF2.TXT')) {
                clearInterval(interval);
                resolve(drivePath);
                return;
              }
            }
          } catch (err) {
            // Drive not accessible, continue
          }
        }

        if (Date.now() - startTime > maxWait) {
          clearInterval(interval);
          resolve(null);
        }
      }, 500);
    });
  };

  bootloaderDrive = await checkForDrive();

  if (!bootloaderDrive) {
    return res.json({
      success: false,
      error: 'Bootloader drive (RPI-RP2) not found. Please check device connection.'
    });
  }

  console.log(`Found bootloader at ${bootloaderDrive}`);

  // Copy UF2 file to bootloader drive
  const fileName = path.basename(uf2Path);
  const destPath = path.join(bootloaderDrive, fileName);

  try {
    fs.copyFileSync(uf2Path, destPath);
    res.json({
      success: true,
      message: 'Firmware flashed successfully! Device will reboot.'
    });
  } catch (err) {
    res.json({
      success: false,
      error: `Failed to copy firmware: ${err.message}`
    });
  }
});

// ============================================================
// ENDPOINT: Health check
// ============================================================
app.get('/api/health', (req, res) => {
  res.json({
    status: 'ok',
    buildInProgress: buildProcess && !buildProcess.killed
  });
});

// ============================================================
// Start Server
// ============================================================
app.listen(PORT, 'localhost', () => {
  console.log('');
  console.log('============================================================');
  console.log('  AdaSpace3D Build Server');
  console.log('============================================================');
  console.log(`  Server running at http://localhost:${PORT}`);
  console.log('  Configurator: http://localhost:3000/configurator.html');
  console.log('');
  console.log('  Press Ctrl+C to stop');
  console.log('============================================================');
  console.log('');
});
