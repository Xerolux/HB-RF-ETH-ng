<template>
    <div class="card">
        <h5>{{ t('settings.title') || 'Settings' }}</h5>

        <form @submit.prevent="saveSettings">
            <div class="p-fluid formgrid grid">
                <div class="field col-12 md:col-6">
                    <label for="hostname">{{ t('settings.hostname') || 'Hostname' }}</label>
                    <InputText id="hostname" v-model="settings.hostname" />
                </div>

                <div class="field col-12 md:col-6">
                    <label>{{ t('settings.dhcp') || 'DHCP' }}</label>
                    <div class="flex gap-3">
                         <div class="flex align-items-center">
                            <RadioButton v-model="settings.useDHCP" :value="true" inputId="dhcp_on" />
                            <label for="dhcp_on" class="ml-2">{{ t('common.enabled') }}</label>
                        </div>
                        <div class="flex align-items-center">
                            <RadioButton v-model="settings.useDHCP" :value="false" inputId="dhcp_off" />
                            <label for="dhcp_off" class="ml-2">{{ t('common.disabled') }}</label>
                        </div>
                    </div>
                </div>

                <template v-if="!settings.useDHCP">
                    <div class="field col-12 md:col-6">
                        <label for="localIP">IP Address</label>
                        <InputText id="localIP" v-model="settings.localIP" />
                    </div>
                    <!-- Add Netmask, Gateway, DNS etc -->
                </template>
            </div>

            <div class="flex justify-content-end mt-3">
                <Button :label="t('common.save')" type="submit" :loading="saving" />
            </div>
        </form>
    </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import axios from 'axios';
import { useI18n } from 'vue-i18n';
import { useToast } from 'primevue/usetoast';
import InputText from 'primevue/inputtext';
import Button from 'primevue/button';
import RadioButton from 'primevue/radiobutton';

const { t } = useI18n();
const toast = useToast();

const settings = ref({
    hostname: '',
    useDHCP: true,
    localIP: ''
});
const loading = ref(true);
const saving = ref(false);

const loadSettings = async () => {
    loading.value = true;
    try {
        const response = await axios.get('/settings.json');
        if (response.data && response.data.settings) {
            settings.value = response.data.settings;
        }
    } catch (e) {
        toast.add({ severity: 'error', summary: 'Error', detail: 'Failed to load settings', life: 3000 });
    } finally {
        loading.value = false;
    }
};

const saveSettings = async () => {
    saving.value = true;
    try {
        await axios.post('/settings.json', settings.value);
        toast.add({ severity: 'success', summary: 'Saved', detail: 'Settings saved successfully', life: 3000 });
    } catch (e) {
        toast.add({ severity: 'error', summary: 'Error', detail: 'Failed to save settings', life: 3000 });
    } finally {
        saving.value = false;
    }
};

onMounted(loadSettings);
</script>
