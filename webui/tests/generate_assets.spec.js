import { test } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const repoRoot = path.resolve(__dirname, '../..');
const screenshotsDir = path.join(repoRoot, 'screenshots');
// The wiki (Web-UI.md, Home.md) embeds these by raw.githubusercontent URL, so
// the file names here are a contract with the wiki - renaming one means
// editing the wiki page in the same change.
const wikiDir = path.join(screenshotsDir, 'newdesign');
const BASE_URL = 'http://127.0.0.1:1234';
const gotoApp = (page, pathName) => page.goto(`${BASE_URL}${pathName}`, { waitUntil: 'domcontentloaded' });

// Read the real version instead of hardcoding one - a pinned literal silently
// went stale and shipped a wrong version number into the README screenshots.
const VERSION = fs.readFileSync(path.join(repoRoot, 'version.txt'), 'utf8').trim();

// The NewDesign emerald accent from docs/WEBUI_DESIGN_SYSTEM.md. The WebUI reads
// the accent from the device via /api/theme (see useThemeStore.init), so serving
// it here renders exactly what a device configured this way shows - no default
// anywhere in firmware or WebUI is changed to produce these images.
const NEWDESIGN_ACCENT = '#2F8B57';

test.use({
  viewport: { width: 1600, height: 1000 },
  // The WebUI picks its language from the browser, and the README and wiki these
  // screenshots feed are German - pin both so captures stay reproducible.
  locale: 'de-DE',
  timezoneId: 'Europe/Berlin'
});

async function setupMocks(page) {
  // Theme - drives the accent colour for every screenshot.
  await page.route('**/api/theme', async route => {
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ colorScheme: 'light', primaryColor: NEWDESIGN_ACCENT })
    });
  });

  // SysInfo (Dashboard data)
  await page.route('**/sysinfo.json**', async route => {
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({
        sysInfo: {
          serial: "MEQ1234567",
          hostname: "hb-rf-eth",
          currentVersion: VERSION,
          latestVersion: "n/a",
          rawUartRemoteAddress: "192.168.1.10",
          memoryUsage: 45.2,
          cpuUsage: 12.5,
          supplyVoltage: null,
          temperature: null,
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
  await page.route('**/settings.json**', async route => {
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
        checkmk: { enabled: true, port: 6556 },
        mqtt: { enabled: false }
      })
    });
  });

  // System Log
  await page.route('**/api/log*', async route => {
    await route.fulfill({
      contentType: 'text/plain',
      headers: { 'X-Log-Total': '500' },
      body: "I (1000) main: Board: HB-RF-ETH-ng (v2.0)\nI (1010) net: Ethernet Link Up. Speed: 100Mbps, Duplex: Full\nI (1012) net: IPv4 Address: 192.168.1.200\nI (1020) hb-rf: Radio module detected: HM-MOD-RPI-PCB\nI (1025) hb-rf: Radio Serial: MEQ1234567\nI (1030) hb-rf: Radio Firmware: 2.8.6\nI (1040) webui: Web server started on port 80\nI (2000) webui: Client connected: 192.168.1.50\n"
    });
  });

  // /api/log/status decides whether the viewer renders the live buffer or the
  // "capture disabled" placeholder. Registered after the generic
  // '**/api/log*' route so this more specific handler wins.
  await page.route('**/api/log/status*', async route => {
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ enabled: true })
    });
  });
}

async function login(page) {
  await gotoApp(page, '/login');
  await page.waitForLoadState('networkidle');
  await page.waitForTimeout(500);
  await page.fill('input[name="username"]', 'admin');
  await page.fill('input[type="password"]', 'Password123');
  // Button might not be type="submit", target by class or text
  await page.click('.login-btn');
  await page.waitForURL(`${BASE_URL}/`);
}

// Settle animations and let mocked data populate before the shutter.
async function shoot(page, dir, name, { fullPage = false } = {}) {
  await page.waitForTimeout(1000);
  await page.screenshot({ path: path.join(dir, name), fullPage });
}

test.describe('Generate Assets', () => {

  test('capture README screenshots', async ({ page }) => {
    test.setTimeout(180000);
    await setupMocks(page);
    fs.mkdirSync(screenshotsDir, { recursive: true });

    await gotoApp(page, '/login');
    await page.waitForLoadState('networkidle');
    await shoot(page, screenshotsDir, '01_Login.png');

    await login(page);
    await page.waitForSelector('.page-shell.dashboard, .dashboard-grid', { state: 'visible', timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1000);
    await shoot(page, screenshotsDir, '02_Dashboard.png');

    await gotoApp(page, '/settings');
    await page.waitForSelector('.settings-form', { timeout: 5000 }).catch(() => {});
    await shoot(page, screenshotsDir, '04_Settings.png');

    await gotoApp(page, '/monitoring');
    await page.waitForSelector('.monitoring-page', { timeout: 5000 }).catch(() => {});
    await shoot(page, screenshotsDir, '05_Monitoring.png');

    await gotoApp(page, '/updates/firmware');
    await page.waitForSelector('.firmware-page', { timeout: 5000 }).catch(() => {});
    await shoot(page, screenshotsDir, '06_FirmwareUpdate.png');

    await gotoApp(page, '/systemlog');
    await page.waitForSelector('.log-container', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(500);
    await shoot(page, screenshotsDir, '07_SystemLog.png');

    await gotoApp(page, '/about');
    await page.waitForSelector('.about-page', { timeout: 5000 }).catch(() => {});
    await shoot(page, screenshotsDir, '08_About.png');
  });

  // Full-page captures for the wiki. Settings tabs come from settings.vue:
  // general, network, time, backup, design (experimental is opt-in and renders
  // an empty state, so it is deliberately not captured).
  test('capture wiki screenshots', async ({ page }) => {
    test.setTimeout(240000);
    await setupMocks(page);
    fs.mkdirSync(wikiDir, { recursive: true });

    await gotoApp(page, '/login');
    await page.waitForLoadState('networkidle');
    await shoot(page, wikiDir, '12_Login.png', { fullPage: true });

    await login(page);
    await page.waitForSelector('.page-shell.dashboard, .dashboard-grid', { state: 'visible', timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(1000);
    await shoot(page, wikiDir, '01_Dashboard.png', { fullPage: true });

    await gotoApp(page, '/monitoring');
    await page.waitForSelector('.monitoring-page', { timeout: 5000 }).catch(() => {});
    await shoot(page, wikiDir, '02_Monitoring.png', { fullPage: true });

    const settingsTabs = [
      ['general', '03_Settings_Allgemein.png'],
      ['network', '04_Settings_Netzwerk.png'],
      ['time', '05_Settings_Zeit.png'],
      ['backup', '06_Settings_Backup.png'],
      ['design', '07_Settings_Design.png']
    ];
    for (const [tab, name] of settingsTabs) {
      await gotoApp(page, `/settings?tab=${tab}`);
      await page.waitForSelector('.settings-form', { timeout: 5000 }).catch(() => {});
      await shoot(page, wikiDir, name, { fullPage: true });
    }

    await gotoApp(page, '/updates/firmware');
    await page.waitForSelector('.firmware-page', { timeout: 5000 }).catch(() => {});
    await shoot(page, wikiDir, '09_Firmware.png', { fullPage: true });

    await gotoApp(page, '/systemlog');
    await page.waitForSelector('.log-container', { timeout: 5000 }).catch(() => {});
    await page.waitForTimeout(500);
    await shoot(page, wikiDir, '10_Systemlog.png', { fullPage: true });

    await gotoApp(page, '/about');
    await page.waitForSelector('.about-page', { timeout: 5000 }).catch(() => {});
    await shoot(page, wikiDir, '11_Ueber.png', { fullPage: true });
  });
});
