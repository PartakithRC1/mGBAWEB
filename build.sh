#!/usr/bin/env bash
# Run this from inside build-libretro/ (the directory that already contains
# mgba_libretro.bc from your `emmake make mgba_libretro` step), after
# copying frontend.c into it.
#
#   cp ../../frontend.c .   # wherever you downloaded it to
#   ./build.sh
#
set -euo pipefail

# CMake's LIBRETRO_STATIC mode builds this as a static-library *archive*
# (ar format) but names it .bc, which fools emcc's extension-based dispatch
# into trying to compile it as bitcode source. Renaming to .a fixes it.
cp mgba_libretro.bc mgba_libretro.a

emcc frontend.c mgba_libretro.a \
  -I../src/platform/libretro \
  -O2 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createMGBAModule \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","FS","HEAPU8","HEAP16","HEAPU16"]' \
  -s EXPORTED_FUNCTIONS='["_malloc","_free","_mgba_web_init","_mgba_web_load_rom","_mgba_web_run_frame","_mgba_web_get_video_buffer","_mgba_web_get_video_width","_mgba_web_get_video_height","_mgba_web_get_video_pitch","_mgba_web_get_pixel_format","_mgba_web_get_audio_buffer","_mgba_web_get_audio_frames","_mgba_web_get_audio_rate","_mgba_web_set_button","_mgba_web_save_state","_mgba_web_load_state","_mgba_web_get_sram_ptr","_mgba_web_get_sram_size"]' \
  -lidbfs.js \
  -o mgba_web.js

echo "Built mgba_web.js + mgba_web.wasm"
