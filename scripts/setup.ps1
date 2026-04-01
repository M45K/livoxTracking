# PowerShell setup script for Windows 10/11 (x64)
# Builds Livox-SDK2 and prints the configure command for this service.

param(
    [string]$LivoxRoot = "$HOME\\Livox-SDK2",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Platform = "x64",
    [string]$BuildType = "Release"
)

function Run($cmd) {
    Write-Host ">> $cmd" -ForegroundColor Cyan
    & cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $cmd" }
}

if (-not (Test-Path $LivoxRoot)) {
    Run "git clone https://github.com/Livox-SDK/Livox-SDK2.git `"$LivoxRoot`""
}

Run "cmake -S `"$LivoxRoot`" -B `"$LivoxRoot\\build`" -G `"$Generator`" -A $Platform"
Run "cmake --build `"$LivoxRoot\\build`" --config $BuildType"

Write-Host "--------------------------------------------------"
Write-Host "Livox SDK2 build complete."
Write-Host "Configure this repo with:"
Write-Host "cmake -S . -B build -G `"$Generator`" -A $Platform -DLIVOX_SDK=$LivoxRoot"
Write-Host "cmake --build build --config $BuildType"
