## 2025-02-18 - [Internationalized ARIA Labels]
**Learning:** Hardcoded English ARIA labels in components (like 'Show password') bypass the i18n system, creating an accessibility gap for non-English users even when the rest of the UI is fully translated.
**Action:** Always check ARIA labels in reusable components and ensure they use the `t()` function with appropriate keys, adding new locale keys if necessary.
