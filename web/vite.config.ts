import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        assetFileNames: "a/[hash:10][extname]",
        chunkFileNames: "c/[hash:10].js",
        entryFileNames: "e/[hash:10].js"
      }
    }
  },
})
