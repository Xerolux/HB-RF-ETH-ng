/**
 * Dismiss a UI element (dropdown, popover) on outside-click or Escape.
 *
 * Used for the locale picker in both headers. Desktop menus get closed by
 * clicking the toggle again, but on a phone the toggle sits over other controls
 * and there is no natural "click elsewhere to dismiss" — this composable adds
 * that affordance plus keyboard (Escape) support.
 *
 * Usage:
 *   const pickerRef = ref(null)
 *   const { enable, disable } = useDismissable(pickerRef, () => (open.value = false))
 *   watch(open, o => o ? enable() : disable())
 *   onUnmounted(disable)
 *
 * The ref must wrap the toggle + its dropdown so clicks inside don't dismiss.
 */
import { onUnmounted } from 'vue'

export function useDismissable(targetRef, dismiss) {
  let active = false

  const onDocumentClick = (e) => {
    if (!active) return
    const el = targetRef.value
    if (el && (el === e.target || el.contains(e.target))) return
    dismiss()
  }
  const onKeydown = (e) => {
    if (!active) return
    if (e.key === 'Escape') {
      e.preventDefault()
      dismiss()
    }
  }

  const enable = () => {
    if (active) return
    active = true
    // Defer attaching to the next tick so the opening click itself doesn't
    // immediately close the element it just opened.
    setTimeout(() => {
      if (!active) return
      document.addEventListener('click', onDocumentClick, true)
      document.addEventListener('touchstart', onDocumentClick, true)
      document.addEventListener('keydown', onKeydown, true)
    }, 0)
  }
  const disable = () => {
    if (!active) return
    active = false
    document.removeEventListener('click', onDocumentClick, true)
    document.removeEventListener('touchstart', onDocumentClick, true)
    document.removeEventListener('keydown', onKeydown, true)
  }

  onUnmounted(disable)

  return { enable, disable }
}
