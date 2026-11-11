import { test, expect } from '@playwright/test'

// Visual regression guard: every core route is screenshot in both themes.
// A baseline mismatch (layout drift, design-system change, broken component)
// fails CI exactly like a functional regression would.
//
// Update baselines deliberately with:
//   npx playwright test tests/visual.spec.js --update-snapshots
// and review the diff in git before committing.
//
// The device APIs are mocked so the rendered shells are deterministic.
// Animations and carets are disabled via Playwright's built-in options.

const BASE_URL = 'http://127.0.0.1:1234'

const json = (body) => (route) =>
  route.fulfill({ contentType: 'application/json', body: JSON.stringify(body) })

const sysInfo = () => ({
  sysInfo: {
    hostname: 'HB-RF-ETH-Visual',
    serial: '2805A5114784',
    currentVersion: '2.4.0-Beta.1',
    memoryUsage: 67,
    cpuUsage: 0,
    uptimeSeconds: 1397,
    boardRevision: 'REV 1.10/1.11 (PUB)',
    resetReason: 'Power on reset',
    ethernetConnected: true,
    ethernetSpeed: 100,
    ethernetDuplex: 'Full',
    localIP: '192.168.1.200',
    netmask: '255.255.255.0',
    gateway: '192.168.1.1',
    dns1: '192.168.1.1',
    dns2: '',
    rawUartRemoteAddress: '192.168.1.10',
    radioModuleType: 'RPI-RF-MOD',
    radioModuleSerial: '58A9A71150',
    radioModuleFirmwareVersion: '4.4.22',
    radioModuleBidCosRadioMAC: '0x123456',
    radioModuleHmIPRadioMAC: '0x654321',
    radioModuleSGTIN: '3014F711A0001F98A99AXXXX'
  }
})

const idleUpdateStatus = () => ({
  state: 'idle',
  everChecked: true,
  channel: 'stable',
  lastCheck: 0,
  cooldownRemainingSec: 0,
  runningFirmware: '2.4.0-Beta.1',
  runningWebui: '1.2.0-Beta.1',
  latestFirmware: '2.4.0-Beta.1',
  latestWebui: '1.2.0-Beta.1',
  firmwareUpdateAvailable: false,
  webuiUpdateAvailable: false,
  notesUrl: '',
  lastError: '',
  lastSkipReason: '',
  install: { state: 'idle', target: 'firmware', progress: 0, version: '', error: '' },
  schedule: {
    mode: 'off', channel: 'stable', intervalHours: 24, windowStart: 3,
    windowLength: 2, idleOnly: true, nextCheckIn: 0, installBlocked: null,
    lastSeenCheck: 0, seenFirmware: '', seenWebui: '', pending: null, stage: null
  }
})

const monitoringConfig = () => ({
  checkmk: { enabled: false, port: 6556 },
  mqtt: { enabled: false },
  prometheus: { enabled: false },
  syslog: { enabled: false },
  notify: { enabled: false, channels: 0, eventMask: 0, eventMaskSupported: 0x0fff }
})

const journal = () => ({
  count: 2,
  events: [
    { time: 1794400000, type: 'ccu_connected', detail: '192.168.1.10' },
    { time: 1794399000, type: 'eth_link_down', detail: '' }
  ]
})

const ROUTES = ['/', '/settings', '/monitoring', '/updates/firmware', '/diagnostics', '/about']
const THEMES = ['light', 'dark']

for (const theme of THEMES) {
  test.describe(`visual ${theme}`, () => {
    test.beforeEach(async ({ page }) => {
      await page.addInitScript((t) => {
        localStorage.setItem('theme', t)
        localStorage.setItem('locale', 'en')
        sessionStorage.setItem('hb-rf-eth-ng-pw', 'device-secret-token')
        sessionStorage.setItem('hb-rf-eth-ng-last-activity', String(Date.now()))
      }, theme)

      // Catch-all first (reverse matching: later registrations win).
      await page.route('**/api/**', json({}))
      await page.route('**/sysinfo.json**', json(sysInfo()))
      await page.route('**/settings.json**', json({ settings: {} }))
      await page.route('**/api/update/status**', json(idleUpdateStatus()))
      await page.route('**/api/update/config**', json({ mode: 'notify', channel: 'stable' }))
      await page.route('**/api/monitoring**', json(monitoringConfig()))
      await page.route('**/api/log/status**', json({ enabled: false, persistent: false }))
      await page.route('**/api/log**', json(''))
      await page.route('**/api/events**', json(journal()))
      await page.route('**/api/loglevel**', json({ tags: [] }))
      await page.route('**/api/crash_log**', json({ available: false, tail: '' }))
    })

    for (const route of ROUTES) {
      test(`${theme} ${route}`, async ({ page }) => {
        await page.goto(`${BASE_URL}${route}`)
        await page.waitForTimeout(1200)
        await expect(page).toHaveScreenshot(`visual-${theme}${route.replace(/\//g, '_')}.png`, {
          fullPage: true,
          animations: 'disabled',
          caret: 'hide',
          maxDiffPixelRatio: 0.01
        })
      })
    }
  })
}
