
import { test, expect } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const screenshotsDir = path.resolve(__dirname, '../../screenshots');

test.use({
  viewport: { width: 1280, height: 800 },
  video: {
    mode: 'on',
    size: { width: 1280, height: 800 },
    dir: screenshotsDir
  }
});

test.describe('Generate Assets', () => {

  test('capture screenshots and video', async ({ page }, testInfo) => {
    // --- Mocks ---

    // SysInfo (Dashboard data)
    await page.route('**/sysinfo.json', async route => {
      await route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({
          sysInfo: {
            serial: "MEQ1234567",
            currentVersion: "2.1.8",
            latestVersion: "2.1.8",
            rawUartRemoteAddress: "192.168.1.10",
            memoryUsage: 45.2,
            cpuUsage: 12.5,
            supplyVoltage: 5.02,
            temperature: 42.1,
            uptimeSeconds: 12345,
            boardRevision: "v2.0",
            resetReason: "Power On Reset",
            ethernetConnected: true,
            ethernetSpeed: 100,
            ethernetDuplex: "Full",
            radioModuleType: "HM-MOD-RPI-PCB",
            radioModuleSerial: "MEQ1234567",
            radioModuleFirmwareVersion: "2.8.6",
            radioModuleBidCosRadioMAC: "0x123456",
            radioModuleHmIPRadioMAC: "0x654321",
            radioModuleSGTIN: "3014F711A0001F98A99AXXXX"
          }
        })
      });
    });

    // Settings
    await page.route('**/settings.json', async route => {
      await route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({
          settings: {
            hostname: "hb-rf-eth",
            useDHCP: true,
            localIP: "192.168.1.200",
            netmask: "255.255.255.0",
            gateway: "192.168.1.1",
            dns1: "192.168.1.1",
            dns2: "",
            ccuIP: "192.168.1.10",
            enableIPv6: false,
            ipv6Mode: "auto",
            timesource: 0,
            dcfOffset: 0,
            gpsBaudrate: 9600,
            ntpServer: "pool.ntp.org",
            ledBrightness: 50,
            ledPrograms: {
                idle: 1,
                ccu_disconnected: 5,
                ccu_connected: 6,
                update_available: 4,
                error: 10,
                booting: 4,
                update_in_progress: 5
            }
          }
        })
      });
    });

    // Login
    await page.route('**/login.json', async route => {
      if (route.request().method() === 'POST') {
        await route.fulfill({
          contentType: 'application/json',
          body: JSON.stringify({
            isAuthenticated: true,
            token: "dummy-token-12345",
            passwordChanged: true
          })
        });
      } else {
        route.continue();
      }
    });

    // Monitoring
    await page.route('**/api/monitoring', async route => {
        await route.fulfill({
            contentType: 'application/json',
            body: JSON.stringify({
                snmp: { enabled: false, port: 161 },
                checkmk: { enabled: true, port: 6556 },
                mqtt: { enabled: false }
            })
        });
    });

    // Update Check (Available)
    await page.route('**/api/check_update?*', async route => {
        await route.fulfill({
            contentType: 'text/plain',
            body: "2.1.9" // Newer version to show badge
        });
    });

    // System Log
    await page.route('**/api/log*', async route => {
        await route.fulfill({
            contentType: 'text/plain',
            headers: { 'X-Log-Total': '500' },
            body: "I (1000) main: Board: HB-RF-ETH-ng (v2.0)\nI (1010) net: Ethernet Link Up. Speed: 100Mbps, Duplex: Full\nI (1012) net: IPv4 Address: 192.168.1.200\nI (1020) hb-rf: Radio module detected: HM-MOD-RPI-PCB\nI (1025) hb-rf: Radio Serial: MEQ1234567\nI (1030) hb-rf: Radio Firmware: 2.8.6\nI (1040) webui: Web server started on port 80\nI (2000) webui: Client connected: 192.168.1.50\nI (5000) update: Checking for updates...\nI (5005) update: Update available: v2.1.9\n"
        });
    });

    // Changelog
    await page.route('**/api/changelog', async route => {
        await route.fulfill({
            contentType: 'text/markdown',
            body: "# Changelog\n\n## v2.1.9 (Latest)\n- **New Feature**: Added dark mode support for all pages.\n- **Improvement**: Enhanced radio communication stability.\n- **Fix**: Resolved issue with static IP configuration.\n\n## v2.1.8\n- **Feature**: Redesigned dashboard layout.\n- **Fix**: Memory leak in long-running sessions fixed.\n- **Security**: Updated dependency versions."
        });
    });

    // --- Execution ---

    // Ensure dir exists
    if (!fs.existsSync(screenshotsDir)){
        fs.mkdirSync(screenshotsDir, { recursive: true });
    }

    // 1. Login Page
    // Navigate to login explicitly
    await page.goto('http://localhost:5173/login');
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(500);
    await page.screenshot({ path: path.join(screenshotsDir, '01_Login.png') });

    // 2. Perform Login
    await page.fill('input[type="password"]', 'Password123');
    // Button might not be type="submit", target by class or text
    await page.click('.login-btn');
    // Wait for navigation to home
    await page.waitForURL('http://localhost:5173/');

    // 3. Dashboard (Home)
    // Wait for either sysinfo page or grid
    try {
        await page.waitForSelector('.sysinfo-page, .dashboard-grid', { state: 'visible', timeout: 5000 });
    } catch (e) {
        console.log("Dashboard selector not found, taking screenshot anyway");
    }
    await page.waitForTimeout(2000); // Allow animations and data population
    await page.screenshot({ path: path.join(screenshotsDir, '02_Dashboard.png') });

    // 4. Settings (General)
    await page.goto('http://localhost:5173/settings');
    await page.waitForSelector('.settings-form', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1000);
    await page.screenshot({ path: path.join(screenshotsDir, '04_Settings.png') });

    // 5. Monitoring
    await page.goto('http://localhost:5173/monitoring');
    await page.waitForSelector('.monitoring-page', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1000);
    await page.screenshot({ path: path.join(screenshotsDir, '05_Monitoring.png') });

    // 6. Firmware
    await page.goto('http://localhost:5173/firmware');
    await page.waitForSelector('.firmware-page', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1000);
    await page.screenshot({ path: path.join(screenshotsDir, '06_FirmwareUpdate.png') });

    // 7. System Log
    await page.goto('http://localhost:5173/systemlog');
    await page.waitForSelector('.log-container', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1500); // Wait for log poll
    await page.screenshot({ path: path.join(screenshotsDir, '07_SystemLog.png') });

    // 8. About
    await page.goto('http://localhost:5173/about');
    await page.waitForSelector('.about-page', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1000);
    await page.screenshot({ path: path.join(screenshotsDir, '08_About.png') });

    // 9. Changelog Modal
    // Go to firmware page where the button exists
    await page.goto('http://localhost:5173/firmware');
    await page.waitForSelector('.firmware-page', { timeout: 5000 }).catch(() => {});

    const changelogBtn = page.locator('.changelog-btn').first();
    if (await changelogBtn.isVisible()) {
        await changelogBtn.click();
        await page.waitForSelector('.modal-content:visible');
        await page.waitForTimeout(1000);
        await page.screenshot({ path: path.join(screenshotsDir, '09_Changelog.png') });
        await page.keyboard.press('Escape');
    }

    // Video will be saved automatically to screenshotsDir with a random name.
  });
});
