if ($env:VCPKG_ROOT -eq "") {
    echo VCPKG_ROOT is not set!
    Exit-PSHostProcess
}

if (-not (Test-Path (Join-Path $env:VCPKG_ROOT "vcpkg.exe"))) {
    echo vcpkg.exe is not found!
    Exit-PSHostProcess
}

Push-Location $PSScriptRoot
cmake -S . -B tmp --preset vcpkg
if (-not $?) {
    exit $LASTEXITCODE
}
cmake --build tmp --config Release
Pop-Location
