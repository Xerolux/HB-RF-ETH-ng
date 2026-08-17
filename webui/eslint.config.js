import js from '@eslint/js'
import pluginVue from 'eslint-plugin-vue'
import globals from 'globals'

// Flat ESLint config for the standalone New Design WebUI.
//
// The rule set is intentionally a correctness gate, not a style gate: it
// catches unused bindings, undefined globals and broken Vue templates, while
// leaving formatting alone. Formatting is not enforced here because the
// existing components were written before any linter existed and a stylistic
// sweep would bury real regressions in noise.
export default [
  {
    ignores: [
      'dist/**',
      'node_modules/**',
      'spiffs_image/**',
      'public/**',
      'test-results/**',
      'playwright-report/**',
    ],
  },
  js.configs.recommended,
  ...pluginVue.configs['flat/recommended'],
  {
    files: ['**/*.js', '**/*.vue'],
    languageOptions: {
      ecmaVersion: 2023,
      sourceType: 'module',
      globals: {
        ...globals.browser,
        ...globals.es2021,
        // Injected by Vite's `define` at build time (see vite.config.js).
        __WEBUI_VERSION__: 'readonly',
        __WEBUI_API_VERSION__: 'readonly',
      },
    },
    rules: {
      // Unused code is the single most common source of drift in this UI —
      // keep it an error, but allow the conventional leading-underscore
      // opt-out for deliberately ignored callback arguments.
      // `ignoreRestSiblings` keeps the established "destructure to omit"
      // idiom working, e.g. `const { secretSet, ...payload } = config` which
      // strips read-only sentinels the backend rejects on write.
      'no-unused-vars': ['error', {
        argsIgnorePattern: '^_',
        varsIgnorePattern: '^_',
        caughtErrorsIgnorePattern: '^_',
        ignoreRestSiblings: true,
      }],
      // The log viewer and the config sanitizers deliberately match ANSI
      // escapes and C0 control characters, which is exactly what this rule
      // flags. Matching them is the point.
      'no-control-regex': 'off',
      // The build strips these, but they should never reach a review.
      'no-debugger': 'error',
      'no-console': ['warn', { allow: ['warn', 'error'] }],

      // Vue rules that catch real breakage rather than taste.
      'vue/no-unused-components': 'error',
      'vue/require-v-for-key': 'error',
      'vue/no-mutating-props': 'error',

      // Multi-word component names and attribute ordering are conventions the
      // existing tree does not follow; enforcing them now would mean renaming
      // shipped components for no functional gain.
      'vue/multi-word-component-names': 'off',
      'vue/attributes-order': 'off',
      'vue/max-attributes-per-line': 'off',
      'vue/singleline-html-element-content-newline': 'off',
      'vue/html-self-closing': 'off',
      'vue/html-indent': 'off',
      'vue/html-closing-bracket-newline': 'off',
      'vue/first-attribute-linebreak': 'off',
    },
  },
  {
    // Node-side tooling and Playwright specs run outside the browser.
    files: ['*.config.js', 'tests/**/*.js'],
    languageOptions: {
      globals: {
        ...globals.node,
      },
    },
    rules: {
      'no-console': 'off',
    },
  },
]
