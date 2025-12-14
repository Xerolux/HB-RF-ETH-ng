<template>
  <BCard
    :header="t('login.title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BFormGroup :label="t('login.password')" label-cols-sm="4">
        <BFormInput
          type="password"
          v-model="password"
          :state="v$.password.$error ? false : null"
        />
      </BFormGroup>
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
        >{{ t("login.loginError") }}</BAlert
      >
      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          @click="loginClick"
          :disabled="!password || password === ''"
          >{{ t("login.login") }}</BButton
        >
      </BFormGroup>
    </BForm>
  </BCard>
</template>

<script setup>
import { ref } from "vue";
import { useRouter, useRoute } from "vue-router";
import { useI18n } from "vue-i18n";
import { useVuelidate } from "@vuelidate/core";
import { required } from "@vuelidate/validators";
import { useLoginStore } from "./stores.js";

const { t } = useI18n();

const router = useRouter();
const route = useRoute();
const loginStore = useLoginStore();

const password = ref("");
const showError = ref(null);

const rules = {
  password: { required },
};

const v$ = useVuelidate(rules, { password });

const loginClick = async () => {
  showError.value = null;
  const success = await loginStore.tryLogin(password.value);
  if (success) {
    router.push(route.query.redirect || "/");
  } else {
    showError.value = 10;
  }
};
</script>

<style scoped></style>
