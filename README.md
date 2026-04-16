# SDL3 + Vulkan C++ examples

## Requirements

- CMake (version 4.3 or higher)
- vcpkg
- C++20 compatible compiler (MSVC, GCC, or Clang)

## How to compile

I only tested on Debian Forky and Windows 11.

### Debian

1. Compile Shaders

```sh
cd src/umutech/Sdl3VkTriangle
bash ./compile_shaders.sh
```

2. Compile Project

```sh
cd src/umutech/Sdl3VkTriangle
bash ./build.sh

../bin/Release/Sdl3VkTriangle
```

### Windows

1. Compile Shaders

```pwsh
cd src\umutech\Sdl3VkTriangle
./compile_shaders.ps1
```

2. Compile Project

- Use MSVC

  Open the solution file, and click the `Start Without Debugging`/`Local Windows Debugger` button.

- Use CMake + MSBuild

```pwsh
cd src\umutech\Sdl3VkTriangle
..\build.ps1

..\bin\Release\Sdl3VkTriangle.exe
```

### 