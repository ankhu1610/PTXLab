# PTXLab Environment and Toolchain Audit Script (Windows PowerShell)
$ErrorActionPreference = "Continue"

$reportPath = Join-Path $PSScriptRoot "..\docs\env-report.txt"
$report = [System.Collections.Generic.List[string]]::new()

function Log-Line($msg) {
    Write-Host $msg
    $report.Add($msg)
}

Log-Line "=========================================================="
Log-Line "PTXLab Environment & Toolchain Audit"
Log-Line "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss K')"
Log-Line "Host OS: $([System.Environment]::OSVersion.VersionString)"
Log-Line "=========================================================="
Log-Line ""

# 1. GPU & Driver
Log-Line "--- 1. GPU & Driver Subsystem ---"
try {
    $gpuQuery = & nvidia-smi --query-gpu=name,driver_version,compute_cap --format=csv,noheader 2>&1
    if ($LASTEXITCODE -eq 0 -and $gpuQuery) {
        $parts = $gpuQuery.Split(',')
        $gpuName = $parts[0].Trim()
        $driverVer = $parts[1].Trim()
        $cc = $parts[2].Trim()
        Log-Line "  [VERIFIED] GPU: $gpuName"
        Log-Line "  [VERIFIED] Driver Version: $driverVer"
        Log-Line "  [VERIFIED] Compute Capability: sm_$($cc.Replace('.', '')) (CC $cc)"
    } else {
        Log-Line "  [UNVERIFIED] nvidia-smi query failed: $gpuQuery"
    }
} catch {
    Log-Line "  [UNVERIFIED] nvidia-smi not available"
}

Log-Line ""
# 2. Compilers & Standard Library
Log-Line "--- 2. C++20 Compilers & Toolchains ---"

# Check G++
$gppCmd = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gppCmd) {
    # Check WinLibs path
    $winlibsGpp = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "g++.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($winlibsGpp) { $gppCmd = $winlibsGpp.FullName }
} else {
    $gppCmd = $gppCmd.Source
}

if ($gppCmd) {
    $gppVer = & $gppCmd --version | Select-Object -First 1
    Log-Line "  [VERIFIED] GCC/G++: $gppVer ($gppCmd)"
} else {
    Log-Line "  [UNVERIFIED] GCC/G++: Not found in PATH"
}

# Check Clang++
$clangCmd = Get-Command clang++ -ErrorAction SilentlyContinue
if (-not $clangCmd) {
    if (Test-Path "C:\Program Files\LLVM\bin\clang++.exe") {
        $clangCmd = "C:\Program Files\LLVM\bin\clang++.exe"
    }
} else {
    $clangCmd = $clangCmd.Source
}

if ($clangCmd) {
    $clangVer = & $clangCmd --version | Select-Object -First 1
    Log-Line "  [VERIFIED] LLVM Clang++: $clangVer ($clangCmd)"
} else {
    Log-Line "  [UNVERIFIED] LLVM Clang++: Not found"
}

Log-Line ""
# 3. Build Orchestration
Log-Line "--- 3. Build Tools ---"

# Check CMake
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCmd) {
    if (Test-Path "C:\Program Files\CMake\bin\cmake.exe") {
        $cmakeCmd = "C:\Program Files\CMake\bin\cmake.exe"
    }
} else {
    $cmakeCmd = $cmakeCmd.Source
}

if ($cmakeCmd) {
    $cmakeVer = & $cmakeCmd --version | Select-Object -First 1
    Log-Line "  [VERIFIED] CMake: $cmakeVer ($cmakeCmd)"
} else {
    Log-Line "  [UNVERIFIED] CMake: Not found"
}

# Check Ninja
$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninjaCmd) {
    $ninjaItem = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "ninja.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ninjaItem) { $ninjaCmd = $ninjaItem.FullName }
} else {
    $ninjaCmd = $ninjaCmd.Source
}

if ($ninjaCmd) {
    $ninjaVer = & $ninjaCmd --version | Select-Object -First 1
    Log-Line "  [VERIFIED] Ninja: v$ninjaVer ($ninjaCmd)"
} else {
    Log-Line "  [UNVERIFIED] Ninja: Not found"
}

Log-Line ""
# 4. Version Control & Python
Log-Line "--- 4. Utilities ---"
try {
    $gitVer = & git --version 2>&1
    Log-Line "  [VERIFIED] Git: $gitVer"
} catch {
    Log-Line "  [UNVERIFIED] Git: Not found"
}

try {
    $pyVer = & python --version 2>&1
    Log-Line "  [VERIFIED] Python: $pyVer"
} catch {
    Log-Line "  [UNVERIFIED] Python: Not found"
}

Log-Line ""
# 5. CUDA Toolkit & PTX Assembler
Log-Line "--- 5. PTX Tools & Assembler ---"
$ptxasCmd = Get-Command ptxas -ErrorAction SilentlyContinue
if ($ptxasCmd) {
    $ptxasVer = & $ptxasCmd --version | Select-Object -Last 1
    Log-Line "  [VERIFIED] ptxas: $ptxasVer"
} else {
    Log-Line "  [UNVERIFIED] ptxas: Not installed / not in PATH (Required for offline PTX validation)"
}

$nvccCmd = Get-Command nvcc -ErrorAction SilentlyContinue
if ($nvccCmd) {
    $nvccVer = & $nvccCmd --version | Select-Object -Last 1
    Log-Line "  [VERIFIED] nvcc: $nvccVer"
} else {
    Log-Line "  [UNVERIFIED] nvcc: Not installed / not in PATH"
}

Log-Line ""
Log-Line "=========================================================="
Log-Line "Platform Decision: Native Windows (MinGW-w64 UCRT GCC 16.2 + LLVM 23 + CMake + Ninja)"
Log-Line "=========================================================="

# Ensure docs directory exists
$docsDir = Split-Path $reportPath
if (-not (Test-Path $docsDir)) { New-Item -ItemType Directory -Path $docsDir -Force | Out-Null }

Set-Content -Path $reportPath -Value ($report -join "`n") -Encoding utf8
Write-Host "`nReport saved to: $reportPath"
