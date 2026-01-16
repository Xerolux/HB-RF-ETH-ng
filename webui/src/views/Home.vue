<template>
    <div class="grid">
        <div class="col-12">
            <div class="card">
                <h5>System Information</h5>
                <div v-if="loading" class="flex justify-content-center">
                    <ProgressSpinner />
                </div>
                <div v-else class="grid">
                    <div class="col-12 md:col-6 lg:col-3">
                        <div class="surface-card shadow-2 p-3 border-round">
                            <div class="flex justify-content-between mb-3">
                                <div>
                                    <span class="block text-500 font-medium mb-3">Serial</span>
                                    <div class="text-900 font-medium text-xl">{{ sysInfo.serial }}</div>
                                </div>
                                <div class="flex align-items-center justify-content-center bg-blue-100 border-round" style="width:2.5rem;height:2.5rem">
                                    <i class="pi pi-id-card text-blue-500 text-xl"></i>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="col-12 md:col-6 lg:col-3">
                        <div class="surface-card shadow-2 p-3 border-round">
                            <div class="flex justify-content-between mb-3">
                                <div>
                                    <span class="block text-500 font-medium mb-3">Version</span>
                                    <div class="text-900 font-medium text-xl">{{ sysInfo.currentVersion }}</div>
                                </div>
                                <div class="flex align-items-center justify-content-center bg-orange-100 border-round" style="width:2.5rem;height:2.5rem">
                                    <i class="pi pi-tag text-orange-500 text-xl"></i>
                                </div>
                            </div>
                        </div>
                    </div>
                    <!-- Add more cards as needed -->
                </div>

                <!-- Full JSON Dump for now to ensure data availability -->
                <div class="mt-4">
                    <pre>{{ sysInfo }}</pre>
                </div>
            </div>
        </div>
    </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue';
import axios from 'axios';
import ProgressSpinner from 'primevue/progressspinner';

const sysInfo = ref({});
const loading = ref(true);
let pollInterval = null;

const fetchSysInfo = async () => {
    try {
        const response = await axios.get('/sysinfo.json');
        if (response.data && response.data.sysInfo) {
            sysInfo.value = response.data.sysInfo;
        }
    } catch (e) {
        console.error(e);
    } finally {
        loading.value = false;
    }
};

onMounted(() => {
    fetchSysInfo();
    pollInterval = setInterval(fetchSysInfo, 1000);
});

onUnmounted(() => {
    if (pollInterval) clearInterval(pollInterval);
});
</script>
