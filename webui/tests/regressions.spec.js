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

test('monitoring repairs an empty-NVS CheckMK port before MQTT is saved', async ({ page }) => {
  let postedConfig = null
  await page.route('**/api/monitoring', route => {
    if (route.request().method() === 'POST') {
      postedConfig = route.request().postDataJSON()
      if (postedConfig.checkmk.port === 0) {
        return route.fulfill({
          status: 400,
          contentType: 'application/json',
          body: JSON.stringify({ error: 'Invalid CheckMK port' })
        })
      }
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ success: true })
      })
    }
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({
        checkmk: { enabled: false, port: 0, allowedHosts: '' },
        mqtt: {
          enabled: false,
          server: '',
          port: 1883,
          user: '',
          password: '',
          topicPrefix: 'hb-rf-eth-ng',
          haDiscoveryEnabled: false,
          haDiscoveryPrefix: 'homeassistant',
          tlsEnable: false,
          tlsSkipVerify: false,
          commandEnabled: true
        },
        prometheus: { enabled: false, port: 9100, allowedHosts: '*' },
        syslog: { enabled: false, server: '', port: 514, transport: 0, minSeverity: 6, hostname: '' },
        notify: { enabled: false, channels: 0, smtpPort: 587, smtpTls: 1, cooldownSeconds: 300 }
      })
    })
  })

  await page.goto(`${BASE_URL}/monitoring`)
  const mqttCard = page.locator('.settings-card', { has: page.getByRole('heading', { name: 'MQTT Client' }) })
  await mqttCard.locator('input[type="checkbox"]').first().check()
  await mqttCard.getByRole('textbox').first().fill('broker.example.test')
  await page.getByRole('button', { name: 'Save', exact: true }).click()

  await expect.poll(() => postedConfig).not.toBeNull()
  expect(postedConfig.checkmk.port).toBe(6556)
  expect(postedConfig.mqtt.server).toBe('broker.example.test')
  await expect(page.locator('.app-toast', { hasText: 'Configuration saved successfully' })).toBeVisible()
})

test('settings tabs and ping controls use desktop and mobile space responsively', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))

  await page.setViewportSize({ width: 1280, height: 900 })
  await page.goto(`${BASE_URL}/settings?tab=network`)
  await expect(page.getByRole('heading', { name: 'Network Settings' })).toBeVisible()

  const desktopTabs = await page.locator('.segment-btn').evaluateAll(buttons =>
    buttons.map(button => button.getBoundingClientRect().top)
  )
  expect(new Set(desktopTabs.map(top => Math.round(top))).size).toBe(1)

  const desktopPing = await page.locator('.ping-controls').evaluate(element => {
    const input = element.querySelector('input').getBoundingClientRect()
    const button = element.querySelector('button').getBoundingClientRect()
    return {
      inputBottom: input.bottom,
      buttonBottom: button.bottom,
      buttonWidth: button.width
    }
  })
  expect(Math.abs(desktopPing.inputBottom - desktopPing.buttonBottom)).toBeLessThan(1)
  expect(desktopPing.buttonWidth).toBeGreaterThanOrEqual(140)

  await page.setViewportSize({ width: 390, height: 844 })
  const mobilePing = await page.locator('.ping-controls').evaluate(element => {
    const input = element.querySelector('input').getBoundingClientRect()
    const button = element.querySelector('button').getBoundingClientRect()
    return {
      inputLeft: input.left,
      inputWidth: input.width,
      inputBottom: input.bottom,
      buttonLeft: button.left,
      buttonWidth: button.width,
      buttonTop: button.top
    }
  })
  expect(mobilePing.buttonTop).toBeGreaterThan(mobilePing.inputBottom)
  expect(Math.abs(mobilePing.inputLeft - mobilePing.buttonLeft)).toBeLessThan(1)
  expect(Math.abs(mobilePing.inputWidth - mobilePing.buttonWidth)).toBeLessThan(1)

  const mobileTabs = await page.locator('.segment-btn').evaluateAll(buttons => ({
    rows: new Set(buttons.map(button => Math.round(button.getBoundingClientRect().top))).size,
    clipped: buttons.some(button => button.scrollWidth > button.clientWidth)
  }))
  expect(mobileTabs.rows).toBe(3)
  expect(mobileTabs.clipped).toBe(false)
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
        enabled: true,
        server: 'broker.example.test',
        port: 8883,
        user: 'mqtt-user',
        password: 'mqtt-secret',
        topicPrefix: 'home/gateway',
        haDiscoveryEnabled: true,
        haDiscoveryPrefix: 'homeassistant',
        tlsEnable: true,
        tlsSkipVerify: false,
        tlsCaCerts: 'CA CERTIFICATE',
        tlsCertfile: 'CLIENT CERTIFICATE',
        tlsKeyfile: 'CLIENT PRIVATE KEY',
        commandEnabled: true,
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
  await expect(page.locator('.segment-btn', { hasText: 'Backup e reset' })).toBeVisible()
  await expect(page.getByRole('alert')).toContainText('credenziali in chiaro')
  const downloadPromise = page.waitForEvent('download')
  await page.locator('.backup-grid .action-tile').first().click()
  const download = await downloadPromise
  const downloadedBackup = JSON.parse(
    await readFile(await download.path(), 'utf8')
  )

  expect(download.suggestedFilename()).toBe('hb-rf-eth-ng-backup.json')
  expect(downloadedBackup.adminPassword).toBe('Backup123')
  expect(downloadedBackup.monitoring.mqtt).toEqual(completeDeviceBackup.monitoring.mqtt)
  expect(downloadedBackup.monitoring.notify.smtpPassword).toBe('smtp-secret')
  expect(downloadedBackup.theme).toEqual(completeDeviceBackup.theme)
  expect(downloadedBackup._security.containsPlaintextSecrets).toBe(true)
  expect(downloadedBackup._security.warning).toContain('in chiaro')
  expect(downloadedBackup._portability.editable).toBe(true)
  expect(downloadedBackup._portability.warning).toContain('hostname')
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
    hostname: 'template-device-02',
    adminPassword: 'Backup123',
    passwordChanged: true,
    _security: {
      containsPlaintextSecrets: true,
      warning: 'Contains plaintext credentials'
    },
    _portability: {
      editable: true,
      warning: 'Review device-specific values'
    },
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
  expect(restoredPayload.hostname).toBe('template-device-02')
  expect(restoredPayload._portability.editable).toBe(true)
  expect(await page.evaluate(() => ({
    locale: localStorage.getItem('locale'),
    showExperimental: localStorage.getItem('showExperimental')
  }))).toEqual({ locale: 'de', showExperimental: '1' })
  await expect(page.locator('.restart-countdown-overlay')).toBeVisible()
  await expect(page.locator('.restart-countdown-overlay')).toContainText('Restart Sync')
})

test('recovery mirrors the current New Design shell and links back to the normal WebUI', async () => {
  const source = await readFile('../main/system_overview_api.cpp', 'utf8')
  const recoveryPage = source.slice(
    source.indexOf('constexpr char RECOVERY_PAGE[]'),
    source.indexOf(')HTML";')
  )
  const backLink = '<a class="back-link" href="/">← Zur normalen WebUI</a>'

  expect(recoveryPage).toContain(backLink)
  expect(recoveryPage.indexOf(backLink)).toBeLessThan(recoveryPage.indexOf('id="loginCard"'))
  expect(recoveryPage.indexOf(backLink)).toBeLessThan(recoveryPage.indexOf('id="tools"'))
  expect(recoveryPage).toContain('class="recovery-shell"')
  expect(recoveryPage).toContain('class="desktop-sidebar"')
  expect(recoveryPage).toContain('class="header-nav"')
  expect(recoveryPage).toContain('class="brand-logo"')
  expect(recoveryPage).toContain('class="hero-eyebrow"')
  expect(recoveryPage).toContain('--newdesign-sidebar-width:360px')
  expect(recoveryPage).toContain('--newdesign-header-height:88px')
  expect(recoveryPage).toContain('--newdesign-radius-card:4px')
  expect(recoveryPage).toContain('@media(prefers-color-scheme:dark)')
  expect(recoveryPage).toContain('--newdesign-panel:#fff')
  expect(recoveryPage).not.toContain('class="brand-mark">RF')
  expect(recoveryPage).not.toContain('class="recovery-header"')
  expect(recoveryPage).not.toContain('ohne New-Design-Bundle')
})

test('embedded recovery authenticates, adopts the device theme and reveals its tools', async ({ page }) => {
  const source = await readFile('../main/system_overview_api.cpp', 'utf8')
  const html = source.match(/R"HTML\(([\s\S]*?)\)HTML"/)?.[1]
  expect(html).toBeTruthy()

  await page.route('http://recovery.test/recovery', route => route.fulfill({
    contentType: 'text/html',
    body: html
  }))
  await page.route('http://recovery.test/api/theme', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ colorScheme: 'light', primaryColor: '#4fa36c' })
  }))
  await page.route('http://recovery.test/login.json', route => {
    expect(route.request().postDataJSON()).toEqual({
      username: 'admin',
      password: 'recovery-secret'
    })
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ isAuthenticated: true, token: 'recovery-token' })
    })
  })
  await page.route(/http:\/\/recovery\.test\/sysinfo\.json.*/, route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      sysInfo: {
        hostname: 'HB-RF-ETH-Recovery',
        currentVersion: '2.2.5-Beta.18',
        uptimeSeconds: 600
      }
    })
  }))
  await page.route('http://recovery.test/api/system/overview', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      totalInternalHeap: 320000,
      usedInternalHeap: 250000,
      internalHeapUsagePercent: 78.125,
      freeInternalHeap: 70000,
      minimumFreeHeap: 52000,
      largestFreeBlock: 32000,
      resetReasonText: 'Software reset',
      psramAvailable: false,
      webui: { version: '1.0.0-Beta.15' },
      logs: { crashTailAvailable: false, enabled: true, bufferBytes: 32768 }
    })
  }))

  await page.goto('http://recovery.test/recovery')
  await expect(page.locator('.desktop-sidebar')).toBeVisible()
  await expect(page.locator('.header-nav')).toBeVisible()
  await expect.poll(() => page.evaluate(
    () => getComputedStyle(document.documentElement).getPropertyValue('--color-primary').trim()
  )).toBe('#4fa36c')

  await page.locator('#pass').fill('recovery-secret')
  await page.locator('#login').click()

  await expect(page.locator('#loginCard')).toBeHidden()
  await expect(page.locator('#tools')).toBeVisible()
  await expect(page.locator('#deviceName')).toHaveText('HB-RF-ETH-Recovery')
  await expect(page.locator('#topFirmware')).toHaveText('2.2.5-Beta.18')
  await expect(page.locator('#facts')).toContainText('32.0 KB')
  await expect(page.locator('#status')).toHaveText('Status aktualisiert.')
})

test('MQTT exposes restart only and retires old update entities', async () => {
  const mqttSource = await readFile('../main/mqtt_handler.cpp', 'utf8')
  const commandHandler = mqttSource.slice(
    mqttSource.indexOf('static void handle_mqtt_command'),
    mqttSource.indexOf('static void mqtt_event_handler')
  )
  const discovery = mqttSource.slice(
    mqttSource.indexOf('void mqtt_handler_publish_ha_discovery'),
    mqttSource.indexOf('esp_err_t mqtt_handler_init')
  )
  expect(commandHandler).toContain('strcmp(command, "restart")')
  expect(commandHandler).not.toContain('triggerManualFetch()')
  expect(discovery).toContain('remove_config("button", "check_update")')
  expect(discovery).toContain('remove_config("update", "firmware_update")')
})

test('MQTT defers every publish until the broker connection is established', async () => {
  const mqttSource = await readFile('../main/mqtt_handler.cpp', 'utf8')
  const publishTask = mqttSource.slice(
    mqttSource.indexOf('void mqtt_publish_task'),
    mqttSource.indexOf('void mqtt_handler_trigger_status_publish')
  )
  const connectedEvent = mqttSource.slice(
    mqttSource.indexOf('case MQTT_EVENT_CONNECTED:'),
    mqttSource.indexOf('case MQTT_EVENT_DISCONNECTED:')
  )

  const canPublish = mqttSource.slice(
    mqttSource.indexOf('static bool mqtt_can_publish()'),
    mqttSource.indexOf('static int mqtt_publish_connected(')
  )
  const publishWrapper = mqttSource.slice(
    mqttSource.indexOf('static int mqtt_publish_connected('),
    mqttSource.indexOf('// Serializes mqtt_handler_start / mqtt_handler_stop')
  )

  // The guard itself must consider both lifecycle bits. The NULL-client check
  // lives in the wrapper below rather than in this predicate, so assert each
  // where it actually is instead of pinning one combined expression.
  expect(canPublish).toContain('mqtt_running.load(std::memory_order_acquire)')
  expect(canPublish).toContain('mqtt_connected.load(std::memory_order_acquire)')

  // Exactly one raw publish call, and it sits inside the wrapper behind both
  // lifecycle bits and a non-NULL client. The wrapper repeats the checks with
  // seq_cst ordering instead of calling mqtt_can_publish(), so that the
  // publisher count pairs with cleanup's mqtt_running=false store.
  expect(mqttSource.match(/esp_mqtt_client_publish\(/g)).toHaveLength(1)
  expect(publishWrapper).toContain('esp_mqtt_client_publish(')
  expect(publishWrapper).toContain('mqtt_running.load(std::memory_order_seq_cst)')
  expect(publishWrapper).toContain('mqtt_connected.load(std::memory_order_acquire)')
  expect(publishWrapper).toContain('if (publish_client != NULL)')
  expect(publishWrapper).toContain('mqtt_active_publishers.fetch_add(1, std::memory_order_seq_cst)')
  expect(publishWrapper).toContain('mqtt_active_publishers.fetch_sub(1, std::memory_order_seq_cst)')

  // While disconnected the task must wait instead of formatting and
  // submitting a batch. The wait is a bounded, notifiable take rather than a
  // plain vTaskDelay so a reconnect wakes it immediately.
  expect(publishTask).toContain('if (!mqtt_can_publish())')
  expect(publishTask).toMatch(/if \(!mqtt_can_publish\(\)\) \{\s*\(void\)ulTaskNotifyTake\(pdTRUE, pdMS_TO_TICKS\(\d+\)\);\s*continue;/)
  expect(connectedEvent.indexOf('mqtt_connected.store(true)')).toBeLessThan(
    connectedEvent.indexOf('mqtt_handler_publish_status()')
  )
})

test('MQTT startup waits for Ethernet IPv4 readiness without losing the GOT_IP race', async () => {
  const monitoringSource = await readFile('../main/monitoring.cpp', 'utf8')
  const init = monitoringSource.slice(
    monitoringSource.indexOf('esp_err_t monitoring_init'),
    monitoringSource.indexOf('// Update configuration')
  )
  const networkReadyHandler = monitoringSource.slice(
    monitoringSource.indexOf('static void mqtt_network_ready_handler'),
    monitoringSource.indexOf('// Initialize monitoring subsystem')
  )
  const retryTask = monitoringSource.slice(
    monitoringSource.indexOf('static void mqtt_retry_task('),
    // Anchor on the definition, not the forward declaration above it.
    monitoringSource.indexOf('static void schedule_mqtt_retry()\n{')
  )

  expect(monitoringSource).toContain('static std::atomic<bool> mqtt_start_deferred{false}')
  expect(monitoringSource).toContain('static void start_mqtt_when_network_ready')
  expect(monitoringSource).toContain('IP_EVENT_ETH_GOT_IP')
  expect(monitoringSource).toContain('MQTT start deferred until IPv4 is ready')
  expect(monitoringSource).toContain('starting MQTT with reconnect fallback')

  // The GOT_IP handler runs on the default event loop, so it must only arm the
  // retry worker. Starting MQTT inline would hold mqtt_lifecycle_mutex for up
  // to 15 s and starve every other event subscriber (Ethernet link, NTP, ...).
  expect(networkReadyHandler).toContain('IP_EVENT_ETH_GOT_IP')
  expect(networkReadyHandler).toContain('mqtt_start_deferred.load(std::memory_order_acquire)')
  expect(networkReadyHandler).toContain('schedule_mqtt_retry()')
  expect(networkReadyHandler).not.toContain('mqtt_handler_start(')
  expect(networkReadyHandler).not.toContain('malloc(')

  // The worker owns the config snapshot. It must be heap-allocated rather than
  // a stack copy, because mqtt_config_t is far too large for the worker stack.
  expect(retryTask).toContain('malloc(sizeof(mqtt_config_t))')
  expect(retryTask).not.toContain('mqtt_config_t snapshot;')
  expect(retryTask).toContain('mqtt_handler_start(snapshot)')
  expect(retryTask).toContain('free(snapshot)')

  expect(init).toContain('esp_event_handler_instance_register(')
  expect(init).toContain('start_mqtt_when_network_ready(&current_config.mqtt)')
  expect(init).not.toContain('mqtt_handler_start(&current_config.mqtt)')
})

test('notification event selection stays consistent across firmware, API and WebUI', async () => {
  const monitoringHeader = await readFile('../include/monitoring.h', 'utf8')
  const eventsHeader = await readFile('../include/events.h', 'utf8')
  const eventsSource = await readFile('../main/events.cpp', 'utf8')
  const configSource = await readFile('../main/monitoring_config.cpp', 'utf8')
  const apiSource = await readFile('../main/monitoring_api.cpp', 'utf8')
  const componentSource = await readFile('src/monitoring.vue', 'utf8')

  // Every NOTIFY_EVENT_* bit the firmware defines must be mapped by
  // events_mask_bit() and offered by the WebUI. A bit defined but unmapped
  // would be permanently unfilterable; a bit mapped but not offered would be
  // impossible to switch off.
  const firmwareBits = [...monitoringHeader.matchAll(/#define (NOTIFY_EVENT_[A-Z0-9_]+)\s+\(1u << (\d+)\)/g)]
    .map(match => ({ name: match[1], shift: Number(match[2]) }))
  expect(firmwareBits.length).toBeGreaterThan(0)

  for (const bit of firmwareBits) {
    expect(eventsSource).toContain(`return ${bit.name};`)
    expect(componentSource).toContain(`{ bit: 1 << ${bit.shift},`)
  }

  const uiBits = [...componentSource.matchAll(/\{ bit: 1 << (\d+),/g)].map(match => Number(match[1]))
  expect(uiBits.sort((a, b) => a - b))
    .toEqual(firmwareBits.map(bit => bit.shift).sort((a, b) => a - b))

  // NOTIFY_EVENT_ALL must actually cover every defined bit, since it is both
  // the factory default and the value an upgrading device keeps.
  const allMatch = monitoringHeader.match(/#define NOTIFY_EVENT_ALL\s+\(\(uint16_t\)0x([0-9A-Fa-f]+)u\)/)
  expect(allMatch).not.toBeNull()
  const expectedAll = firmwareBits.reduce((mask, bit) => mask | (1 << bit.shift), 0)
  expect(parseInt(allMatch[1], 16)).toBe(expectedAll)

  // The test notification must stay unfilterable, otherwise the diagnostic
  // button silently does nothing once a user deselects everything.
  expect(eventsHeader).toContain('EVENT_TEST')
  expect(eventsSource).toMatch(/default:\s*return 0;/)

  // Defaults and migration: a fresh config selects everything, and the NVS
  // loader must allow a stored zero so "notify nothing" survives a reboot.
  expect(configSource).toContain('config->notify.event_mask = NOTIFY_EVENT_ALL')
  expect(configSource).toContain('config->notify.event_mask &= NOTIFY_EVENT_ALL')

  // The API both reports the selection and advertises what the firmware
  // supports, so a newer WebUI does not offer events the device cannot emit.
  expect(apiSource).toContain('cJSON_AddNumberToObject(notify, "eventMask"')
  expect(apiSource).toContain('cJSON_AddNumberToObject(notify, "eventMaskSupported", NOTIFY_EVENT_ALL)')
  expect(apiSource).toContain('Invalid notification event mask')
})

test('CCU connection changes and low heap are emitted as notifiable events', async () => {
  const listenerSource = await readFile('../main/rawuartudplistener.cpp', 'utf8')
  const monitoringSource = await readFile('../main/monitoring.cpp', 'utf8')

  // Both connect paths report the same event; both disconnect paths report
  // the same event with distinguishing detail. An explicit CCU disconnect and
  // a silent keep-alive timeout are very different failures and the
  // notification has to say which one happened.
  expect(listenerSource.match(/events_emit\(EVENT_CCU_CONNECTED/g)).toHaveLength(2)
  expect(listenerSource.match(/events_emit\(EVENT_CCU_DISCONNECTED/g)).toHaveLength(2)
  expect(listenerSource).toContain('CCU sent an explicit disconnect')
  expect(listenerSource).toContain('no keep-alive from the CCU for 10 seconds')

  // The low-heap warning has to leave the device before the watchdog reboots
  // it, so it is emitted on the first hit of the streak and not next to
  // esp_restart().
  expect(monitoringSource).toContain('events_emit(EVENT_LOW_HEAP')
  const watchdog = monitoringSource.slice(
    monitoringSource.indexOf('static void heap_watchdog_task'),
    monitoringSource.indexOf('static void schedule_mqtt_retry();')
  )
  expect(watchdog.indexOf('events_emit(EVENT_LOW_HEAP'))
    .toBeLessThan(watchdog.indexOf('esp_restart()'))
  expect(watchdog).toContain('if (low_heap_streak == 1)')
})

test('CCU relay latency is measured in the receive path and reported to MQTT', async () => {
  const listenerSource = await readFile('../main/rawuartudplistener.cpp', 'utf8')
  const udpHelper = await readFile('../include/udphelper.h', 'utf8')
  const mqttSource = await readFile('../main/mqtt_handler.cpp', 'utf8')
  const metricsSource = await readFile('../main/metrics.cpp', 'utf8')

  // The timestamp has to be taken in the lwIP callback, not in the handler
  // task — taking it after dequeue would measure nothing at all, since the
  // whole point is the gap between those two moments.
  expect(udpHelper).toContain('uint32_t enqueued_us')
  const receiveCallback = listenerSource.slice(
    listenerSource.indexOf('bool RawUartUdpListener::_udpReceivePacket'),
    listenerSource.indexOf('Index 0 - Type:')
  )
  expect(receiveCallback).toContain('event.enqueued_us = (uint32_t)esp_timer_get_time()')

  // Unsigned subtraction, so the 32-bit microsecond wraparound is not a
  // special case. A signed or widened subtraction here would report absurd
  // latencies roughly every 71 minutes.
  expect(listenerSource).toContain('(uint32_t)esp_timer_get_time() - event.enqueued_us')
  expect(listenerSource).toContain('g_queue_wait_max.record(waited_us)')
  expect(listenerSource).toContain('g_queue_depth_max.record')

  // Disjoint buckets: an else-if chain, so one slow datagram counts once.
  expect(listenerSource).toMatch(
    /if \(waited_us > 1000000u\) \{[\s\S]*?\} else if \(waited_us > 100000u\) \{[\s\S]*?\} else if \(waited_us > 10000u\) \{/
  )

  // The gauge must be a genuine high-water mark, not a last-value store.
  expect(metricsSource).toContain('compare_exchange_weak')
  expect(metricsSource).toContain('while (value > observed)')
  expect(metricsSource).toContain('# TYPE %s gauge')

  // Reported to MQTT as well as Prometheus: the users hitting this are
  // watching Home Assistant, not scraping /metrics.
  expect(mqttSource).toContain('status/ccu_queue_wait_max_ms')
  expect(mqttSource).toContain('status/ccu_queue_depth_max')
  expect(mqttSource).toContain('status/ccu_delayed_frames')
  expect(mqttSource).toContain('status/ccu_dropped_frames')
  expect(mqttSource).toContain('publish_config("sensor", "ccu_queue_wait_max_ms"')
})

test('Raw-UART receive path avoids per-packet heap churn for ordinary frames', async () => {
  const source = await readFile('../main/rawuartudplistener.cpp', 'utf8')

  expect(source).not.toContain('#include <vector>')
  expect(source).not.toContain('malloc(sizeof(udp_event_t))')
  // 32 slots, not 64: the descriptor lives in the queue itself, and the
  // deeper reserve cost ~1 KB of heap for no observed benefit on a single
  // CCU-3 session (see "harden firmware for 2.2.6-Beta.4").
  expect(source).toContain('xQueueCreate(32, sizeof(udp_event_t))')
  expect(source).toContain('unsigned char small_data[256]')
  expect(source).toContain('if (length > sizeof(small_data))')
  expect(source).toContain('if (!heap_data.value)')
})

test('project documentation advertises exactly the four shipped locales', async () => {
  const localeIndex = await readFile('src/locales/index.js', 'utf8')
  const localeCodes = [...localeIndex.matchAll(/\{ code: '([a-z]{2})'/g)].map(match => match[1])
  expect(localeCodes).toEqual(['de', 'en', 'fr', 'it'])

  const documentation = await Promise.all([
    readFile('../README.md', 'utf8'),
    readFile('../CHANGELOG.md', 'utf8'),
    readFile('../docs/WIKI.md', 'utf8'),
    readFile('../generate_release_notes.py', 'utf8'),
    readFile('../release/RELEASE_NOTES.md', 'utf8')
  ])
  const combined = documentation.join('\n')

  expect(combined).not.toMatch(/10[ -]?(?:Sprachen|languages|locales)/i)
  expect(documentation[0]).toContain('Deutsch, Englisch, Französisch und Italienisch')
  expect(documentation[2]).toContain('4 Sprachen: Deutsch, Englisch, Französisch und Italienisch')
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

test('firmware page uses GitHub discovery and a local file without device-side search', async ({ page }) => {
  let retiredSearchRequests = 0
  await page.route('**/api/check_update**', route => {
    retiredSearchRequests++
    return route.abort()
  })

  await page.goto(`${BASE_URL}/updates/firmware`)

  const firmwarePage = page.locator('.firmware-page')
  await expect(firmwarePage.getByRole('link', { name: 'View on GitHub' })).toHaveAttribute(
    'href',
    'https://github.com/Xerolux/HB-RF-ETH-ng/releases'
  )
  await expect(firmwarePage).toContainText('Install firmware manually')
  await expect(firmwarePage.locator('input[type="file"]')).toHaveCount(1)
  await expect(firmwarePage.getByRole('button', { name: /Search for updates/i })).toHaveCount(0)
  expect(retiredSearchRequests).toBe(0)
})

test('manual WebUI upload uses a raw local image without release metadata', async ({ page }) => {
  let uploadHeaders = null
  let retiredSearchRequests = 0

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
  await page.route('**/api/check_update**', route => {
    retiredSearchRequests++
    return route.abort()
  })
  await page.route('**/api/webui/update', route => {
    uploadHeaders = route.request().headers()
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ success: true, version: '1.0.1' })
    })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  await page.locator('input[type="file"]').setInputFiles({
    name: 'webui_1.0.1.bin',
    mimeType: 'application/octet-stream',
    buffer: Buffer.alloc(327680)
  })
  await page.getByRole('button', { name: 'Install WebUI' }).click()

  await expect.poll(() => uploadHeaders).not.toBeNull()
  expect(uploadHeaders['content-type']).toContain('application/octet-stream')
  expect(uploadHeaders['x-webui-sha256']).toBeUndefined()
  expect(uploadHeaders['x-webui-api-version']).toBeUndefined()
  expect(uploadHeaders['x-webui-min-firmware-version']).toBeUndefined()
  expect(retiredSearchRequests).toBe(0)
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
  let releaseSysInfo
  const sysInfoGate = new Promise(resolve => {
    releaseSysInfo = resolve
  })
  let settingsGetCount = 0

  await page.route('**/sysinfo.json**', async route => {
    await sysInfoGate
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ sysInfo: { currentVersion: '2.2.4-Beta.3' } })
    })
  })
  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'GET') settingsGetCount++
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings })
    })
  })

  const sysInfoResponse = page.waitForResponse(response => response.url().includes('/sysinfo.json'))
  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()

  const hostname = page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input')
  await expect(hostname).toHaveValue('hb-rf-eth')
  await hostname.fill('edit-must-survive')
  await expect(page.locator('.floating-footer')).toBeVisible()

  releaseSysInfo()
  await sysInfoResponse
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
      body: JSON.stringify({ releases: [] })
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
