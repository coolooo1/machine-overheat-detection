param(
    [string]$Compiler = 'gcc',
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$isWindowsHost = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
$suffix = if ($isWindowsHost) { '.exe' } else { '' }
$compilerCommand = Get-Command $Compiler -ErrorAction Stop
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null

function Build-Program([string]$Name, [string[]]$Sources) {
    $arguments = @('-std=c17', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-g',
                   '-I', (Join-Path $projectRoot 'include'))
    foreach ($source in $Sources) {
        $arguments += Join-Path $projectRoot $source
    }
    $arguments += @('-o', (Join-Path $buildDirectory ($Name + $suffix)))
    & $compilerCommand.Source @arguments
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $Name" }
    Write-Output "Built: $Name"
}

Build-Program 'machineguard' @('src/main.c', 'src/controller.c', 'src/logger.c')
if ($Test) {
    Build-Program 'controller_tests' @('tests/controller_tests.c', 'src/controller.c')
    Build-Program 'logger_tests' @('tests/logger_tests.c', 'src/logger.c', 'src/controller.c')
    # 測試版本改連結失敗替身；正式程式永遠使用 src/logger.c。
    Build-Program 'machineguard_failure' @('src/main.c', 'src/controller.c', 'tests/logger_failure_stub.c')
    foreach ($name in @('controller_tests', 'logger_tests')) {
        & (Join-Path $buildDirectory ($name + $suffix))
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $name" }
    }
    & (Join-Path $projectRoot 'tests/integration_tests.ps1') -BuildDirectory $buildDirectory
}
