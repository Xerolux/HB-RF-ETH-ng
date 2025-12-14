import pluginVue from "eslint-plugin-vue";
import vueConfigPrettier from "@vue/eslint-config-prettier";

export default [
  ...pluginVue.configs["flat/essential"],
  vueConfigPrettier,
  {
    rules: {
      "vue/multi-word-component-names": "off",
      "no-console": "warn",
      "no-debugger": "warn",
    },
  },
];
