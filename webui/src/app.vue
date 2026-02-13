<template>
  <div id="app">
    <div class="app-container">
      <Header />
      <main class="main-content">
        <RouterView v-slot="{ Component }">
          <Transition name="page" mode="out-in">
            <component :is="Component" />
          </Transition>
        </RouterView>
      </main>
      <footer class="app-footer">
        <div class="footer-content">
          <small class="text-muted">HB-RF-ETH-ng {{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }} &copy; 2025-2026 Xerolux</small>
          <button class="sponsor-btn" @click="showSponsorModal = true">
            <span aria-hidden="true">❤️</span> Sponsor
          </button>
        </div>
      </footer>
    </div>

    <SponsorModal v-model="showSponsorModal" />
  </div>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { useLoginStore, useSysInfoStore } from './stores.js'
import Header from './header.vue'
import SponsorModal from './components/SponsorModal.vue'

const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()
const showSponsorModal = ref(false)

// Idle timeout is handled globally in main.js via the login store's
// activity tracking (10-minute timeout with cross-tab sync via localStorage).

onMounted(() => {
  sysInfoStore.update().catch(() => {})
})
</script>

<style scoped>
#app {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

.app-container {
  max-width: 900px;
  width: 100%;
  margin: 0 auto;
  padding: var(--spacing-md);
  flex: 1;
  display: flex;
  flex-direction: column;
}

@media (min-width: 768px) {
  .app-container {
    padding: var(--spacing-lg);
  }
}

@media (max-width: 480px) {
  .app-container {
    padding: var(--spacing-sm);
  }
}

/* Safe area padding for notch devices */
@supports (padding: env(safe-area-inset-left)) {
  .app-container {
    padding-left: max(var(--spacing-sm), env(safe-area-inset-left));
    padding-right: max(var(--spacing-sm), env(safe-area-inset-right));
  }
}

.main-content {
  flex: 1;
  margin-bottom: var(--spacing-lg);
  /* Prevent content from overflowing horizontally on mobile */
  overflow-x: hidden;
}

@media (max-width: 768px) {
  .main-content {
    margin-bottom: var(--spacing-md);
  }
}

.app-footer {
  padding: var(--spacing-lg) 0 var(--spacing-xl);
  /* border-top: 1px solid var(--color-border); */ /* Cleaner look without border */
  margin-top: auto;
}

@media (max-width: 768px) {
  .app-footer {
    padding: var(--spacing-md) 0 var(--spacing-lg);
  }
}

.footer-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--spacing-md);
}

.sponsor-btn {
  background: var(--color-surface);
  border: 1px solid var(--color-border-light);
  padding: 8px 20px;
  border-radius: var(--radius-full);
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s;
  display: flex;
  align-items: center;
  gap: 8px;
  box-shadow: var(--shadow-sm);
}

.sponsor-btn:hover {
  background: #fff0f0;
  color: #ff3b30;
  border-color: #ff3b30; /* Keep border color change for feedback */
  transform: translateY(-2px);
  box-shadow: var(--shadow-md);
}
</style>
