import globals from 'globals'
import pluginVue from 'eslint-plugin-vue'
import eslintConfigPrettier from '@vue/eslint-config-prettier'

export default [
  { files: ['**/*.{js,mjs,cjs,vue}'] },
  { languageOptions: { globals: globals.browser } },
  ...pluginVue.configs['flat/essential'],
  eslintConfigPrettier,
  {
    ignores: ['dist/**', '.parcel-cache/**', 'node_modules/**']
  },
  {
    rules: {
      'vue/multi-word-component-names': 'off'
    }
  }
]
