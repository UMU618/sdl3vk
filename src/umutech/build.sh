#!/usr/bin/env bash

if [ -z "$VCPKG_ROOT" ]; then
    echo "VCPKG_ROOT is not set!"
    exit 1
fi

if [ ! -f "${VCPKG_ROOT}/vcpkg" ]; then
    echo "vcpkg is not found!"
    exit 2
fi

pushd "$(dirname "$0")"

bash Sdl3VkTriangle/compile_shaders.sh
bash Sdl3VkTriangle++/compile_shaders.sh

cmake -S . -B tmp --preset vcpkg -DCMAKE_BUILD_TYPE=Release -G Ninja
if [ $? -eq 0 ]; then
    cmake --build tmp
fi
popd
