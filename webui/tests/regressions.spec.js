import { test, expect } from '@playwright/test'

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
    localStorage.setItem('locale', 'en')
    sessionStorage.setItem('hb-rf-eth-ng-pw', 'device-secret-token')
    sessionStorage.setItem('hb-rf-eth-ng-last-activity', String(Date.now()))
  })

  await page.route('**/sysinfo.json', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ sysInfo: { currentVersion: '2.2.4-Beta.3' } })
  }))

  await page.route('**/api/check_update**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ latestVersion: '2.2.4-Beta.3', updateAvailable: false })
  }))
})

test('settings save/discard state follows the edited form immediately', async ({ page }) => {
  const persisted = { ...settings }

  await page.route('**/settings.json', async route => {
    if (route.request().method() === 'POST') {
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
  await page.getByRole('button', { name: 'Network' }).click()

  const hostname = page.locator('.form-group', { hasText: 'Hostname' }).locator('input')
  const footer = page.locator('.floating-footer')

  await expect(hostname).toHaveValue('hb-rf-eth')
  await expect(footer).toBeHidden()

  await hostname.fill('changed-host')
  await expect(footer).toBeVisible()

  page.once('dialog', dialog => dialog.accept())
  await footer.locator('.discard-btn').click()
  await expect(hostname).toHaveValue('hb-rf-eth')
  await expect(footer).toBeHidden()

  await hostname.fill('saved-host')
  await expect(footer).toBeVisible()
  await footer.locator('.save-btn').click()
  await expect(footer).toBeHidden()
  expect(persisted.hostname).toBe('saved-host')
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

    await page.route('**/settings.json', async route => {
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
    await expect(page.locator('.floating-footer')).toBeHidden()
  })
}

test('invalid settings reveal the affected tab instead of silently ignoring save', async ({ page }) => {
  let postCount = 0
  await page.route('**/settings.json', async route => {
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
  await page.locator('.form-group', { hasText: 'Hostname' }).locator('input').fill('invalid_hostname')
  await page.getByRole('button', { name: 'General' }).click()
  await page.locator('.form-group', { hasText: 'CCU IP address' }).locator('input').fill('192.168.1.11')
  await page.locator('.floating-footer .save-btn').click()

  await expect(page.getByRole('button', { name: 'Network' })).toHaveClass(/active/)
  await expect(page.locator('.floating-footer .footer-alert')).toContainText('Please correct the highlighted fields.')
  expect(postCount).toBe(0)
})

test('firmware archive loads only from the manifest embedded in the device', async ({ page }) => {
  await page.route('**/settings.json', route => route.fulfill({
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

  await page.goto(`${BASE_URL}/firmware`)
  await expect.poll(() => localRequests).toBe(1)
  await page.waitForTimeout(250)
  expect(externalRequests).toBe(0)
  await expect(page.locator('.archive-refresh')).toHaveCount(0)
  await expect(page.locator('.archive-error')).toBeHidden()
})
