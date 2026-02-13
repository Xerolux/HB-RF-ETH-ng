<template>
  <div class="about-page">
    <!-- Project Info -->
    <div class="about-section">
      <div class="section-header">
        <span class="section-icon">📡</span>
        <h3 class="section-title">HB-RF-ETH-ng</h3>
      </div>
      <div class="project-info">
        <div class="version-badge">
          <span class="version-label">{{ t('sysinfo.version') }}</span>
          <span class="version-number">{{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '...' }}</span>
        </div>
        <p class="project-description">{{ t('about.description') }}</p>
      </div>
    </div>

    <!-- Fork Info -->
    <div class="about-section">
      <div class="section-header">
        <span class="section-icon">🔀</span>
        <h3 class="section-title">{{ t('about.fork') }}</h3>
      </div>
      <div class="info-card">
        <p>{{ t('about.forkDescription') }}</p>
        <a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_blank" rel="noopener noreferrer" class="github-link">
          <span class="link-icon">🔗</span>
          GitHub Repository
        </a>
        <div class="license-info">
          <p>Copyright (c) 2025, Xerolux</p>
          <p>
            {{ t('about.firmwareLicense') }} HB-RF-ETH-ng {{ t('about.under') }}
            <a href="https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/LICENSE.md" target="_blank" rel="noopener noreferrer">CC BY-NC-SA 4.0</a>
          </p>
        </div>
      </div>
    </div>

    <!-- Original Author -->
    <div class="about-section">
      <div class="section-header">
        <span class="section-icon">👤</span>
        <h3 class="section-title">{{ t('about.original') }}</h3>
      </div>
      <div class="info-card">
        <p>Copyright (c) 2022, Alexander Reinert</p>
        <div class="license-info">
          <p>
            {{ t('about.firmwareLicense') }}
            <a href="https://github.com/alexreinert/HB-RF-ETH" target="_blank" rel="noopener noreferrer">HB-RF-ETH Software</a>
            {{ t('about.under') }}
            <a href="https://github.com/alexreinert/HB-RF-ETH/blob/master/LICENSE.md" target="_blank" rel="noopener noreferrer">CC BY-NC-SA 4.0</a>
          </p>
          <p>
            {{ t('about.hardwareLicense') }}
            <a href="https://github.com/alexreinert/PCB" target="_blank" rel="noopener noreferrer">HB-RF-ETH Hardware</a>
            {{ t('about.under') }}
            <a href="https://github.com/alexreinert/PCB/blob/master/LICENSE.md" target="_blank" rel="noopener noreferrer">CC BY-NC-SA 4.0</a>
          </p>
        </div>
      </div>
    </div>

    <!-- Third Party Software -->
    <div class="about-section">
      <div class="section-header">
        <span class="section-icon">📦</span>
        <h3 class="section-title">{{ t('thirdParty.title') }}</h3>
      </div>
      <div class="info-card">
        <p class="text-muted">{{ t('thirdParty.containsThirdPartySoftware') }}</p>
        <p class="text-muted">{{ t('thirdParty.providedAsIs') }}</p>
        <div class="libs-grid">
          <a
            v-for="lib in libs"
            :key="lib.name"
            :href="lib.website"
            target="_blank"
            rel="noopener noreferrer"
            class="lib-card"
          >
            <span class="lib-name">{{ lib.name }}</span>
            <span class="lib-license">{{ lib.license }}</span>
          </a>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore } from './stores.js'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()

onMounted(() => {
  sysInfoStore.update().catch(() => {})
})

const libs = ref([
  { name: 'Bootstrap 5', license: 'MIT', website: 'https://www.getbootstrap.com/' },
  { name: 'Vue.js 3', license: 'MIT', website: 'https://www.vuejs.org/' },
  { name: 'Pinia', license: 'MIT', website: 'https://pinia.vuejs.org/' },
  { name: 'vue-router', license: 'MIT', website: 'https://router.vuejs.org/' },
  { name: 'bootstrap-vue-next', license: 'MIT', website: 'https://bootstrap-vue-next.github.io/' },
  { name: 'Vuelidate', license: 'MIT', website: 'https://vuelidate-next.netlify.app/' },
  { name: 'Vue I18n', license: 'MIT', website: 'https://vue-i18n.intlify.dev/' },
  { name: 'axios', license: 'MIT', website: 'https://github.com/axios/axios' },
  { name: 'Vite', license: 'MIT', website: 'https://vitejs.dev/' },
  { name: 'ESP-IDF', license: 'Apache 2.0', website: 'https://docs.espressif.com/projects/esp-idf/' }
])
</script>

<style scoped>
.about-page {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-lg);
}

.about-section {
  background: var(--color-surface);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-md);
  overflow: hidden;
  padding: var(--spacing-lg);
}

.section-header {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  margin-bottom: var(--spacing-lg);
  padding-bottom: var(--spacing-md);
  border-bottom: 1px solid var(--color-border);
}

.section-icon {
  font-size: 1.5rem;
  filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.1));
}

.section-title {
  font-size: 1.125rem;
  font-weight: 600;
  color: var(--color-text);
  margin: 0;
}

.project-info {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.version-badge {
  display: inline-flex;
  align-items: center;
  gap: var(--spacing-sm);
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  color: white;
  padding: var(--spacing-sm) var(--spacing-lg);
  border-radius: var(--radius-full);
  font-weight: 600;
  width: fit-content;
}

.version-label {
  font-size: 0.875rem;
  opacity: 0.9;
}

.version-number {
  font-size: 1rem;
}

.project-description {
  color: var(--color-text-secondary);
  font-size: 0.9375rem;
  margin: 0;
}

.info-card {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.info-card p {
  margin: 0;
  color: var(--color-text);
  line-height: 1.6;
}

.info-card a {
  color: var(--color-primary);
  text-decoration: none;
  font-weight: 500;
}

.info-card a:hover {
  text-decoration: underline;
}

.github-link {
  display: inline-flex;
  align-items: center;
  gap: var(--spacing-sm);
  padding: var(--spacing-sm) var(--spacing-md);
  background: var(--color-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-md);
  color: var(--color-primary) !important;
  font-weight: 600;
  transition: all var(--transition-fast);
  width: fit-content;
}

.github-link:hover {
  border-color: var(--color-primary);
  box-shadow: var(--shadow-sm);
  text-decoration: none !important;
}

.link-icon {
  font-size: 1.125rem;
}

.license-info {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-sm);
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
  border: 1px solid var(--color-border);
}

.license-info p {
  font-size: 0.875rem;
  color: var(--color-text-secondary);
}

.libs-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: var(--spacing-sm);
}

.lib-card {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: var(--spacing-md);
  background: var(--color-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-md);
  text-decoration: none !important;
  transition: all var(--transition-fast);
}

.lib-card:hover {
  border-color: var(--color-primary);
  box-shadow: var(--shadow-sm);
  transform: translateY(-1px);
}

.lib-name {
  font-weight: 500;
  color: var(--color-text);
  font-size: 0.875rem;
}

.lib-license {
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  background: var(--color-surface);
  padding: 0.125rem 0.5rem;
  border-radius: var(--radius-full);
  font-weight: 500;
}

.text-muted {
  color: var(--color-text-secondary) !important;
  font-size: 0.875rem;
}

@media (max-width: 768px) {
  .about-section {
    padding: var(--spacing-md);
    border-radius: var(--radius-md);
  }

  .section-header {
    margin-bottom: var(--spacing-md);
    padding-bottom: var(--spacing-sm);
  }

  .libs-grid {
    grid-template-columns: 1fr 1fr;
    gap: var(--spacing-xs);
  }

  .lib-card {
    padding: var(--spacing-sm) var(--spacing-md);
  }

  .license-info {
    padding: var(--spacing-sm) var(--spacing-md);
  }
}

@media (max-width: 480px) {
  .libs-grid {
    grid-template-columns: 1fr;
  }
}
</style>
