# SDL3 + Vulkan C++ examples

## Requirements

- CMake (version 4.3 or higher)
- vcpkg
- C++20 compatible compiler (MSVC, GCC, or Clang)

## How to compile

I only tested on Debian Forky and Windows 11.

### Debian

```sh
cd src/umutech
bash ./build.shaders.sh

bin/Release/Sdl3VkTriangle
```

### Windows

```pwsh
cd src\umutech
./build.ps1

bin\Release\Sdl3VkTriangle.exe
```
If you use MSVC, just open the solution file, and click the `Start Without Debugging`/`Local Windows Debugger` button.
