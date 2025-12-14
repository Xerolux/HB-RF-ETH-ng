import pluginVue from 'eslint-plugin-vue'
import skipFormatting from '@vue/eslint-config-prettier/skip-formatting'

export default [
  {
    name: 'app/files-to-lint',
    files: ['**/*.{js,mjs,jsx,vue}'],
  },
  {
    name: 'app/files-to-ignore',
    ignores: ['**/dist/**', '**/dist-ssr/**', '**/coverage/**', '**/.parcel-cache/**'],
  },
  ...pluginVue.configs['flat/essential'],
  skipFormatting,
  {
      rules: {
          'vue/multi-word-component-names': 'off',
          'no-console': process.env.NODE_ENV === 'production' ? 'warn' : 'off',
          'no-debugger': process.env.NODE_ENV === 'production' ? 'warn' : 'off'
      }
  }
]
