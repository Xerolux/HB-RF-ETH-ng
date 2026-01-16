<template>
    <div class="flex align-items-center justify-content-center min-h-screen bg-surface-ground">
        <div class="surface-card p-4 shadow-2 border-round w-full lg:w-4">
            <div class="text-center mb-5">
                <div class="text-900 text-3xl font-medium mb-3">HB-RF-ETH-ng</div>
                <span class="text-600 font-medium line-height-3">{{ t('login.title') }}</span>
            </div>

            <form @submit.prevent="handleLogin">
                <label for="password" class="block text-900 font-medium mb-2">{{ t('login.password') }}</label>
                <Password
                    id="password"
                    v-model="password"
                    :feedback="false"
                    toggleMask
                    class="w-full mb-3"
                    inputClass="w-full"
                    :placeholder="t('login.password')"
                ></Password>

                <Button
                    :label="loading ? t('common.loading') : t('login.submit')"
                    icon="pi pi-user"
                    class="w-full"
                    type="submit"
                    :loading="loading"
                ></Button>
            </form>

            <Message v-if="error" severity="error" class="mt-3" :closable="false">{{ error }}</Message>
        </div>
    </div>
</template>

<script setup>
import { ref } from 'vue';
import { useRouter, useRoute } from 'vue-router';
import { useLoginStore } from '@/stores';
import { useI18n } from 'vue-i18n';
import axios from 'axios';
import Password from 'primevue/password';
import Button from 'primevue/button';
import Message from 'primevue/message';

const { t } = useI18n();
const router = useRouter();
const route = useRoute();
const loginStore = useLoginStore();

const password = ref('');
const loading = ref(false);
const error = ref('');

const handleLogin = async () => {
    loading.value = true;
    error.value = '';

    try {
        const response = await axios.post('/login.json', { password: password.value });
        if (response.data.isAuthenticated) {
            loginStore.setToken(response.data.token);
            const redirect = route.query.redirect || '/';
            router.push(redirect);
        } else {
            error.value = t('login.failed');
        }
    } catch (err) {
        error.value = t('common.error') + ': ' + (err.response?.data?.error || err.message);
    } finally {
        loading.value = false;
    }
};
</script>
