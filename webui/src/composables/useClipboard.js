/**
 * Cross-browser clipboard copy, iOS Safari included.
 *
 * Why this exists: the dashboard "copy value" buttons (hostname, serial, IPs,
 * MACs, …) used a hidden <textarea> + execCommand('copy') without focusing the
 * element. iOS Safari only honours execCommand('copy') when the target is
 * focused and editable, so nothing was copied on iPhone. The systemlog page
 * already had a working implementation built around a contentEditable span and
 * a document-level Selection; that approach is extracted here so both call
 * sites share one correct, iOS-safe code path.
 *
 * Strategy:
 *  1. On a secure context (HTTPS / localhost) prefer the async Clipboard API.
 *  2. Otherwise (the common case — the device WebUI is served over plain HTTP)
 *     use a throwaway contentEditable span + a document-level Selection and
 *     execCommand('copy'). This works on iOS Safari, does not fight modal
 *     focus-traps (the selection lives at document level), and preserves
 *     newlines.
 *
 * Returns true on success, false on failure. Callers own the i18n + toast.
 */
export async function copyToClipboard(text) {
  if (typeof text !== 'string') text = String(text ?? '')

  // 1. Async Clipboard API (secure contexts only).
  if (navigator.clipboard && window.isSecureContext) {
    try {
      await navigator.clipboard.writeText(text)
      return true
    } catch {
      // Permission denied, clipboard busy, or transient failure — fall through.
    }
  }

  // 2. Synchronous execCommand fallback for HTTP contexts and older browsers.
  const span = document.createElement('span')
  span.textContent = text
  span.style.position = 'fixed'
  span.style.top = '0'
  span.style.left = '0'
  span.style.opacity = '0'
  span.style.pointerEvents = 'none'
  span.style.whiteSpace = 'pre'   // preserve newlines in multi-line copies
  span.style.userSelect = 'text'  // override any global `user-select: none`
  span.setAttribute('aria-hidden', 'true')
  // contentEditable makes the selection copyable on iOS Safari as well.
  span.contentEditable = 'true'
  document.body.appendChild(span)

  const selection = window.getSelection()
  const previousRange = selection && selection.rangeCount > 0 ? selection.getRangeAt(0) : null
  let ok = false
  try {
    const range = document.createRange()
    range.selectNodeContents(span)
    selection.removeAllRanges()
    selection.addRange(range)
    ok = document.execCommand('copy')
  } catch {
    // `ok` is still false here: nothing runs after the execCommand result is
    // assigned, so a throw can only have happened while it was still false.
    // The finally block below still restores the selection and removes the
    // scratch node.
  } finally {
    if (selection) {
      selection.removeAllRanges()
      if (previousRange) selection.addRange(previousRange)
    }
    document.body.removeChild(span)
  }

  if (!ok) throw new Error('clipboard copy failed')
  return true
}
