# mGBAWEB
===
Build 3 Live: [https://partakithrc1.github.io/mGBAWEB/](https://partakithware.github.io/mGBAWEB/)

```
sudo apt install cmake git python3 build-essential
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh
```
```
git clone https://github.com/libretro/mgba.git   # libretro-maintained mirror, has the libretro core
cd mgba
emmake make -f Makefile.libretro platform=emscripten
```
```
# you're already in ~/emsdk/mgba with emsdk sourced and it likely error'd run the below.
```
```
mkdir build-libretro && cd build-libretro

emcmake cmake .. \
  -DLIBMGBA_ONLY=ON \
  -DBUILD_LIBRETRO=ON \
  -DLIBRETRO_STATIC=ON \
  -DM_CORE_GBA=ON \
  -DM_CORE_GB=ON \
  -DCMAKE_BUILD_TYPE=Release

emmake make mgba_libretro -j$(nproc)
```
Frontend.c && Build.sh place in. ```'~/emsdk/mgba/build-libretro'```

Then run build: 
```
chmod +x build.sh && ./build.sh
```

While the repository frontend is MIT-licensed, the underlying mGBA emulator engine/Wasm module is subject to the Mozilla Public License v2.0
[https://github.com/libretro/mgba](https://github.com/libretro/mgba)
