import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { viteCompression } from 'vite-plugin-compression'
import { resolve } from 'path'

export default defineConfig({
  plugins: [
    vue(),
    viteCompression({
      algorithm: 'gzip',
      ext: '.gz',
      threshold: 0,
      deleteOriginFile: false
    })
  ],
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src')
    }
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rollupOptions: {
      output: {
        entryFileNames: 'index-[hash].js',
        chunkFileNames: 'index-[hash].js',
        assetFileNames: 'index-[hash].[ext]',
        manualChunks: {
          'vendor': ['vue', 'vue-router', 'pinia'],
          'bootstrap': ['bootstrap', 'bootstrap-vue-next']
        }
      }
    },
    minify: 'terser',
    terserOptions: {
      compress: {
        drop_console: true,
        drop_debugger: true
      }
    }
  },
  server: {
    port: 1234,
    strictPort: true,
    proxy: {
      '/api': {
        target: 'http://192.168.1.1', // Default target for development
        changeOrigin: true
      }
    }
  }
})
