# DXVK Native triangle example using SDL3 WSI

## 1. Build and Install DXVK

```sh
sudo apt install -y cmake meson
sudo apt install -y glslang-tools libsdl3-dev libvulkan-dev

git clone --single-branch -b v3.1 https://github.com/doitsujin/dxvk
cd dxvk
git submodule update --init --recursive

meson setup ../build-dxvk-native -Dnative_sdl3=enabled -Dnative_glfw=disabled -Dnative_sdl2=disabled -Denable_d3d8=false -Denable_d3d9=false -Denable_d3d10=false -Dbuild_id=false -Dbuildtype=release
meson install -C ../build-dxvk-native

```

## 2. Compile

```sh
cmake -S . -B ../tmp/DxvkTriangle
cmake --build ../tmp/DxvkTriangle

```

## 3. Run

```sh
DXVK_WSI_DRIVER=SDL3 ../bin/Release/dxvk_triangle

```
