<template>
  <div class="layout-wrapper" :class="containerClass">
    <app-topbar></app-topbar>
    <div class="layout-sidebar">
      <app-menu></app-menu>
    </div>
    <div class="layout-main-container">
      <div class="layout-main">
        <router-view></router-view>
      </div>
      <app-footer></app-footer>
    </div>
    <div class="layout-mask"></div>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';
import AppTopbar from './AppTopbar.vue';
import AppMenu from './AppMenu.vue';
import AppFooter from './AppFooter.vue';
// import { useLayout } from '@/layout/composables/layout';

// Simple layout logic for now, mimicking standard PrimeVue admin template structure
const containerClass = computed(() => {
    return {
        'layout-theme-light': true,
        'layout-overlay': false,
        'layout-static': true,
        'layout-static-inactive': false,
        'layout-overlay-active': false,
        'layout-mobile-active': false,
        'p-input-filled': false,
        'p-ripple-disabled': false
    };
});
</script>

<style scoped>
/* Basic layout styles to get started if full scss is missing */
.layout-wrapper {
    display: flex;
    flex-direction: column;
    min-height: 100vh;
}
.layout-sidebar {
    position: fixed;
    width: 250px;
    height: 100vh;
    background: var(--p-surface-overlay);
    border-right: 1px solid var(--p-surface-border);
    top: 0;
    left: 0;
    z-index: 999;
    transition: transform 0.2s;
}
.layout-main-container {
    display: flex;
    flex-direction: column;
    min-height: 100vh;
    justify-content: space-between;
    padding: 7rem 2rem 2rem 2rem;
    margin-left: 250px;
    transition: margin-left 0.2s;
}

@media (max-width: 991px) {
    .layout-sidebar {
        transform: translateX(-100%);
    }
    .layout-main-container {
        margin-left: 0;
        padding-left: 2rem;
    }
}
</style>
