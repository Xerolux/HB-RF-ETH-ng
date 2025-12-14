<template>
  <div>
    <BCard
      :header="t('about.title')"
      header-tag="h6"
      header-bg-variant="secondary"
      header-text-variant="white"
      class="mb-3"
    >
      <BAlert variant="info" :model-value="true" class="mb-3">
        <strong>{{ t('about.version') }}</strong>
      </BAlert>
      <p><strong>{{ t('about.fork') }}</strong></p>
      <p>{{ t('about.forkDescription') }}</p>
      <p>
        <a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_new">GitHub Repository (Fork v2.1)</a>
      </p>
      <hr />
      <p><strong>{{ t('about.original') }}</strong></p>
      <p>Copyright (c) 2022, Alexander Reinert</p>
      <p>
        {{ t('about.firmwareLicense') }}
        <a href="https://github.com/alexreinert/HB-RF-ETH" target="_new">HB-RF-ETH Software</a>
        {{ t('about.under') }}
        <a href="https://github.com/alexreinert/HB-RF-ETH/blob/master/LICENSE.md" target="_new">CC BY-NC-SA 4.0</a>
      </p>
      <p>
        {{ t('about.hardwareLicense') }}
        <a href="https://github.com/alexreinert/PCB" target="_new">HB-RF-ETH Hardware</a>
        {{ t('about.under') }}
        <a href="https://github.com/alexreinert/PCB/blob/master/LICENSE.md" target="_new">CC BY-NC-SA 4.0</a>
      </p>
    </BCard>

    <BCard
      :header="t('thirdParty.title')"
      header-tag="h6"
      header-bg-variant="secondary"
      header-text-variant="white"
      class="mb-3"
    >
      <p>{{ t('thirdParty.containsThirdPartySoftware') }}</p>
      <p>{{ t('thirdParty.providedAsIs') }}</p>
      <BListGroup>
        <BListGroupItem v-for="(lib, index) in libs" :key="lib.name" style="cursor: pointer">
          {{ lib.name }} - <a :href="lib.website" target="_new">{{ lib.website }}</a>
        </BListGroupItem>
      </BListGroup>
    </BCard>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useI18n } from 'vue-i18n'

const { t } = useI18n()

const libs = ref([
  { name: 'Bootstrap 5', copyright: 'Copyright (c) 2011-2025 The Bootstrap Authors', license: 'MIT', website: 'https://www.getbootstrap.com/' },
  { name: 'Vue.js 3', copyright: 'Copyright (c) 2020-2025, Evan You', license: 'MIT', website: 'https://www.vuejs.org/' },
  { name: 'Pinia', copyright: 'Copyright (c) 2019-2025, Eduardo San Martin Morote', license: 'MIT', website: 'https://pinia.vuejs.org/' },
  { name: 'vue-router', copyright: 'Copyright (c) 2020-2025, Evan You', license: 'MIT', website: 'https://router.vuejs.org/' },
  { name: 'bootstrap-vue-next', copyright: 'Copyright (c) 2023-2025, BootstrapVueNext', license: 'MIT', website: 'https://bootstrap-vue-next.github.io/' },
  { name: 'Vuelidate', copyright: 'Copyright (c) 2019-2025, Damian Dulisz', license: 'MIT', website: 'https://vuelidate-next.netlify.app/' },
  { name: 'Vue I18n', copyright: 'Copyright (c) 2020-2025, kazuya kawaguchi', license: 'MIT', website: 'https://vue-i18n.intlify.dev/' },
  { name: 'axios', copyright: 'Copyright (c) 2014-present, Matt Zabriskie', license: 'MIT', website: 'https://github.com/axios/axios' },
  { name: 'Parcel', copyright: 'Copyright (c) 2017-present, Devon Govett', license: 'MIT', website: 'https://parceljs.org/' },
  { name: 'Espressif IoT Development Framework', copyright: 'Copyright 2015-2025, Espressif Systems (Shanghai) PTE LTD', license: 'Apache2', website: 'https://docs.espressif.com/projects/esp-idf/' }
])
</script>

<style scoped>
</style>
