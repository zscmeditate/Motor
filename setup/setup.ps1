<#
  STM32 电机驱动固件 —— 离线开发环境恢复脚本

  离线包内容（setup/offline，约 800MB）:
    platforms/                   PlatformIO ststm32 平台
    packages/                    编译器、STM32CubeF0、OpenOCD、SCons 等
    wheels/                      PlatformIO 及其依赖的 Python wheel
    python-3.14.3-amd64.exe      Python 安装器

  用法:
    powershell -ExecutionPolicy Bypass -File setup\setup.ps1
    powershell -ExecutionPolicy Bypass -File setup\setup.ps1 -Flash
#>
param(
    [switch]$Flash
)

$ErrorActionPreference = 'Stop'

$Offline  = Join-Path $PSScriptRoot 'offline'
$Project  = Split-Path $PSScriptRoot -Parent
$Firmware = Join-Path $Project 'firmware'
$Wheels   = Join-Path $Offline 'wheels'

if (-not (Test-Path (Join-Path $Offline 'platforms'))) {
    Write-Error "离线包不完整: 缺少 $Offline\platforms"
    exit 1
}

# —— 1. 让 PlatformIO 使用随项目携带的核心目录 ——
$env:PLATFORMIO_CORE_DIR = $Offline
Write-Host "[1/4] PLATFORMIO_CORE_DIR = $Offline"

# —— 2. 检查 Python，缺失时启动离线安装器 ——
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $installer = Join-Path $Offline 'python-3.14.3-amd64.exe'
    if (Test-Path $installer) {
        Write-Host "[2/4] 未检测到 Python，启动安装器（请勾选 Add python.exe to PATH）..."
        Start-Process -FilePath $installer -Wait
        $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
                    [Environment]::GetEnvironmentVariable('Path', 'User')
        $python = Get-Command python -ErrorAction SilentlyContinue
    }
}
if (-not $python) {
    Write-Warning "未检测到 Python，请手动安装 $Offline\python-3.14.3-amd64.exe 后重新运行本脚本"
    exit 1
}
Write-Host "[2/4] Python $(& python --version 2>&1)"

# —— 3. 检查 PlatformIO，缺失时离线安装 ——
& python -m platformio --version *> $null
if ($LASTEXITCODE -ne 0) {
    $ver = & python -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"
    if ($ver -ne '3.14') {
        Write-Warning "离线 wheels 适配 Python 3.14，当前为 $ver，请改用 $Offline\python-3.14.3-amd64.exe"
        exit 1
    }
    Write-Host "[3/4] 离线安装 PlatformIO ..."
    & python -m pip install --no-index --find-links $Wheels platformio
    if ($LASTEXITCODE -ne 0) { exit 1 }
} else {
    Write-Host "[3/4] PlatformIO 已安装"
}

# —— 4. 编译固件 ——
Write-Host "[4/4] 编译固件 ..."
& python -m platformio run -d $Firmware
if ($LASTEXITCODE -ne 0) { exit 1 }

if ($Flash) {
    Write-Host "烧录固件 (ST-Link) ..."
    & python -m platformio run -d $Firmware -t upload
    if ($LASTEXITCODE -ne 0) { exit 1 }
}
