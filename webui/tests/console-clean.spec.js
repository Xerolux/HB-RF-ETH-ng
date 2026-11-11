import { test, expect } from '@playwright/test'

// Zero-console regression guard: every route must load without a single
// browser console error or warning. The password-fields-outside-a-form DOM
// hint on /monitoring shipped unnoticed because no test looked at the
// console; this spec makes that class of regression visible again.
//
// All device APIs are mocked so a missing endpoint (404 on the dev server)
// cannot masquerade as an application defect. Playwright matches routes in
// reverse registration order, so the catch-all is registered first and the
// specific payloads below win.

const BASE_URL = 'http://127.0.0.1:1234'

const json = (body) => (route) =>
  route.fulfill({ contentType: 'application/json', body: JSON.stringify(body) })

const idleUpdateStatus = () => ({
  state: 'idle',
  everChecked: true,
  channel: 'stable',
  lastCheck: Date.now() / 1000,
  cooldownRemainingSec: 0,
  runningFirmware: '2.3.1',
  runningWebui: '1.1.1-Beta.2',
  latestFirmware: '2.3.1',
  latestWebui: '1.1.1-Beta.2',
  firmwareUpdateAvailable: false,
  webuiUpdateAvailable: false,
  notesUrl: '',
  lastError: '',
  lastSkipReason: '',
  install: { state: 'idle', target: 'firmware', progress: 0, version: '', error: '' },
  schedule: {
    mode: 'off',
    channel: 'stable',
    intervalHours: 24,
    windowStart: 3,
    windowLength: 2,
    idleOnly: true,
    nextCheckIn: 0,
    installBlocked: null,
    lastSeenCheck: 0,
    seenFirmware: '',
    seenWebui: '',
    pending: null,
    stage: null
  }
})

const settings = {
  adminUsername: 'admin',
  hostname: 'hb-rf-eth',
  useDHCP: true,
  localIP: '192.168.1.200',
  netmask: '255.255.255.0',
  gateway: '192.168.1.1',
  dns1: '192.168.1.1',
  dns2: '',
  ccuIP: '192.168.1.10',
  timesource: 0,
  dcfOffset: 0,
  gpsBaudrate: 9600,
  ntpServer: 'pool.ntp.org',
  ledBrightness: 50,
  ledPrograms: {},
  enableIPv6: false,
  ipv6Mode: 'auto',
  ipv6Address: '',
  ipv6PrefixLength: 64,
  ipv6Gateway: '',
  ipv6Dns1: '',
  ipv6Dns2: '',
  flashPause: false
}

const sysInfo = () => ({
  sysInfo: {
    hostname: 'HB-RF-ETH-TestRoma',
    serial: '2805A5114784',
    currentVersion: '2.3.1',
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

const monitoringConfig = () => ({
  checkmk: { enabled: false, port: 6556 },
  mqtt: { enabled: false },
  prometheus: { enabled: false },
  syslog: { enabled: false },
  notify: { enabled: false, channels: 0, eventMask: 0, eventMaskSupported: 0x0fff }
})

const ROUTES = [
  '/',
  '/monitoring',
  '/settings',
  '/system-overview',
  '/updates/firmware',
  '/updates/webui',
  '/diagnostics',
  '/systemlog',
  '/about'
]

test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => {
    if (!localStorage.getItem('locale')) localStorage.setItem('locale', 'en')
    sessionStorage.setItem('hb-rf-eth-ng-pw', 'device-secret-token')
    sessionStorage.setItem('hb-rf-eth-ng-last-activity', String(Date.now()))
  })

  // Catch-all first (reverse matching: later registrations win).
  await page.route('**/api/**', json({}))
  await page.route('**/sysinfo.json**', json(sysInfo()))
  await page.route('**/settings.json**', json({ settings }))
  await page.route('**/api/update/status**', json(idleUpdateStatus()))
  await page.route('**/api/update/config**', json({
    mode: 'notify', channel: 'stable', idleOnly: true, windowStart: 3, windowLength: 2
  }))
  await page.route('**/api/monitoring**', json(monitoringConfig()))
  await page.route('**/api/log/status**', json({ enabled: false, persistent: false }))
  await page.route('**/api/log**', json(''))
  await page.route('**/api/crash_log**', json({ available: false, tail: '' }))
})

for (const route of ROUTES) {
  test(`route ${route} loads with a clean console`, async ({ page }) => {
    const offenders = []
    page.on('console', (msg) => {
      if (msg.type() === 'error' || msg.type() === 'warning') {
        offenders.push(`[${msg.type()}] ${msg.text()}`)
      }
    })
    page.on('pageerror', (err) => offenders.push(`[pageerror] ${err.message}`))

    await page.goto(`${BASE_URL}${route}`)
    // Give lazy polls (update status, sysinfo refresh) a moment to fire.
    await page.waitForTimeout(1500)

    expect(offenders, offenders.join('\n')).toEqual([])
  })
}
