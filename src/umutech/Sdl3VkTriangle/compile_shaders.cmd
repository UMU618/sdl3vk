set GLSLC_BIN="%VCPKG_ROOT%\packages\shaderc_x64-windows\tools\shaderc\glslc.exe"
if not exist %GLSLC_BIN% (
    set GLSLC_BIN="%VCPKG_ROOT%\installed\x64-windows\tools\shaderc\glslc.exe"
)
if not exist %GLSLC_BIN% (
    echo GLSL compiler not found. Please ensure shaderc is installed via `vcpkg install shaderc`.
    pause
    exit /b 1
)

%GLSLC_BIN% src\shaders\triangle.vert -o assets\triangle_vert.spv
%GLSLC_BIN% src\shaders\triangle.frag -o assets\triangle_frag.spv
pause
