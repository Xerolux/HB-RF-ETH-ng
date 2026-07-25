/**
 * Safe wrappers around localStorage / sessionStorage.
 *
 * iOS Safari in Private Browsing mode throws QuotaExceededError / SecurityError
 * on *every* write (setItem), and some embedded WebView contexts throw on writes
 * too. Left unguarded these exceptions propagate up through Vue actions and
 * break the UI — most notably the login flow, where a throw inside `login()`
 * rolls back an already-accepted server login (see stores.js).
 *
 * Every persistent write in the app should go through safeSetItem / safeRemoveItem
 * so a non-persistable storage never takes down functionality. Reads are safe
 * (they return null on failure), but wrapped here for symmetry and to centralise
 * the access path.
 */

export function safeSetItem(storage, key, value) {
  try {
    storage.setItem(key, value)
    return true
  } catch (e) {
    // QuotaExceededError (Safari private mode), SecurityError (sandboxed),
    // or a genuine quota exhaustion — none of them should abort the caller.
    return false
  }
}

export function safeRemoveItem(storage, key) {
  try {
    storage.removeItem(key)
    return true
  } catch (e) {
    return false
  }
}

export function safeGetItem(storage, key) {
  try {
    return storage.getItem(key)
  } catch (e) {
    return null
  }
}

export function safeClear(storage) {
  try {
    storage.clear()
    return true
  } catch (e) {
    return false
  }
}

// Convenience bound helpers so call sites read naturally:
//   safeLocal.set('theme', 'dark')   /  safeLocal.remove('theme')
export const safeLocal = {
  get: (key) => safeGetItem(localStorage, key),
  set: (key, value) => safeSetItem(localStorage, key, value),
  remove: (key) => safeRemoveItem(localStorage, key),
  clear: () => safeClear(localStorage)
}

export const safeSession = {
  get: (key) => safeGetItem(sessionStorage, key),
  set: (key, value) => safeSetItem(sessionStorage, key, value),
  remove: (key) => safeRemoveItem(sessionStorage, key),
  clear: () => safeClear(sessionStorage)
}
