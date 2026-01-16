import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import viteCompression from 'vite-plugin-compression'
import path from 'path'

export default defineConfig({
  plugins: [
    vue(),
    viteCompression({
      algorithm: 'gzip',
      ext: '.gz',
      deleteOriginFile: false
    })
  ],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    assetsDir: '',
    rollupOptions: {
      output: {
        manualChunks: undefined, // Disable default vendor chunking
        entryFileNames: 'main.js',
        // Force all chunks into the entry file if possible, or disable dynamic imports in router
        inlineDynamicImports: true,
        assetFileNames: (assetInfo) => {
          if (assetInfo.name.endsWith('.css')) return 'main.css';
          return '[name][extname]';
        }
      }
    }
  },
  server: {
    host: '0.0.0.0',
    port: 1234
  }
})
