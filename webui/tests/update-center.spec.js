import { test, expect } from '@playwright/test'

const BASE = 'http://127.0.0.1:1234'
const status = (overrides = {}) => ({
  state: 'idle', everChecked: true, channel: 'stable', runningFirmware: '2.3.1', runningWebui: '1.1.1',
  latestFirmware: '2.3.2', latestWebui: '1.1.2', firmwareUpdateAvailable: true, webuiUpdateAvailable: true,
  lastCheck: 1794400000, install: { state: 'idle', target: '', progress: 0 }, ...overrides
})
const respond = data => route => route.fulfill({ contentType: 'application/json', body: JSON.stringify(data) })
test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => {
    localStorage.setItem('locale', 'en')
    sessionStorage.setItem('hb-rf-eth-ng-pw', 'test-token')
    sessionStorage.setItem('hb-rf-eth-ng-last-activity', String(Date.now()))
  })
  await page.route('**/api/**', respond({}))
  await page.route('**/settings.json**', respond({ settings: { hostname: 'HB-RF-ETH-Test', flashPause: false } }))
  await page.route('**/sysinfo.json**', respond({ sysInfo: { hostname: 'HB-RF-ETH-Test', currentVersion: '2.3.1', cpuUsage: 10, memoryUsage: 50, ethernetConnected: true } }))
  await page.route('**/api/update/config**', respond({ mode: 'off', channel: 'stable' }))
  await page.route('**/api/update/status**', respond(status()))
  await page.route('**/api/webui/status**', respond({ effectiveVersion: '1.1.1', valid: true }))
})

for (const target of ['firmware', 'webui']) {
  test(`${target}: version comparison and on-demand escaped notes`, async ({ page }) => {
    let noteRequests = 0
    let installRequests = 0
    await page.route('**/updates/notes/*.md', route => {
      noteRequests++
      return route.fulfill({ body: '# Improvements\n- Clearer status\n<script>window.compromised=true</script>' })
    })
    await page.route('**/api/update/install', route => { installRequests++; return respond({ outcome: 'accepted' })(route) })
    await page.goto(`${BASE}/updates/${target}`)
    const overview = page.getByRole('region', { name: 'Version comparison' })
    await expect(overview).toContainText(target === 'firmware' ? '2.3.1' : '1.1.1')
    await expect(overview).toContainText(target === 'firmware' ? '2.3.2' : '1.1.2')
    expect(noteRequests).toBe(0)
    await page.getByRole('button', { name: 'Install now', exact: true }).click()
    await expect(page.getByRole('dialog').getByRole('heading', { name: 'Improvements', exact: true })).toBeVisible()
    await expect(page.locator('.release-notes')).toContainText('<script>')
    expect(await page.evaluate(() => window.compromised)).toBeUndefined()
    await page.getByRole('dialog').getByRole('button', { name: 'Cancel', exact: true }).click()
    await expect(page.locator('.release-notes')).toHaveCount(0)
    expect(noteRequests).toBe(1)
    expect(installRequests).toBe(0)
  })
}

test('status load error is distinct from no search and retry recovers', async ({ page }) => {
  await page.route('**/api/update/status**', route => route.fulfill({ status: 503, body: '' }))
  await page.goto(`${BASE}/updates/firmware`)
  const overview = page.getByRole('region', { name: 'Version comparison' })
  await expect(overview).toContainText('Could not load status')
  await expect(overview).not.toContainText('Up to date')
  await page.route('**/api/update/status**', respond(status({ everChecked: false })))
  await overview.getByRole('button', { name: 'Retry' }).click()
  await expect(overview).toContainText('Search for updates')
  await expect(overview).not.toContainText('2.3.2')
})

test('channel change hides an old result until a matching search succeeds', async ({ page }) => {
  await page.goto(`${BASE}/updates/firmware`)
  const overview = page.getByRole('region', { name: 'Version comparison' })
  await expect(overview).toContainText('2.3.2')
  await page.locator('#update-channel').selectOption('beta')
  await expect(overview).toContainText('Search for updates')
  await expect(overview).not.toContainText('2.3.2')
  await expect(page.getByRole('button', { name: 'Install now', exact: true })).toBeDisabled()
})

test('active installation shows real progress and stops polling after navigation', async ({ page }) => {
  let reads = 0
  await page.route('**/api/update/status**', route => {
    reads++
    return respond(status({ install: { target: 'webui', state: 'verifying', progress: 97 } }))(route)
  })
  await page.setViewportSize({ width: 390, height: 844 })
  await page.emulateMedia({ reducedMotion: 'reduce' })
  await page.goto(`${BASE}/updates/webui`)
  await expect(page.getByRole('progressbar')).toHaveAttribute('aria-valuenow', '97')
  await expect(page.locator('.install-steps [aria-current="step"]')).toContainText('Verify')
  await expect(page.locator('.ota-chip')).toHaveAttribute('href', '/updates/webui')
  await expect(page.locator('.ota-chip')).toContainText('OTA verifying')
  await expect.poll(() => reads).toBeGreaterThan(1)
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true)
  await page.screenshot({ path: 'test-results/update-center-mobile.png', fullPage: true })
  await page.goto(`${BASE}/about`)
  await page.waitForTimeout(1200)
  const stoppedAt = reads
  await page.waitForTimeout(1200)
  expect(reads).toBe(stoppedAt)
})

test('desktop dark comparison fits and successful search is shown clearly', async ({ page }) => {
  await page.addInitScript(() => localStorage.setItem('theme', 'dark'))
  await page.setViewportSize({ width: 1440, height: 1000 })
  await page.route('**/api/update/status**', respond(status({ firmwareUpdateAvailable: false, latestFirmware: '2.3.1' })))
  await page.goto(`${BASE}/updates/firmware`)
  await expect(page.getByRole('region', { name: 'Version comparison' })).toContainText('Up to date at the last successful check')
  await page.screenshot({ path: 'test-results/update-center-dark.png', fullPage: true })
})
