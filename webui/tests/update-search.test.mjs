import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { ref, computed } from 'vue'

// Evaluate the real composable with deterministic HTTP and timer boundaries.
// Each test receives fresh module state, without real devices or wall-clock waits.
const source = readFileSync(new URL('../src/composables/useUpdateSearch.js', import.meta.url), 'utf8')
  .replace(/^import .*$/gm, '')
  .replace('export function useUpdateSearch', 'function useUpdateSearch')

const fixture = () => {
  const timers = new Map()
  let sequence = 0
  let offline = true
  const axios = {
    get: async () => {
      if (offline) throw new Error('offline')
      return { data: { install: { state: 'downloading', target: 'firmware' } } }
    },
    post: async () => ({ data: { outcome: 'accepted' } })
  }
  const create = new Function('ref', 'computed', 'axios', 'safeLocal', 'setTimeout', 'clearTimeout',
    source + '\nreturn useUpdateSearch(key => key)')
  const api = create(ref, computed, axios, { get: () => null },
    callback => { const id = ++sequence; timers.set(id, callback); return id },
    id => timers.delete(id))
  api.updateStatus.value.install = { state: 'downloading', target: 'firmware' }
  return {
    api, timers,
    online: () => { offline = false },
    offline: () => { offline = true },
    tick: async () => {
      const [id, callback] = timers.entries().next().value
      timers.delete(id)
      await callback()
    }
  }
}

test('installation polling stops after 30 consecutive failures without enabling another install', async () => {
  const f = fixture()
  // Seed the poll through the accepted-install path.
  f.api.updateStatus.value.install.state = 'idle'
  await f.api.startInstall('firmware')
  for (let i = 0; i < 30; i++) await f.tick()
  assert.equal(f.timers.size, 0)
  assert.equal(f.api.installConnectionLost.value, true)
  assert.equal(f.api.installBusy.value, true)
  assert.equal(await f.api.startInstall('firmware'), 'busy')
})

test('a successful status read resets the consecutive failure streak', async () => {
  const f = fixture()
  f.api.updateStatus.value.install.state = 'idle'
  await f.api.startInstall('firmware')
  for (let i = 0; i < 29; i++) await f.tick()
  f.online()
  await f.tick()
  assert.equal(f.api.installConnectionLost.value, false)
  assert.equal(f.timers.size, 1)
  f.offline()
  for (let i = 0; i < 29; i++) await f.tick()
  assert.equal(f.api.installConnectionLost.value, false)
  assert.equal(f.timers.size, 1)
  f.api.stopPolling()
  assert.equal(f.timers.size, 0)
})

test('a recovered status read clears the lost-connection warning', async () => {
  const f = fixture()
  f.api.updateStatus.value.install.state = 'idle'
  await f.api.startInstall('firmware')
  for (let i = 0; i < 30; i++) await f.tick()
  f.online()
  await f.api.loadUpdateStatus()
  assert.equal(f.api.installConnectionLost.value, false)
})
