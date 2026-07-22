import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'
import { readFileSync, writeFileSync, existsSync } from 'fs'
import { createHash } from 'crypto'
import { join } from 'path'

const packageMetadata = JSON.parse(
  readFileSync(new URL('./package.json', import.meta.url), 'utf8')
)

const escapeRegExp = (value) => value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')

/**
 * Cache-busting for the embedded ESP32 WebUI.
 *
 * The firmware serves fixed asset names. Query parameters provide a new browser
 * cache key for every build while the ESP-IDF URI handlers continue to match the
 * same paths.
 */
function cacheBustingPlugin() {
  return {
    name: 'esp32-cache-busting',
    writeBundle(opts) {
      const outDir = opts.dir || 'dist'
      const indexPath = join(outDir, 'index.html')
      if (!existsSync(indexPath)) return

      let html = readFileSync(indexPath, 'utf8')
      for (const file of ['main.js', 'main.css', 'manifest.webmanifest', 'icon-256.png', 'favicon.ico']) {
        const assetPath = join(outDir, file)
        if (!existsSync(assetPath)) continue
        const hash = createHash('sha256').update(readFileSync(assetPath)).digest('hex').slice(0, 8)
        const escaped = escapeRegExp(file)
        const refPattern = new RegExp(`(["'(=\\s]/?${escaped})\\?v=[a-f0-9]{1,16}`, 'g')
        html = html.replace(refPattern, '$1')
        const plainPattern = new RegExp(`(["'(=\\s]/?${escaped})(["'#?])`, 'g')
        html = html.replace(plainPattern, `$1?v=${hash}$2`)
      }

      writeFileSync(indexPath, html)
      console.log('[cache-busting] rewrote asset references in index.html')
    }
  }
}

export default defineConfig({
  plugins: [vue(), cacheBustingPlugin()],
  define: {
    __WEBUI_VERSION__: JSON.stringify(packageMetadata.version)
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src')
    }
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rolldownOptions: {
      output: {
        entryFileNames: 'main.js',
        assetFileNames: (assetInfo) => {
          const name = (assetInfo.names && assetInfo.names[0]) || assetInfo.name || ''
          if (name.endsWith('.ico')) return 'favicon.ico'
          if (name.endsWith('.css')) return 'main.css'
          return '[name].[ext]'
        },
        // The ESP32 serves one JS bundle and one CSS bundle.
        codeSplitting: false
      }
    },
    cssCodeSplit: false,
    minify: 'esbuild',
    target: 'es2020',
    chunkSizeWarningLimit: 1000
  },
  esbuild: {
    legalComments: 'none',
    drop: ['debug'],
    keepNames: false
  },
  server: {
    port: 1234,
    strictPort: true,
    proxy: {
      '/api': {
        target: 'http://192.168.1.1',
        changeOrigin: true
      }
    }
  }
})
