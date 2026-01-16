<template>
    <ul class="layout-menu">
        <template v-for="(item, i) in model" :key="item.label">
             <li class="layout-menuitem-category" v-if="item.items">
                {{ item.label }}
             </li>
             <li v-for="(child, i) in item.items || [item]" :key="child.label" class="layout-menuitem">
                <router-link :to="child.to" class="layout-menuitem-link" active-class="active-route">
                    <i :class="child.icon" class="layout-menuitem-icon"></i>
                    <span class="layout-menuitem-text">{{ child.label }}</span>
                </router-link>
             </li>
        </template>
    </ul>
</template>

<script setup>
import { ref } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();

const model = ref([
    {
        label: 'Home',
        items: [
            { label: t('nav.home'), icon: 'pi pi-fw pi-home', to: '/' },
            { label: t('nav.settings'), icon: 'pi pi-fw pi-cog', to: '/settings' },
            { label: t('nav.firmware'), icon: 'pi pi-fw pi-refresh', to: '/firmware' },
            { label: t('nav.monitoring'), icon: 'pi pi-fw pi-chart-bar', to: '/monitoring' },
            { label: t('nav.analyzer'), icon: 'pi pi-fw pi-wifi', to: '/analyzer' },
            { label: t('nav.log'), icon: 'pi pi-fw pi-list', to: '/log' },
        ]
    }
]);
</script>

<style scoped>
.layout-menu {
    list-style-type: none;
    margin: 0;
    padding: 0;
}

.layout-menuitem-category {
    font-weight: 700;
    font-size: 0.875rem;
    text-transform: uppercase;
    color: var(--p-text-color);
    margin: 1rem 0 0.5rem 0;
    padding-left: 1rem;
}

.layout-menuitem-link {
    display: flex;
    align-items: center;
    position: relative;
    outline: 0 none;
    color: var(--p-text-color);
    cursor: pointer;
    padding: 0.75rem 1rem;
    border-radius: 12px;
    transition: background-color 0.2s, box-shadow 0.2s;
    text-decoration: none;
    margin-bottom: 0.25rem;
}

.layout-menuitem-link:hover {
    background-color: var(--p-surface-hover);
}

.layout-menuitem-link.active-route {
    background-color: var(--p-primary-color);
    color: var(--p-primary-contrast-color);
}

.layout-menuitem-icon {
    margin-right: 0.5rem;
}
</style>
