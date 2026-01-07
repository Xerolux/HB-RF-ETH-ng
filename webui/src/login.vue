<template>
  <BCard
    :header="t('login.title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent="loginClick">
      <BFormGroup :label="t('login.password')" label-cols-sm="4">
        <PasswordInput
          v-model="password"
          :state="v$.password.$error ? false : null"
          :autofocus="true"
          autocomplete="current-password"
        />
      </BFormGroup>
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
      >{{ t('login.loginError') }}</BAlert>
      <BFormGroup label-cols-sm="9">
        <BButton
          type="submit"
          variant="primary"
          block
          :disabled="!password || password === '' || loading"
        >
          <BSpinner small v-if="loading" class="me-2" />
          {{ t('login.login') }}
        </BButton>
      </BFormGroup>
    </BForm>
  </BCard>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required } from '@vuelidate/validators'
import { useLoginStore } from './stores.js'
import PasswordInput from './components/PasswordInput.vue'

const { t } = useI18n()

const router = useRouter()
const route = useRoute()
const loginStore = useLoginStore()

const password = ref('')
const showError = ref(null)
const loading = ref(false)

const rules = {
  password: { required }
}

const v$ = useVuelidate(rules, { password })

const loginClick = async () => {
  loading.value = true
  showError.value = null
  try {
    const success = await loginStore.tryLogin(password.value)
    if (success) {
      router.push(route.query.redirect || '/')
    } else {
      // Login failed - incorrect password
      showError.value = true
      loading.value = false
    }
  } catch (error) {
    // Network or server error
    showError.value = true
    loading.value = false
  }
}
</script>

<style scoped>
</style>
