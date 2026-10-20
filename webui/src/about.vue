<template>
  <div class="about-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="info" /> {{ t('nav.about') }}</span>
        <h1 class="hero-title">{{ t('about.title') }}</h1>
        <p class="hero-subtitle">{{ t('about.description') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> {{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '...' }}</span>
      </div>
    </section>

    <!-- Project Info -->
    <section class="surface-card">
      <div class="card-section-title">
        <span class="icon-badge soft"><AppIcon name="router" /></span>
        <h3>HB-RF-ETH-ng</h3>
      </div>
      <div class="project-info">
        <div class="version-badge">
          <span class="version-label">{{ t('sysinfo.version') }}</span>
          <span class="version-number">{{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '...' }}</span>
        </div>
        <p class="project-description">{{ t('about.description') }}</p>
      </div>
    </section>

    <!-- Fork Info -->
    <section class="surface-card">
      <div class="card-section-title">
        <span class="icon-badge soft"><AppIcon name="gitFork" /></span>
        <h3>{{ t('about.fork') }}</h3>
      </div>
      <div class="info-card">
        <p>{{ t('about.forkDescription') }}</p>
        <a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_blank" rel="noopener noreferrer" class="github-link">
          <AppIcon name="link" class="link-icon" />
          {{ t('about.githubRepository') }}
        </a>
        <div class="license-info">
          <p>{{ t('about.copyrightXerolux') }}</p>
          <p>
            {{ t('about.firmwareLicense') }} HB-RF-ETH-ng {{ t('about.under') }}
            <a href="https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/LICENSE.md" target="_blank" rel="noopener noreferrer">CC BY-NC-SA 4.0</a>
          </p>
          <p>{{ t('privacy.updateCheck') }}</p>
        </div>
      </div>
    </section>

    <!-- Original Author -->
    <section class="surface-card">
      <div class="card-section-title">
        <span class="icon-badge soft"><AppIcon name="user" /></span>
        <h3>{{ t('about.original') }}</h3>
      </div>
      <div class="info-card">
        <p>{{ t('about.copyrightOriginal') }}</p>
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
    </section>

    <!-- Third Party Software -->
    <section class="surface-card">
      <div class="card-section-title">
        <span class="icon-badge soft"><AppIcon name="package" /></span>
        <h3>{{ t('thirdParty.title') }}</h3>
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
    </section>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSysInfoStore } from './stores.js'

const { t } = useI18n()
const sysInfoStore = useSysInfoStore()

onMounted(() => {
  sysInfoStore.update().catch((error) => {
    console.warn('Failed to load system info on about page:', error)
  })
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
  min-width: 0;
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
  background: var(--color-primary);
  color: white;
  padding: var(--spacing-sm) var(--spacing-lg);
  border-radius: var(--radius-full);
  font-weight: var(--font-weight-bold);
  width: fit-content;
  max-width: 100%;
  flex-wrap: wrap;
}

.version-label {
  font-size: var(--fs-xs);
  opacity: 0.9;
}

.version-number {
  font-size: var(--fs-md);
}

.project-description {
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
  margin: 0;
  overflow-wrap: anywhere;
}

.info-card {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
  min-width: 0;
}

.info-card p {
  margin: 0;
  color: var(--color-text);
  line-height: 1.6;
  overflow-wrap: anywhere;
}

.info-card a {
  color: var(--color-primary);
  text-decoration: none;
  font-weight: var(--font-weight-medium);
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
  font-weight: var(--font-weight-semibold);
  transition: all var(--transition-fast);
  width: fit-content;
  max-width: 100%;
  white-space: normal;
  overflow-wrap: anywhere;
}

.github-link:hover {
  border-color: var(--color-primary);
  box-shadow: var(--shadow-sm);
  text-decoration: none !important;
}

.link-icon {
  font-size: var(--fs-lg);
  display: inline-flex;
}

.license-info {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-sm);
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
  border: 1px solid var(--color-border);
  min-width: 0;
}

.license-info p {
  font-size: var(--fs-xs);
  color: var(--color-text-secondary);
}

.libs-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(min(200px, 100%), 1fr));
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
  gap: var(--spacing-sm);
  min-width: 0;
}

.lib-card:hover {
  border-color: var(--color-primary);
  box-shadow: var(--shadow-sm);
  transform: translateY(-1px);
}

.lib-name {
  font-weight: var(--font-weight-medium);
  color: var(--color-text);
  font-size: var(--fs-xs);
  min-width: 0;
  overflow-wrap: anywhere;
}

.lib-license {
  font-size: var(--fs-2xs);
  color: var(--color-text-secondary);
  background: var(--color-surface);
  padding: 0.125rem 0.5rem;
  border-radius: var(--radius-full);
  font-weight: var(--font-weight-medium);
  flex: 0 0 auto;
}

.text-muted {
  color: var(--color-text-secondary) !important;
  font-size: var(--fs-xs);
}

@media (max-width: 768px) {
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
