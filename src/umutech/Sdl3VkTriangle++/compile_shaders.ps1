$GLSLC_BIN = "$env:VCPKG_ROOT\packages\shaderc_x64-windows\tools\shaderc\glslc.exe"
if (-not (Test-Path $GLSLC_BIN)) {
    $GLSLC_BIN = "$env:VCPKG_ROOT\installed\x64-windows\tools\shaderc\glslc.exe"
}
if (-not (Test-Path $GLSLC_BIN)) {
    Write-Host "GLSL compiler not found. Please ensure shaderc is installed via `vcpkg install shaderc`."
    Read-Host "Press Enter to exit"
    exit 1
}

& $GLSLC_BIN shaders\triangle++.vert -o ..\assets\triangle++_vert.spv
& $GLSLC_BIN shaders\triangle++.frag -o ..\assets\triangle++_frag.spv
