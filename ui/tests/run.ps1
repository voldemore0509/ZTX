[CmdletBinding()]
param(
    [string]$Msys2Root = 'C:\msys64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $Msys2Root 'ucrt64\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    $compilerCommand = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($null -eq $compilerCommand) {
        throw 'MSYS2 GCC was not found. Install mingw-w64-ucrt-x86_64-gcc, or pass -Msys2Root.'
    }
    $compiler = $compilerCommand.Source
}

$outputDirectory = Join-Path $projectRoot 'build'
$null = New-Item -ItemType Directory -Force -Path $outputDirectory
$testExecutable = Join-Path $outputDirectory 'preferences_test.exe'
$previousPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $previousPath

try {
    $compilerArguments = @(
        '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-static', '-static-libgcc', '-static-libstdc++',
        '-I', (Join-Path $projectRoot 'src'),
        (Join-Path $PSScriptRoot 'preferences_test.cpp'),
        (Join-Path $projectRoot 'src\preferences.cpp'),
        '-o', $testExecutable
    )
    & $compiler @compilerArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Preference test compilation failed (exit $LASTEXITCODE)."
    }
    & $testExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "Preference tests failed (exit $LASTEXITCODE)."
    }
}
finally {
    $env:PATH = $previousPath
}
