## 2025-02-18 - [Internationalized ARIA Labels]
**Learning:** Hardcoded English ARIA labels in components (like 'Show password') bypass the i18n system, creating an accessibility gap for non-English users even when the rest of the UI is fully translated.
**Action:** Always check ARIA labels in reusable components and ensure they use the `t()` function with appropriate keys, adding new locale keys if necessary.

## 2025-02-19 - [Password Autocomplete]
**Learning:** Generic password fields without explicit `autocomplete` attributes force browsers to guess, often leading to annoying "Save Password" prompts on "New Password" fields or failure to autofill "Current Password".
**Action:** Explicitly set `autocomplete="current-password"` for login forms and `autocomplete="new-password"` for change/reset forms to guide browser behavior and password managers correctly.
