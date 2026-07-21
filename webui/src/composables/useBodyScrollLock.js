/**
 * Body scroll lock that actually works on iOS Safari.
 *
 * iOS Safari ignores `overflow: hidden` on <body> for touch scrolling, so the
 * naive approach (`document.body.style.overflow = 'hidden'`) leaves the page
 * scrolling behind an open mobile menu / modal ("scroll-through"). The reliable
 * trick is to fix the body in place with `position: fixed` and a negative top
 * offset equal to the current scroll position, then restore both on unlock.
 *
 * `lock()` / `unlock()` are idempotent and safe to call without a matching
 * counterpart. A depth counter guards nested callers (menu open while a modal
 * is already locking). The module keeps a single record of the saved scroll
 * position so unlock fully restores the page.
 *
 * This is the SINGLE source of truth for body scroll locking in the app. Both
 * the mobile menu / restart modal (via setBodyScrollLock) AND BootstrapLite's
 * BModal (via lockBodyScroll / unlockBodyScroll) go through the same depth
 * counter, so a modal opened on top of a menu (or vice versa) unlocks cleanly
 * when the inner one closes — previously BModal kept its own separate
 * `overflow:hidden` state and the two systems could leave the page stuck.
 */

let lockDepth = 0
let savedScrollY = 0
let savedBodyStyle = ''
let savedBodyTop = ''

export function lockBodyScroll() {
  lockDepth++
  if (lockDepth === 1) {
    savedScrollY = window.scrollY || window.pageYOffset || 0
    savedBodyStyle = document.body.style.cssText || ''
    savedBodyTop = document.body.style.top || ''
    const width = document.body.offsetWidth
    // `position: fixed` removes the body from flow, which makes the page width
    // collapse when a scrollbar was visible. Reserve the width to avoid layout
    // shift / reflow.
    document.body.style.cssText = savedBodyStyle
    document.body.style.position = 'fixed'
    document.body.style.top = `-${savedScrollY}px`
    document.body.style.left = '0'
    document.body.style.right = '0'
    document.body.style.width = `${width}px`
    document.body.style.overflow = 'hidden'
  }
}

export function unlockBodyScroll() {
  if (lockDepth === 0) return
  lockDepth--
  if (lockDepth === 0) {
    // Restore the original inline style wholesale, then jump back to the
    // recorded scroll position.
    document.body.style.cssText = savedBodyStyle
    if (savedBodyTop) document.body.style.top = savedBodyTop
    window.scrollTo(0, savedScrollY)
    savedScrollY = 0
    savedBodyStyle = ''
    savedBodyTop = ''
  }
}

/** Current lock depth — useful for diagnostics and for callers that want to
 *  check whether they are nested inside another lock. */
export function bodyScrollLockDepth() {
  return lockDepth
}

// Convenience: bind to a ref<boolean>. Returns a watcher-friendly pair.
export function setBodyScrollLock(locked) {
  if (locked) lockBodyScroll()
  else unlockBodyScroll()
}
