[CmdletBinding()]
param(
    [switch]$SkipPackage,
    [switch]$RunTests,
    [switch]$DownloadSdk
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $projectRoot 'build'
$distDir = Join-Path $projectRoot 'dist'

New-Item -ItemType Directory -Force -Path $buildDir, $distDir | Out-Null

$vcvarsCandidates = @(
    'C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vcvars) {
    throw 'Visual Studio C++ Build Tools (x64) were not found.'
}

$source = Join-Path $projectRoot 'src\BiimTemplate.cpp'
$sdkInclude = Join-Path $projectRoot 'aviutl2_sdk'
$sdkHeader = Join-Path $sdkInclude 'filter2.h'
$sdkPluginHeader = Join-Path $sdkInclude 'plugin2.h'
if ($DownloadSdk -or -not (Test-Path -LiteralPath $sdkHeader) -or -not (Test-Path -LiteralPath $sdkPluginHeader)) {
    $sdkUri = 'https://spring-fragrance.mints.ne.jp/aviutl/aviutl2_sdk.zip'
    $sdkSha256 = '747EF1FB59DAC4DC1B3D54489BFBF968C18CAFE4A977C0FC85C3B79702C6A1B7'
    $sdkArchive = Join-Path $buildDir 'aviutl2_sdk.zip'
    $downloadedSdk = Join-Path $buildDir 'downloaded-sdk'

    $downloaded = $false
    for ($attempt = 1; $attempt -le 3 -and -not $downloaded; $attempt++) {
        try {
            Invoke-WebRequest -UseBasicParsing -Uri $sdkUri -OutFile $sdkArchive
            $downloaded = $true
        } catch {
            if ($attempt -eq 3) {
                throw
            }
        }
    }

    $actualSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdkArchive).Hash
    if ($actualSha256 -ne $sdkSha256) {
        throw "SDK checksum mismatch. Expected $sdkSha256, got $actualSha256."
    }

    $resolvedBuild = [System.IO.Path]::GetFullPath($buildDir).TrimEnd('\') + '\'
    $resolvedSdk = [System.IO.Path]::GetFullPath($downloadedSdk)
    if (-not $resolvedSdk.StartsWith($resolvedBuild, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The downloaded SDK directory is outside the build directory.'
    }
    if (Test-Path -LiteralPath $resolvedSdk) {
        Remove-Item -LiteralPath $resolvedSdk -Recurse -Force
    }
    Expand-Archive -LiteralPath $sdkArchive -DestinationPath $resolvedSdk
    $sdkInclude = $resolvedSdk
    Write-Host "SDK: downloaded and verified ($sdkSha256)"
} else {
    Write-Host "SDK: $sdkInclude"
}
$output = Join-Path $distDir 'BiimTemplate.auf2'
$object = Join-Path $buildDir 'BiimTemplate.obj'
$pdb = Join-Path $buildDir 'BiimTemplate.pdb'

$environmentLines = & $env:ComSpec /d /s /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to initialize the Visual Studio build environment.'
}
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -LiteralPath ("Env:" + $matches[1]) -Value $matches[2]
    }
}

$compilerArguments = @(
    '/nologo',
    '/std:c++20',
    '/O2',
    '/EHsc',
    '/LD',
    '/W4',
    '/permissive-',
    $source,
    "/I$sdkInclude",
    "/Fo$object",
    '/link',
    '/NOIMPLIB',
    "/OUT:$output",
    "/PDB:$pdb",
    'user32.lib',
    'gdi32.lib'
)

& cl.exe @compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

if ($RunTests) {
    $testSource = Join-Path $projectRoot 'tests\PluginSmoke.cpp'
    $testExe = Join-Path $buildDir 'PluginSmoke.exe'
    $testObject = Join-Path $buildDir 'PluginSmoke.obj'
    $testPdb = Join-Path $buildDir 'PluginSmoke.pdb'
    $testArguments = @(
        '/nologo',
        '/std:c++20',
        '/O2',
        '/EHsc',
        '/W4',
        '/permissive-',
        $testSource,
        "/I$sdkInclude",
        "/Fo$testObject",
        "/Fe$testExe",
        '/link',
        "/PDB:$testPdb"
    )
    & cl.exe @testArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke test build failed with exit code $LASTEXITCODE."
    }
    & $testExe $output
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke test failed with exit code $LASTEXITCODE."
    }
}

if (-not $SkipPackage) {
    $stageDir = Join-Path $buildDir 'package-stage'
    $resolvedBuild = [System.IO.Path]::GetFullPath($buildDir).TrimEnd('\') + '\'
    $resolvedStage = [System.IO.Path]::GetFullPath($stageDir)
    if (-not $resolvedStage.StartsWith($resolvedBuild, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The package staging directory is outside the build directory.'
    }
    if (Test-Path -LiteralPath $resolvedStage) {
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }

    $pluginDir = Join-Path $stageDir 'Plugin\BiimTemplate'
    New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
    Copy-Item -LiteralPath $output -Destination $pluginDir
    Copy-Item -LiteralPath (Join-Path $projectRoot 'package\package.ini') -Destination $stageDir
    Copy-Item -LiteralPath (Join-Path $projectRoot 'package\package.txt') -Destination $stageDir

    $packageOutput = Join-Path $distDir 'BiimTemplate.au2pkg.zip'
    Compress-Archive -Path (Join-Path $stageDir '*') -DestinationPath $packageOutput -CompressionLevel Optimal -Force
}

Write-Host "Built: $output"
if ($RunTests) {
    Write-Host 'Smoke test: passed'
}
if (-not $SkipPackage) {
    Write-Host "Package: $(Join-Path $distDir 'BiimTemplate.au2pkg.zip')"
}
