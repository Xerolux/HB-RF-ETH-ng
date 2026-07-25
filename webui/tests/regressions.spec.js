import { test, expect } from '@playwright/test'
import { readFile } from 'node:fs/promises'

const BASE_URL = 'http://127.0.0.1:1234'

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

const archive = {
  releases: [{
    id: 'v2.2.3',
    version: '2.2.3',
    tagName: 'v2.2.3',
    prerelease: false,
    publishedAt: '2026-07-09T00:00:00Z',
    downloadUrl: 'https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.2.3/firmware_2.2.3.bin'
  }]
}

test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => {
    if (!localStorage.getItem('locale')) localStorage.setItem('locale', 'en')
    sessionStorage.setItem('hb-rf-eth-ng-pw', 'device-secret-token')
    sessionStorage.setItem('hb-rf-eth-ng-last-activity', String(Date.now()))
  })

  await page.route('**/sysinfo.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ sysInfo: { currentVersion: '2.2.4-Beta.3' } })
  }))

  await page.route('**/api/check_update**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ latestVersion: '2.2.4-Beta.3', updateAvailable: false })
  }))
})

test('dashboard typography and summary-card footers stay aligned', async ({ page }) => {
  await page.route('**/sysinfo.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      sysInfo: {
        hostname: 'HB-RF-ETH-TestRoma',
        serial: '2805A5114784',
        currentVersion: '2.2.4-Beta.3',
        memoryUsage: 67,
        cpuUsage: 0,
        uptimeSeconds: 1397,
        boardRevision: 'REV 1.10/1.11 (PUB)',
        resetReason: 'Watchdog Reset (Interrupt Watchdog)',
        ethernetConnected: true,
        ethernetSpeed: 100,
        ethernetDuplex: 'Full',
        radioModuleType: 'RPI-RF-MOD',
        radioModuleSerial: '58A9A71150',
        radioModuleFirmwareVersion: '4.4.22',
        radioModuleBidCosRadioMAC: '0x123456',
        radioModuleHmIPRadioMAC: '0x654321',
        radioModuleSGTIN: '3014F711A0001F98A99AXXXX'
      }
    })
  }))

  await page.goto(`${BASE_URL}/`)
  await expect(page.locator('.dashboard .stats-grid')).toBeVisible()

  const typography = await page.locator('.dashboard').evaluate(element => {
    const bodyStyle = getComputedStyle(document.body)
    const values = [...element.querySelectorAll('.kv-value')].map(value => {
      const style = getComputedStyle(value)
      return { fontFamily: style.fontFamily, fontSize: style.fontSize }
    })
    return {
      bodyFontFamily: bodyStyle.fontFamily,
      bodyFontSize: bodyStyle.fontSize,
      values
    }
  })

  expect(typography.values.length).toBeGreaterThan(0)
  expect(typography.values.every(value => value.fontFamily === typography.bodyFontFamily)).toBe(true)
  expect(typography.values.every(value => value.fontSize === typography.bodyFontSize)).toBe(true)

  const footerBottoms = await page.locator('.dashboard .metric-footer').evaluateAll(footers =>
    footers.map(footer => footer.getBoundingClientRect().bottom)
  )
  expect(footerBottoms).toHaveLength(4)
  expect(Math.max(...footerBottoms) - Math.min(...footerBottoms)).toBeLessThan(1)
})

test('monitoring diagnostic rows use one consistent grey row size', async ({ page }) => {
  await page.route('**/api/monitoring', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      checkmk: { enabled: false, port: 6556 },
      mqtt: { enabled: false },
      prometheus: { enabled: false },
      syslog: { enabled: false },
      notify: { enabled: false, channels: 0 }
    })
  }))

  await page.goto(`${BASE_URL}/monitoring`)
  const rows = page.locator('.diagnostics-panel .diag-row')
  await expect(rows).toHaveCount(5)

  const rowStyles = await rows.evaluateAll(elements => elements.map(element => {
    const style = getComputedStyle(element)
    return {
      height: element.getBoundingClientRect().height,
      paddingTop: style.paddingTop,
      paddingBottom: style.paddingBottom,
      backgroundColor: style.backgroundColor
    }
  }))

  expect(new Set(rowStyles.map(row => row.height)).size).toBe(1)
  expect(new Set(rowStyles.map(row => row.paddingTop)).size).toBe(1)
  expect(new Set(rowStyles.map(row => row.paddingBottom)).size).toBe(1)
  expect(new Set(rowStyles.map(row => row.backgroundColor)).size).toBe(1)
  expect(rowStyles[0].backgroundColor).not.toBe('rgba(0, 0, 0, 0)')
})

test('firmware update page follows the selected language completely', async ({ page }) => {
  await page.goto(`${BASE_URL}/updates/firmware`)

  await expect(page.locator('.firmware-page .hero-title')).toHaveText('Update device firmware')
  await expect(page.locator('.firmware-page')).toContainText('Available device firmware')
  await expect(page.locator('.firmware-page')).toContainText('Install firmware manually')
  await expect(page.locator('.firmware-page')).not.toContainText('Firmware aktualisieren')
  await expect(page.locator('.firmware-page')).not.toContainText('Noch kein Prüfergebnis')

  await page.evaluate(() => localStorage.setItem('locale', 'de'))
  await page.reload()

  await expect(page.locator('.firmware-page .hero-title')).toHaveText('Geräte-Firmware aktualisieren')
  await expect(page.locator('.firmware-page')).toContainText('Verfügbare Geräte-Firmware')
  await expect(page.locator('.firmware-page')).toContainText('Firmware manuell installieren')
})

test('language picker exposes only supported locales and migrates a retired selection', async ({ page }) => {
  await page.addInitScript(() => localStorage.setItem('locale', 'es'))
  await page.goto(`${BASE_URL}/`)

  const storedLocale = await page.evaluate(() => localStorage.getItem('locale'))
  expect(['de', 'en', 'fr', 'it']).toContain(storedLocale)
  expect(storedLocale).not.toBe('es')

  await page.locator('.locale-picker .utility-row').click()
  const localeOptions = page.locator('.locale-picker .locale-menu-item')
  await expect(localeOptions).toHaveCount(4)
  await expect(page.locator('.locale-picker .locale-menu')).toContainText('Deutsch')
  await expect(page.locator('.locale-picker .locale-menu')).toContainText('English')
  await expect(page.locator('.locale-picker .locale-menu')).toContainText('Français')
  await expect(page.locator('.locale-picker .locale-menu')).toContainText('Italiano')
  await expect(page.locator('.locale-picker .locale-menu')).not.toContainText('Español')
})

test('factory reset requires the exact non-copyable case-sensitive challenge', async ({ page }) => {
  let factoryResetRequests = 0
  await page.route('**/api/factory-reset', route => {
    factoryResetRequests += 1
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ success: true })
    })
  })
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.addInitScript(() => {
    localStorage.setItem('theme', 'dark')
    localStorage.setItem('primaryColor', '#3971e8')
    localStorage.setItem('future-user-setting', 'must-be-erased')
    sessionStorage.setItem('future-session-setting', 'must-be-erased')
  })

  await page.goto(`${BASE_URL}/settings?tab=backup`)
  await page.locator('.system-action.danger').click()

  const dialog = page.getByRole('dialog', { name: 'Factory reset' })
  const challenge = dialog.getByTestId('factory-reset-challenge')
  const confirmation = dialog.getByLabel('Confirm security code')
  const resetButton = dialog.getByRole('button', { name: 'Reset to factory defaults' })

  await expect(dialog).toBeVisible()
  await expect(challenge).toHaveText(/^[A-Za-z0-9]{8}$/)
  await expect(resetButton).toBeDisabled()

  const challengeCode = (await challenge.textContent()).trim()
  expect(challengeCode).toMatch(/[A-Z]/)
  expect(challengeCode).toMatch(/[a-z]/)
  expect(challengeCode).toMatch(/[0-9]/)

  const copyProtection = await challenge.evaluate(element => {
    const copyEvent = new Event('copy', { bubbles: true, cancelable: true })
    element.dispatchEvent(copyEvent)
    return {
      copyPrevented: copyEvent.defaultPrevented,
      userSelect: getComputedStyle(element).userSelect
    }
  })
  expect(copyProtection.copyPrevented).toBe(true)
  expect(copyProtection.userSelect).toBe('none')

  const wrongCaseCode = challengeCode.replace(/[A-Z]/, character => character.toLowerCase())
  await confirmation.fill(wrongCaseCode)
  await expect(dialog.getByText('The entered code does not match exactly.')).toBeVisible()
  await expect(resetButton).toBeDisabled()
  expect(factoryResetRequests).toBe(0)

  await confirmation.fill(challengeCode)
  await expect(dialog.getByText('Code confirmed. Factory reset is now unlocked.')).toBeVisible()
  await expect(resetButton).toBeEnabled()

  await resetButton.click()
  await expect.poll(() => factoryResetRequests).toBe(1)
  await expect(page.locator('.restart-countdown-overlay')).toBeVisible()
  expect(await page.evaluate(() => ({
    theme: localStorage.getItem('theme'),
    primaryColor: localStorage.getItem('primaryColor'),
    token: sessionStorage.getItem('hb-rf-eth-ng-pw'),
    futureUserSetting: localStorage.getItem('future-user-setting'),
    futureSessionSetting: sessionStorage.getItem('future-session-setting')
  }))).toEqual({
    theme: null,
    primaryColor: null,
    token: null,
    futureUserSetting: null,
    futureSessionSetting: null
  })
})

test('downloaded backup keeps all sensitive device settings and browser preferences', async ({ page }) => {
  const completeDeviceBackup = {
    _format: 'hb-rf-eth-ng-backup',
    _version: 2,
    ...settings,
    adminPassword: 'Backup123',
    passwordChanged: true,
    theme: {
      colorScheme: 'dark',
      primaryColor: '#3971e8'
    },
    monitoring: {
      mqtt: {
        password: 'mqtt-secret',
        commandToken: 'Command123'
      },
      notify: {
        smtpPassword: 'smtp-secret'
      }
    }
  }
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/backup', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify(completeDeviceBackup)
  }))
  await page.addInitScript(() => {
    localStorage.setItem('locale', 'it')
    localStorage.setItem('showExperimental', '1')
  })

  await page.goto(`${BASE_URL}/settings?tab=backup`)
  const downloadPromise = page.waitForEvent('download')
  await page.locator('.backup-grid .action-tile').first().click()
  const download = await downloadPromise
  const downloadedBackup = JSON.parse(
    await readFile(await download.path(), 'utf8')
  )

  expect(download.suggestedFilename()).toBe('hb-rf-eth-ng-backup.json')
  expect(downloadedBackup.adminPassword).toBe('Backup123')
  expect(downloadedBackup.monitoring.mqtt.password).toBe('mqtt-secret')
  expect(downloadedBackup.monitoring.notify.smtpPassword).toBe('smtp-secret')
  expect(downloadedBackup.theme).toEqual(completeDeviceBackup.theme)
  expect(downloadedBackup.browserPreferences).toEqual({
    locale: 'it',
    showExperimental: true
  })
})

test('manual restart sends only one request and shows the synchronization countdown', async ({ page }) => {
  let restartRequests = 0
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/restart', route => {
    restartRequests += 1
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ success: true })
    })
  })

  await page.goto(`${BASE_URL}/settings?tab=backup`)
  await page.locator('.system-action.warning').click()

  const dialog = page.getByRole('dialog')
  await expect(dialog.getByRole('heading', { name: 'Restart Required' })).toBeVisible()
  const restartButton = dialog.getByRole('button', { name: 'Restart Now' })
  await expect(restartButton).toBeEnabled()
  await restartButton.evaluate(button => {
    button.click()
    button.click()
  })

  await expect.poll(() => restartRequests).toBe(1)
  await expect(page.locator('.restart-countdown-overlay')).toBeVisible()
  await expect(page.locator('.restart-countdown-overlay')).toContainText('Restart Sync')
})

test('settings restore is guarded against duplicates and enters restart synchronization', async ({ page }) => {
  let restoreRequests = 0
  let restoredPayload = null
  const completeBackup = {
    ...settings,
    adminPassword: 'Backup123',
    passwordChanged: true,
    theme: {
      colorScheme: 'dark',
      primaryColor: '#3971e8'
    },
    monitoring: {
      mqtt: {
        password: 'mqtt-secret',
        commandToken: 'Command123'
      }
    },
    browserPreferences: {
      locale: 'de',
      showExperimental: true
    }
  }
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/restore', route => {
    restoreRequests += 1
    restoredPayload = route.request().postDataJSON()
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ success: true })
    })
  })

  await page.goto(`${BASE_URL}/settings?tab=backup`)
  await page.evaluate(() => {
    window.confirm = () => true
  })
  await page.locator('input.file-input').setInputFiles({
    name: 'settings.json',
    mimeType: 'application/json',
    buffer: Buffer.from(JSON.stringify(completeBackup))
  })

  const restoreButton = page.getByRole('button', { name: 'Restore', exact: true })
  await restoreButton.evaluate(button => {
    button.click()
    button.click()
  })

  await expect.poll(() => restoreRequests).toBe(1)
  expect(restoredPayload.adminPassword).toBe('Backup123')
  expect(restoredPayload.monitoring.mqtt.password).toBe('mqtt-secret')
  expect(restoredPayload.theme).toEqual(completeBackup.theme)
  expect(await page.evaluate(() => ({
    locale: localStorage.getItem('locale'),
    showExperimental: localStorage.getItem('showExperimental')
  }))).toEqual({ locale: 'de', showExperimental: '1' })
  await expect(page.locator('.restart-countdown-overlay')).toBeVisible()
  await expect(page.locator('.restart-countdown-overlay')).toContainText('Restart Sync')
})

test('settings and system actions do not overflow a mobile viewport', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 })
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))

  await page.goto(`${BASE_URL}/settings?tab=backup`)
  await expect(page.getByRole('heading', { name: 'System actions' })).toBeVisible()

  const layout = await page.evaluate(() => ({
    viewportWidth: window.innerWidth,
    documentWidth: document.documentElement.scrollWidth,
    actionBounds: [...document.querySelectorAll('.system-action')].map(element => {
      const rect = element.getBoundingClientRect()
      return { left: rect.left, right: rect.right }
    })
  }))

  expect(layout.documentWidth).toBeLessThanOrEqual(layout.viewportWidth)
  expect(layout.actionBounds).toHaveLength(3)
  expect(layout.actionBounds.every(bounds =>
    bounds.left >= 0 && bounds.right <= layout.viewportWidth
  )).toBe(true)
})

test('network ping sends authentication and shows structured results', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  let authorization = ''
  await page.route('**/api/ping', route => {
    authorization = route.request().headers().authorization || ''
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ success: true, latency_ms: 7 })
    })
  })

  await page.goto(`${BASE_URL}/settings?tab=network`)
  await page.getByPlaceholder('192.168.1.1').fill('192.168.1.1')
  await page.getByRole('button', { name: 'Ping', exact: true }).click()

  await expect(page.getByText('Ping successful. Latency: 7 ms')).toBeVisible()
  expect(authorization).toBe('Token device-secret-token')
})

test('settings show field-level network and NTP validation messages', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings: { ...settings, useDHCP: false } })
  }))

  await page.goto(`${BASE_URL}/settings?tab=network`)
  const netmask = page.locator('label.form-label').filter({ hasText: /^Netmask$/ }).locator('..').locator('input')
  await netmask.fill('255.0.255.0')
  await page.locator('.floating-footer .save-btn').click()
  await expect(page.getByText('Invalid subnet mask. Its set bits must be contiguous.')).toBeVisible()

  await page.getByRole('button', { name: 'Time' }).click()
  const ntp = page.locator('label.form-label').filter({ hasText: /^NTP Server$/ }).locator('..').locator('input')
  await ntp.fill('bad host name')
  await expect(page.getByText('Invalid NTP server or port.')).toBeVisible()
})

test('wrong current password is shown in the active locale', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/change-password', route => route.fulfill({
    status: 403,
    contentType: 'application/json',
    body: JSON.stringify({ success: false, error: 'current_password_incorrect' })
  }))

  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Change Password' }).click()
  const dialog = page.getByRole('dialog')
  await dialog.locator('input[autocomplete="current-password"]').fill('Wrong123')
  const newPasswordInputs = dialog.locator('input[autocomplete="new-password"]')
  await newPasswordInputs.nth(0).fill('Correct123')
  await newPasswordInputs.nth(1).fill('Correct123')
  await dialog.getByRole('button', { name: 'Change Password' }).click()
  await expect(dialog.getByText('The current password is incorrect.')).toBeVisible()
})

test('updates sub-navigation uses the shared design and switches child routes', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/firmware_archive**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify(archive)
  }))
  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.6',
      effectiveVersion: '1.0.0-Beta.6',
      partitionSize: 327680,
      usedBytes: 250000
    })
  }))

  await page.goto(`${BASE_URL}/updates/firmware`)

  const navigation = page.locator('.updates-page > .tabs-container')
  const control = navigation.locator('.segmented-control')
  const firmwareTab = control.getByRole('link', { name: /Device firmware/ })
  const webUiTab = control.getByRole('link', { name: /WebUI/ })

  await expect(navigation).toBeVisible()
  await expect(navigation).toHaveCSS('display', 'flex')
  await expect(control).toHaveCSS('display', 'grid')
  await expect(firmwareTab).toHaveClass(/active/)
  await expect(firmwareTab).toHaveCSS('min-height', '68px')
  await expect(firmwareTab).toContainText('ESP32, network and radio')
  await expect(webUiTab).toContainText('Browser-based controls')
  await expect(page.locator('.firmware-page')).toBeVisible()
  await expect(page.locator('.firmware-page .content-grid > .update-card')).toHaveCount(2)
  await expect(page.locator('.firmware-page .update-card > .card-header')).toHaveCount(2)
  await expect(page.locator('.firmware-page .update-card > .card-body')).toHaveCount(2)

  const tabSizes = await control.locator('.segment-btn').evaluateAll(tabs => tabs.map(tab => ({
    width: tab.getBoundingClientRect().width,
    height: tab.getBoundingClientRect().height
  })))
  expect(tabSizes).toHaveLength(2)
  expect(Math.abs(tabSizes[0].width - tabSizes[1].width)).toBeLessThan(1)
  expect(tabSizes.every(tab => tab.height >= 44)).toBe(true)

  await webUiTab.click()
  await expect(page).toHaveURL(`${BASE_URL}/updates/webui`)
  await expect(webUiTab).toHaveClass(/active/)
  await expect(page.locator('.www-page')).toBeVisible()
  await expect(page.locator('.www-page .content-grid > .update-card')).toHaveCount(2)
  await expect(page.locator('.www-page .update-card > .card-header')).toHaveCount(2)
  await expect(page.locator('.www-page .update-card > .card-body')).toHaveCount(2)

  await page.setViewportSize({ width: 390, height: 844 })
  const mobileBounds = await control.evaluate(element => {
    const rect = element.getBoundingClientRect()
    return { left: rect.left, right: rect.right, viewportWidth: window.innerWidth }
  })
  expect(mobileBounds.left).toBeGreaterThanOrEqual(0)
  expect(mobileBounds.right).toBeLessThanOrEqual(mobileBounds.viewportWidth)
})

test('incompatible installed WebUI is never presented as active and shows a persistent repair warning', async ({ page }) => {
  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      mounted: true,
      manifestValid: true,
      compatible: false,
      valid: false,
      version: '1.1.0',
      effectiveVersion: '1.0.0-Beta.8',
      source: 'embedded',
      apiVersion: 2,
      supportedApiVersion: 1,
      minFirmwareVersion: '2.2.5-Beta.1',
      firmwareVersion: '2.2.5-Beta.12',
      compatibilityStatus: 'api_mismatch',
      partitionSize: 327680,
      usedBytes: 260000
    })
  }))

  await page.goto(`${BASE_URL}/`)

  const warning = page.getByTestId('webui-compatibility-warning')
  await expect(warning).toBeVisible()
  await expect(warning).toContainText('WebUI compatibility problem')
  await expect(warning).toContainText('requires API 2')
  await expect(warning).toContainText('firmware supports API 1')
  await expect(warning).toContainText('embedded WebUI is active')

  await warning.getByRole('link', { name: 'Repair WebUI' }).click()
  await expect(page).toHaveURL(`${BASE_URL}/updates/webui`)
})

test('firmware update banner does not promise a direct OTA installation', async ({ page }) => {
  await page.route('**/api/check_update**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      latestVersion: '2.2.5-Beta.12',
      updateAvailable: true,
      downloadUrl: 'https://example.invalid/firmware_2.2.5-Beta.12.bin'
    })
  }))

  await page.goto(`${BASE_URL}/`)

  const banner = page.locator('.update-banner')
  await expect(banner).toBeVisible()
  await expect(banner).toContainText('Download and manual installation')
  await expect(banner.getByRole('link', { name: 'View update' })).toBeVisible()
  await expect(banner).not.toContainText('Update Now')
})

test('release-matched WebUI upload sends compatibility preflight headers', async ({ page }) => {
  const releaseFile = 'webui_1.0.1.bin'
  let uploadHeaders = null

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      mounted: true,
      manifestValid: true,
      compatible: true,
      valid: true,
      version: '1.0.0',
      effectiveVersion: '1.0.0',
      source: 'spiffs',
      apiVersion: 1,
      supportedApiVersion: 1,
      minFirmwareVersion: '2.2.5-Beta.1',
      firmwareVersion: '2.2.5-Beta.12',
      compatibilityStatus: 'compatible',
      partitionSize: 327680,
      usedBytes: 250000
    })
  }))
  await page.route('**/api/check_update**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      latestVersion: '2.2.5-Beta.12',
      updateAvailable: false,
      fetchedAt: 1784750400000,
      webui: {
        version: '1.0.1',
        design: 'newdesign',
        apiVersion: 1,
        minFirmwareVersion: '2.2.5-Beta.1',
        size: 327680,
        sha256: 'a'.repeat(64),
        downloadUrl: `https://example.invalid/${releaseFile}`
      }
    })
  }))
  await page.route('**/api/webui/update', route => {
    uploadHeaders = route.request().headers()
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ success: true, version: '1.0.1' })
    })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  await page.locator('input[type="file"]').setInputFiles({
    name: releaseFile,
    mimeType: 'application/octet-stream',
    buffer: Buffer.alloc(327680)
  })
  await page.getByRole('button', { name: 'Install WebUI' }).click()

  await expect.poll(() => uploadHeaders).not.toBeNull()
  expect(uploadHeaders['x-webui-sha256']).toBe('a'.repeat(64))
  expect(uploadHeaders['x-webui-api-version']).toBe('1')
  expect(uploadHeaders['x-webui-min-firmware-version']).toBe('2.2.5-Beta.1')
})

test('firmware keeps a successful no-update result visible without a release version', async ({ page }) => {
  const settledSnapshot = {
    latestVersion: 'n/a',
    updateAvailable: false,
    fetchedAt: 1784750400000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(settledSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await expect(page.getByText('No check result yet')).toBeVisible()

  await page.getByRole('button', { name: 'Search for updates now' }).click()

  await expect(page.getByText('Firmware is up to date')).toBeVisible()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
})

test('WebUI keeps a successful no-update result visible without a release version', async ({ page }) => {
  const settledSnapshot = {
    latestVersion: 'n/a',
    updateAvailable: false,
    fetchedAt: 1784750400000,
    fetchInProgress: false,
    error: null,
    webui: {
      version: '',
      design: 'newdesign',
      apiVersion: 1,
      minFirmwareVersion: '2.2.5-Beta.1',
      size: 327680
    }
  }

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.7',
      effectiveVersion: '1.0.0-Beta.7',
      partitionSize: 327680,
      usedBytes: 292666
    })
  }))
  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(settledSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  await expect(page.getByText('The installed WebUI is up to date.')).toHaveCount(0)

  await page.getByRole('button', { name: 'Search for updates now' }).click()

  await expect(page.getByText('The installed WebUI is up to date.')).toBeVisible()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
})

test('update-search cooldown does not fabricate a successful result', async ({ page }) => {
  const emptySnapshot = {
    latestVersion: 'n/a',
    updateAvailable: false,
    fetchedAt: 0,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: false, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(emptySnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await page.getByRole('button', { name: 'Search for updates now' }).click()

  await expect(page.locator('.app-toast', { hasText: 'A check is already running or the 60-second cooldown is still active.' })).toBeVisible()
  await expect(page.getByText('No check result yet')).toBeVisible()
  await expect(page.getByText('Firmware is up to date')).toHaveCount(0)
})

test('firmware keeps manual no-update feedback visible when it was already current', async ({ page }) => {
  let postCount = 0
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      postCount += 1
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await expect(page.getByText('Firmware is up to date')).toBeVisible()
  const lastCheck = page.locator('.hero-meta .meta-chip').nth(1)
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
  expect(postCount).toBe(1)

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Everything is up to date')
  await expect(feedback).toContainText('No newer version is available.')
  await expect(lastCheck).not.toHaveText(beforeCheck || '')
})

test('WebUI keeps manual no-update feedback visible when it was already current', async ({ page }) => {
  let postCount = 0
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null,
    webui: {
      version: '1.0.0-Beta.7',
      design: 'newdesign',
      apiVersion: 1,
      minFirmwareVersion: '2.2.4-Beta.1',
      size: 327680
    }
  }

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.7',
      effectiveVersion: '1.0.0-Beta.7',
      partitionSize: 327680,
      usedBytes: 292666
    })
  }))
  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      postCount += 1
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  await expect(page.getByText('The installed WebUI is up to date.')).toBeVisible()
  const lastCheck = page.locator('.status-card').nth(2).locator('strong')
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
  expect(postCount).toBe(1)

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Everything is up to date')
  await expect(feedback).toContainText('No newer version is available.')
  await expect(lastCheck).not.toHaveText(beforeCheck || '')
})

test('cooldown feedback stays visible without advancing the successful check time', async ({ page }) => {
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: false, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  const lastCheck = page.locator('.hero-meta .meta-chip').nth(1)
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: '60-second cooldown' })).toBeVisible()

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Just a moment')
  await expect(feedback).toContainText('60-second cooldown')
  await expect(lastCheck).toHaveText(beforeCheck || '')
})

test('WebUI cooldown feedback stays visible without advancing the successful check time', async ({ page }) => {
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null,
    webui: {
      version: '1.0.0-Beta.7',
      design: 'newdesign',
      apiVersion: 1,
      minFirmwareVersion: '2.2.4-Beta.1',
      size: 327680
    }
  }

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.7',
      effectiveVersion: '1.0.0-Beta.7',
      partitionSize: 327680,
      usedBytes: 292666
    })
  }))
  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: false, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  const lastCheck = page.locator('.status-card').nth(2).locator('strong')
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: '60-second cooldown' })).toBeVisible()

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Just a moment')
  await expect(feedback).toContainText('60-second cooldown')
  await expect(lastCheck).toHaveText(beforeCheck || '')
})

test('error feedback stays visible without advancing the successful check time', async ({ page }) => {
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        status: 500,
        contentType: 'application/json',
        body: JSON.stringify({ error: 'TLS handshake failed' })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  const lastCheck = page.locator('.hero-meta .meta-chip').nth(1)
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: 'TLS handshake failed' }).last()).toBeVisible()

  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Check failed')
  await expect(feedback).toContainText('TLS handshake failed')
  await expect(lastCheck).toHaveText(beforeCheck || '')
})

test('settings save/discard state follows the edited form immediately', async ({ page }) => {
  const persisted = { ...settings }
  const getUrls = []
  const postUrls = []

  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'POST') {
      postUrls.push(route.request().url())
      Object.assign(persisted, route.request().postDataJSON())
      await route.fulfill({ contentType: 'application/json', body: JSON.stringify({ success: true }) })
      return
    }
    getUrls.push(route.request().url())
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings: persisted })
    })
  })

  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()

  const hostname = page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input')
  const footer = page.locator('.floating-footer')

  await expect(hostname).toHaveValue('hb-rf-eth')
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeDisabled()
  await expect(footer.locator('.save-btn')).toBeDisabled()

  await hostname.fill('changed-host')
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeEnabled()
  await expect(footer.locator('.save-btn')).toBeEnabled()

  let discardDialogShown = false
  page.once('dialog', dialog => {
    discardDialogShown = true
    dialog.dismiss()
  })
  await footer.locator('.discard-btn').click()
  await expect(hostname).toHaveValue('hb-rf-eth')
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeDisabled()
  await expect(footer.locator('.save-btn')).toBeDisabled()
  expect(discardDialogShown).toBe(false)

  await hostname.fill('saved-host')
  await expect(footer).toBeVisible()
  await footer.locator('.save-btn').click()
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeDisabled()
  await expect(footer.locator('.save-btn')).toBeDisabled()
  expect(persisted.hostname).toBe('saved-host')
  expect(getUrls.length).toBeGreaterThan(0)
  expect(getUrls.every(url => new URL(url).searchParams.has('t'))).toBe(true)
  expect(postUrls).toEqual([`${BASE_URL}/settings.json`])
})

test('late header initialization does not discard settings edits', async ({ page }) => {
  let releaseUpdateCheck
  const updateCheckGate = new Promise(resolve => {
    releaseUpdateCheck = resolve
  })
  let settingsGetCount = 0

  await page.route('**/api/check_update**', async route => {
    await updateCheckGate
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ latestVersion: '2.2.4-Beta.3', updateAvailable: false })
    })
  })
  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'GET') settingsGetCount++
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings })
    })
  })

  const updateResponse = page.waitForResponse(response => response.url().includes('/api/check_update'))
  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()

  const hostname = page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input')
  await expect(hostname).toHaveValue('hb-rf-eth')
  await hostname.fill('edit-must-survive')
  await expect(page.locator('.floating-footer')).toBeVisible()

  releaseUpdateCheck()
  await updateResponse
  await page.waitForTimeout(150)

  expect(settingsGetCount).toBe(1)
  await expect(hostname).toHaveValue('edit-must-survive')
  await expect(page.locator('.floating-footer')).toBeVisible()
})

for (const scenario of [
  {
    name: 'a firmware-valid hostname longer than 32 characters',
    overrides: { hostname: `hb-rf-eth-${'a'.repeat(30)}` }
  },
  {
    name: 'optional empty fields in static IPv6 mode',
    overrides: {
      enableIPv6: true,
      ipv6Mode: 'static',
      ipv6Address: '2001:db8::10',
      ipv6Gateway: '',
      ipv6Dns1: '',
      ipv6Dns2: ''
    }
  },
  {
    name: 'a firmware-valid IPv4-mapped IPv6 address',
    overrides: {
      enableIPv6: true,
      ipv6Mode: 'static',
      ipv6Address: '::ffff:192.168.1.10',
      ipv6Gateway: '2001:db8::1',
      ipv6Dns1: '2001:4860:4860::8888'
    }
  }
]) {
  test(`settings can still be saved with ${scenario.name}`, async ({ page }) => {
    const persisted = { ...settings, ...scenario.overrides }
    let postCount = 0

    await page.route('**/settings.json**', async route => {
      if (route.request().method() === 'POST') {
        postCount++
        Object.assign(persisted, route.request().postDataJSON())
        await route.fulfill({ contentType: 'application/json', body: JSON.stringify({ success: true }) })
        return
      }
      await route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ settings: persisted })
      })
    })

    await page.goto(`${BASE_URL}/settings`)
    const ccuIp = page.locator('.form-group', { hasText: 'CCU IP address' }).locator('input')
    await ccuIp.fill('192.168.1.11')
    await page.locator('.floating-footer .save-btn').click()

    await expect.poll(() => postCount).toBe(1)
    await expect(page.locator('.floating-footer')).toBeVisible()
    await expect(page.locator('.floating-footer .save-btn')).toBeDisabled()
  })
}

test('invalid settings reveal the affected tab instead of silently ignoring save', async ({ page }) => {
  let postCount = 0
  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'POST') {
      postCount++
      await route.fulfill({ contentType: 'application/json', body: JSON.stringify({ success: true }) })
      return
    }
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings })
    })
  })

  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()
  await page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input').fill('invalid_hostname')
  await page.getByRole('button', { name: 'General' }).click()
  await page.locator('.form-group', { hasText: 'CCU IP address' }).locator('input').fill('192.168.1.11')
  await page.locator('.floating-footer .save-btn').click()

  await expect(page.getByRole('button', { name: 'Network' })).toHaveClass(/active/)
  await expect(page.locator('.floating-footer .footer-alert')).toContainText('Please correct the highlighted fields.')
  expect(postCount).toBe(0)
})

test('OTA success dialog uses the active locale and has no default cancel button', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))

  await page.goto(BASE_URL)
  await page.evaluate(() => {
    localStorage.setItem('locale', 'de')
    localStorage.setItem('otaUpdateVersion', '2.2.4-Beta.5')
  })
  await page.reload()

  const dialog = page.getByRole('dialog')
  await expect(dialog).toContainText('Update erfolgreich')
  await expect(dialog).toContainText('Die Firmware wurde erfolgreich auf Version 2.2.4-Beta.5 aktualisiert.')
  await expect(dialog.getByRole('button', { name: 'Schließen', exact: true })).toHaveCount(1)
  await expect(dialog.getByRole('button', { name: /Cancel|Abbrechen/, exact: true })).toHaveCount(0)
})

test('firmware update page does not load the retired release archive', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))

  let localRequests = 0
  let externalRequests = 0
  await page.route('**/api/firmware_archive**', async route => {
    localRequests++
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify(archive)
    })
  })
  await page.route('https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/archive.json**', async route => {
    externalRequests++
    await route.abort()
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await page.waitForTimeout(250)
  expect(localRequests).toBe(0)
  expect(externalRequests).toBe(0)
  await expect(page.locator('.archive-refresh')).toHaveCount(0)
  await expect(page.locator('.archive-error')).toBeHidden()
})

test('crash log modal shows recovered tail once after a crash', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/log/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ enabled: false, persistent: false })
  }))
  await page.route('**/api/crash_log**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      available: true,
      tail: '[heap_watchdog] low heap: free=18432 largest=16384 uptime=84213s\nI (84213) mqtt: published status'
    })
  }))

  await page.goto(`${BASE_URL}/systemlog`)

  // The crash recovery modal auto-opens and shows the recovered tail.
  const modal = page.locator('.modal.show')
  await expect(modal).toBeVisible({ timeout: 5000 })
  await expect(modal).toContainText('[heap_watchdog] low heap')
  await expect(modal).toContainText('published status')

  // Close it.
  await modal.locator('.btn-primary', { hasText: /close/i }).click()
  await expect(modal).toBeHidden()

  // The backend clears the snapshot after the first read — a reload without
  // re-stubbing must not re-show it. The page falls back to the default
  // (empty) route handler here, which returns a 404-ish body; the modal
  // stays hidden.
  await page.unroute('**/api/crash_log**')
  await page.route('**/api/crash_log**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ available: false, tail: '' })
  }))
  await page.goto(`${BASE_URL}/systemlog`)
  await page.waitForTimeout(400)
  await expect(page.locator('.modal.show')).toHaveCount(0)
})

test('live log waits for the authenticated WebSocket acknowledgement and closes cleanly', async ({ page }) => {
  await page.addInitScript(() => {
    class FakeWebSocket {
      static instances = []

      constructor(url) {
        this.url = url
        this.readyState = 0
        FakeWebSocket.instances.push(this)
        queueMicrotask(() => {
          if (this.readyState !== 0) return
          this.readyState = 1
          this.onopen?.({})
        })
      }

      close() {
        if (this.readyState === 3) return
        this.readyState = 3
        queueMicrotask(() => this.onclose?.({ code: 1000 }))
      }

      emit(data) {
        this.onmessage?.({ data })
      }
    }

    window.WebSocket = FakeWebSocket
    window.__fakeWebSockets = FakeWebSocket.instances
  })

  let logGetCount = 0
  await page.route('**/api/log/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ enabled: true, persistent: true, subscribers: 0 })
  }))
  await page.route(/\/api\/log(?:\?.*)?$/, route => {
    logGetCount++
    return route.fulfill({
      contentType: 'text/plain',
      headers: { 'X-Log-Total': '0' },
      body: ''
    })
  })
  await page.route('**/api/log/disable**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ enabled: false })
  }))
  await page.route('**/api/crash_log**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ available: false, tail: '' })
  }))

  await page.goto(`${BASE_URL}/systemlog`)
  const logSocketCount = () => page.evaluate(() =>
    window.__fakeWebSockets.filter(socket => socket.url.includes('/api/log/stream')).length
  )
  await expect.poll(logSocketCount).toBe(1)
  expect(await page.evaluate(() =>
    window.__fakeWebSockets.find(socket => socket.url.includes('/api/log/stream')).url
  )).toContain(
    '/api/log/stream?token=device-secret-token&offset=0'
  )

  // A transport-level open or arbitrary data must not mark the application
  // stream ready. Only the server's post-handshake acknowledgement does.
  await page.evaluate(() =>
    window.__fakeWebSockets.find(socket => socket.url.includes('/api/log/stream')).emit('I (1) premature: ignored\n')
  )
  await expect(page.locator('.log-container')).not.toContainText('premature')

  await page.evaluate(() => {
    const socket = window.__fakeWebSockets.find(socket => socket.url.includes('/api/log/stream'))
    const backlog = 'I (1) app: snapshot backlog\n'
    const backlogBytes = new TextEncoder().encode(backlog).length
    socket.emit(`stream snapshot ${backlogBytes}\n`)
    socket.emit(`stream backlog ${backlogBytes}\n`)
    socket.emit(backlog)
    socket.emit(`stream connected ${backlogBytes}\n`)

    const live = 'I (2) app: live frame\n'
    const liveEnd = backlogBytes + new TextEncoder().encode(live).length
    socket.emit(`stream data ${liveEnd}\n${live}`)

    const colored = [
      '\u001b[0;31mE (3) app: colored error\u001b[0m\n',
      '\u001b[0;33mW (4) app: colored warning\u001b[0m\n',
      '\u001b[0;32mI (5) app: colored info\u001b[0m\n',
      '\u001b[0;36mD (6) app: colored debug\u001b[0m\n'
    ]
    let coloredEnd = liveEnd
    for (const line of colored) {
      coloredEnd += new TextEncoder().encode(line).length
      socket.emit(`stream data ${coloredEnd}\n${line}`)
    }
  })
  await expect(page.locator('.log-container')).toContainText('snapshot backlog')
  await expect(page.locator('.log-container')).toContainText('live frame')
  await expect(page.locator('.log-container')).not.toContainText('stream connected')
  await expect(page.locator('.log-container')).not.toContainText('stream snapshot')

  await page.locator('.level-filter').selectOption('E')
  await expect(page.locator('.log-container')).toContainText('colored error')
  await expect(page.locator('.log-container')).not.toContainText('colored warning')
  await page.locator('.level-filter').selectOption('W')
  await expect(page.locator('.log-container')).toContainText('colored warning')
  await page.locator('.level-filter').selectOption('I')
  await expect(page.locator('.log-container')).toContainText('colored info')
  await page.locator('.level-filter').selectOption('D')
  await expect(page.locator('.log-container')).toContainText('colored debug')
  await page.locator('.level-filter').selectOption('all')

  // Once acknowledged, the offset-aware stream is authoritative. The old
  // setInterval captured the initial false state and kept polling every 5s.
  await page.waitForTimeout(5500)
  expect(logGetCount).toBe(1)

  // Intentional shutdown must not let the socket's late close event create a
  // new reconnect loop.
  await page.locator('.toggle-chip input').uncheck()
  await page.waitForTimeout(2200)
  expect(await logSocketCount()).toBe(1)
})
