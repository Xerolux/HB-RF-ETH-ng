## 2025-02-18 - [Internationalized ARIA Labels]
**Learning:** Hardcoded English ARIA labels in components (like 'Show password') bypass the i18n system, creating an accessibility gap for non-English users even when the rest of the UI is fully translated.
**Action:** Always check ARIA labels in reusable components and ensure they use the `t()` function with appropriate keys, adding new locale keys if necessary.

## 2025-02-18 - [Consistent Input Hints]
**Learning:** Inconsistent use of placeholders between similar feature sets (like IPv4 vs IPv6 settings) increases cognitive load. If one section guides the user with format examples, the other should too.
**Action:** When auditing forms, check for symmetry in helper text and placeholders across related groups of inputs.
