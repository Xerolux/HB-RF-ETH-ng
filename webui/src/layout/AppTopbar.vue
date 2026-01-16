<template>
    <div class="layout-topbar">
        <router-link to="/" class="layout-topbar-logo">
            <span class="text-xl font-bold">HB-RF-ETH-ng</span>
        </router-link>

        <button class="p-link layout-menu-button layout-topbar-button" @click="onMenuToggle">
            <i class="pi pi-bars"></i>
        </button>

        <button class="p-link layout-topbar-menu-button layout-topbar-button" @click="onTopBarMenuButton">
            <i class="pi pi-ellipsis-v"></i>
        </button>

        <div class="layout-topbar-menu" :class="topbarMenuClasses">
            <button @click="onThemeToggle" class="p-link layout-topbar-button">
                <i :class="['pi', themeStore.isDark ? 'pi-moon' : 'pi-sun']"></i>
                <span>Theme</span>
            </button>
            <button @click="onLogout" class="p-link layout-topbar-button">
                <i class="pi pi-sign-out"></i>
                <span>Logout</span>
            </button>
        </div>
    </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useLoginStore, useThemeStore } from '@/stores';
import { useRouter } from 'vue-router';

const loginStore = useLoginStore();
const themeStore = useThemeStore();
const router = useRouter();

const onMenuToggle = () => {
    // Implement sidebar toggle logic
};

const onTopBarMenuButton = () => {
    // Mobile menu toggle
};

const onThemeToggle = () => {
    themeStore.toggleTheme();
};

const onLogout = () => {
    loginStore.logout();
    router.push('/login');
};

const topbarMenuClasses = computed(() => {
    return {
        'layout-topbar-menu-mobile-active': false
    };
});
</script>

<style scoped>
.layout-topbar {
    position: fixed;
    height: 5rem;
    z-index: 997;
    left: 0;
    top: 0;
    width: 100%;
    padding: 0 2rem;
    background-color: var(--p-surface-card);
    transition: left 0.2s;
    display: flex;
    align-items: center;
    box-shadow: 0px 3px 5px rgba(0,0,0,.02), 0px 0px 2px rgba(0,0,0,.05), 0px 1px 4px rgba(0,0,0,.08);
}

.layout-topbar-logo {
    display: flex;
    align-items: center;
    color: var(--p-text-color);
    font-size: 1.5rem;
    font-weight: 500;
    width: 250px;
    border-radius: 12px;
}

.layout-topbar-button {
    display: inline-flex;
    justify-content: center;
    align-items: center;
    position: relative;
    color: var(--p-text-color-secondary);
    border-radius: 50%;
    width: 3rem;
    height: 3rem;
    cursor: pointer;
    transition: background-color 0.2s;
}

.layout-topbar-button:hover {
    color: var(--p-text-color);
    background-color: var(--p-surface-hover);
}

.layout-topbar-menu {
    margin: 0 0 0 auto;
    padding: 0;
    list-style: none;
    display: flex;
}

.layout-topbar-menu .layout-topbar-button {
    margin-left: 1rem;
}
</style>
