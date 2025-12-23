<template>
  <div v-if="visible" class="loading-overlay" role="alert" aria-live="assertive">
    <div class="text-center text-white">
      <BSpinner style="width: 3rem; height: 3rem;" variant="light" label="Loading..."></BSpinner>
      <h4 class="mt-3">{{ message }}</h4>
    </div>
  </div>
</template>

<script setup>
defineProps({
  visible: {
    type: Boolean,
    default: false
  },
  message: {
    type: String,
    default: 'Loading...'
  }
})
</script>

<style scoped>
.loading-overlay {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: rgba(0, 0, 0, 0.85);
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
  backdrop-filter: blur(5px);
}
</style>
