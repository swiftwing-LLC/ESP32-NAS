param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$sdkVersion = '1.0.4258.31'
$packageName = "Microsoft.Web.WebView2.$sdkVersion.nupkg"
$packagePath = Join-Path $PSScriptRoot $packageName
$release = Join-Path $PSScriptRoot 'Release'
$compiler = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw '.NET Framework 4.x x64 compiler not found. Install the .NET Framework 4.8 Developer Pack.'
}

if ($Clean -and (Test-Path -LiteralPath $release)) {
    $resolved = [IO.Path]::GetFullPath($release)
    $root = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\') + '\'
    $expected = [IO.Path]::Combine($root, 'Release')
    $item = Get-Item -LiteralPath $release -Force
    if (-not $resolved.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or
        $resolved -ne $expected -or
        (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) {
        throw 'Refusing to clean an unexpected or linked directory.'
    }
    Remove-Item -LiteralPath $release -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $release | Out-Null
if (-not (Test-Path -LiteralPath $packagePath)) {
    $uri = "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$sdkVersion/microsoft.web.webview2.$sdkVersion.nupkg"
    Write-Host "Downloading official Microsoft.Web.WebView2 SDK $sdkVersion..."
    Invoke-WebRequest -Uri $uri -OutFile $packagePath -TimeoutSec 90
}

Add-Type -AssemblyName System.IO.Compression
$zip = [IO.Compression.ZipFile]::OpenRead($packagePath)
try {
    $files = @{
        'lib/net462/Microsoft.Web.WebView2.Core.dll' = 'Microsoft.Web.WebView2.Core.dll'
        'lib/net462/Microsoft.Web.WebView2.WinForms.dll' = 'Microsoft.Web.WebView2.WinForms.dll'
        'runtimes/win-x64/native/WebView2Loader.dll' = 'WebView2Loader.dll'
        'LICENSE.txt' = 'WEBVIEW2-SDK-LICENSE.txt'
    }
    foreach ($sourceName in $files.Keys) {
        $entry = $zip.GetEntry($sourceName)
        if ($null -eq $entry) { throw "Package entry missing: $sourceName" }
        $destination = Join-Path $release $files[$sourceName]
        $inputStream = $entry.Open()
        $outputStream = [IO.File]::Create($destination)
        try { $inputStream.CopyTo($outputStream) }
        finally {
            $outputStream.Dispose()
            $inputStream.Dispose()
        }
    }
}
finally { $zip.Dispose() }

$exe = Join-Path $release 'Swiftwing NAS.exe'
$core = Join-Path $release 'Microsoft.Web.WebView2.Core.dll'
$forms = Join-Path $release 'Microsoft.Web.WebView2.WinForms.dll'
$source = Join-Path $PSScriptRoot 'Program.cs'
$manifest = Join-Path $PSScriptRoot 'app.manifest'
$icon = Join-Path (Split-Path -Parent $PSScriptRoot) 'Assets\swiftwing.ico'

$compilerArgs = @(
    '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
    "/out:$exe", "/win32manifest:$manifest",
    '/reference:System.dll', '/reference:System.Core.dll',
    '/reference:System.Drawing.dll', '/reference:System.Windows.Forms.dll',
    "/reference:$core", "/reference:$forms", $source
)
if (Test-Path -LiteralPath $icon) {
    $compilerArgs += "/win32icon:$icon"
}

& $compiler @compilerArgs
if ($LASTEXITCODE -ne 0) { throw "C# compilation failed with exit code $LASTEXITCODE" }

Write-Host "Built: $exe"
Get-Item -LiteralPath $exe | Select-Object FullName, Length, LastWriteTime
