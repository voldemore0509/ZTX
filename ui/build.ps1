[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [string]$Msys2Root = 'C:\msys64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$compiler = Join-Path $Msys2Root 'ucrt64\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    $compilerCommand = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($null -eq $compilerCommand) {
        throw 'MSYS2 GCC was not found. Install mingw-w64-ucrt-x86_64-gcc, or pass -Msys2Root with your MSYS2 folder.'
    }
    $compiler = $compilerCommand.Source
}
$toolchainDirectory = Split-Path -Parent $compiler
$resourceCompiler = Join-Path $toolchainDirectory 'windres.exe'
if (-not (Test-Path -LiteralPath $resourceCompiler -PathType Leaf)) {
    throw "The resource compiler was not found: $resourceCompiler"
}

$outputDirectory = Join-Path $projectRoot 'build'
$null = New-Item -ItemType Directory -Force -Path $outputDirectory
$resourceObject = Join-Path $outputDirectory 'resources.o'
$executable = Join-Path $outputDirectory 'ZTX.exe'
$previousPath = $env:PATH
$env:PATH = $toolchainDirectory + ';' + $previousPath

try {
    Push-Location (Join-Path $projectRoot 'resources')
    try {
        & $resourceCompiler '-I' '.' '--codepage=65001' '-O' 'coff' 'ztx.rc' '-o' $resourceObject
        if ($LASTEXITCODE -ne 0) { throw "Resource compilation failed (exit $LASTEXITCODE)." }
    }
    finally { Pop-Location }

    $compilerArguments = @(
        '-std=c++17', '-Wall', '-Wextra', '-Wpedantic',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
        '-municode', '-mwindows', '-static', '-static-libgcc', '-static-libstdc++'
    )
    if ($Configuration -eq 'Release') {
        $compilerArguments += @('-Os', '-flto', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-s')
    }
    else {
        $compilerArguments += @('-Og', '-g')
    }
    $compilerArguments += @(
        (Join-Path $projectRoot 'src\main.cpp'),
        (Join-Path $projectRoot 'src\preferences.cpp'),
        $resourceObject,
        '-lgdiplus', '-ldwmapi', '-lcomctl32', '-lole32', '-lshell32',
        '-luuid', '-lgdi32', '-luser32', '-lshlwapi',
        '-o', $executable
    )
    & $compiler @compilerArguments
    if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed (exit $LASTEXITCODE)." }
    $size = (Get-Item -LiteralPath $executable).Length
    Write-Host ('Built {0} ({1:N0} bytes, {2}).' -f $executable, $size, $Configuration)
}
finally {
    $env:PATH = $previousPath
}
