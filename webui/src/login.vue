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
        <BInputGroup>
          <BFormInput
            :type="passwordVisible ? 'text' : 'password'"
            v-model="password"
            :state="v$.password.$error ? false : null"
            autofocus
            placeholder="••••••"
          />
          <BInputGroupAppend>
            <BButton
              @click="passwordVisible = !passwordVisible"
              variant="outline-secondary"
              :aria-label="passwordVisible ? 'Hide password' : 'Show password'"
            >
              <svg v-if="passwordVisible" xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye-slash" viewBox="0 0 16 16">
                <path d="M13.359 11.238C15.06 9.72 16 8 16 8s-3-5.5-8-5.5a7.028 7.028 0 0 0-2.79.588l.77.771A5.944 5.944 0 0 1 8 3.5c2.12 0 3.879 1.168 5.168 2.457A13.134 13.134 0 0 1 14.828 8c-.058.087-.122.183-.195.288-.335.48-.83 1.12-1.465 1.755-.165.165-.337.328-.517.486z"/>
                <path d="M11.297 5.316 5.066 11.547c-.862-.2-1.635-.726-2.195-1.474-.294-.396-.569-.83-.798-1.258a13.3 13.3 0 0 1-.954-3.14l.872-.486c.205.908.537 1.728.954 2.196.48.538 1.106.945 1.764 1.115l.623-.623A4.5 4.5 0 0 1 4.5 8a4.502 4.502 0 0 1 4.093-6.248l.965.965a5.503 5.503 0 0 0-.66.068c-1.996-.282-3.87 1.053-4.186 3.018l.865.865A3.49 3.49 0 0 0 11 8c0 .245-.045.478-.127.697l2.128 2.128a11.59 11.59 0 0 0 1.25-1.583l-.872-.486c-.238.41-.502.795-.788 1.139l-1.294-1.294z"/>
                <path d="M0 1.354 1.354 0l14 14-1.354 1.354-14-14zM8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"/>
              </svg>
              <svg v-else xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye" viewBox="0 0 16 16">
                <path d="M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8zM1.173 8a13.133 13.133 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5c2.12 0 3.879 1.168 5.168 2.457A13.133 13.133 0 0 1 14.828 8c-.314.435-.8.995-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5c-2.12 0-3.879-1.168-5.168-2.457A13.134 13.134 0 0 1 1.172 8z"/>
                <path d="M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0z"/>
              </svg>
            </BButton>
          </BInputGroupAppend>
        </BInputGroup>
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

const { t } = useI18n()

const router = useRouter()
const route = useRoute()
const loginStore = useLoginStore()

const password = ref('')
const passwordVisible = ref(false)
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
