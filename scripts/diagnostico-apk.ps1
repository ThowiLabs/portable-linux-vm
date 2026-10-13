# Diagnostico de APK HTTPS/HTTP en Blink para Windows, sin modificar el host.
[CmdletBinding()]
param(
    [string]$PortableBoxPath = (Split-Path -Parent $PSScriptRoot),
    [ValidateRange(5, 90)][int]$TimeoutSeconds = 25
)
$ErrorActionPreference = 'Stop'
$base = (Resolve-Path -LiteralPath $PortableBoxPath).Path
$rootfs = Join-Path $base 'distros\alpine-3.24'
$blink = Join-Path $base 'blink.exe'
$repoFile = Join-Path $rootfs 'etc\apk\repositories'
foreach ($file in @($blink, $repoFile, (Join-Path $rootfs 'sbin\apk'))) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Falta archivo: $file"
    }
}
$logs = Join-Path $env:TEMP 'portablebox-apk-diagnostico'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
Write-Host "Registros: $logs"
Write-Host 'Verificando conectividad HTTPS nativa de Windows...'
try {
    $uri = 'https://dl-cdn.alpinelinux.org/alpine/v3.24/main/x86_64/APKINDEX.tar.gz'
    $response = Invoke-WebRequest -Uri $uri -Method Head -UseBasicParsing -TimeoutSec 12
    Write-Host "HTTPS Windows: $($response.StatusCode)"
} catch {
    Write-Warning ('HTTPS Windows: ' + $_.Exception.Message)
}

function Test-Apk([string]$scheme) {
    $stdout = Join-Path $logs "apk-$scheme.stdout.log"
    $stderr = Join-Path $logs "apk-$scheme.stderr.log"
    foreach ($file in @($stdout, $stderr)) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
    }
    $previousPath = $env:PATH
    $previousHome = $env:HOME
    $previousLang = $env:LANG
    $previousLc = $env:LC_ALL
    $proc = $null
    try {
        $env:PATH = '/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
        $env:HOME = '/root'
        $env:LANG = 'C'
        $env:LC_ALL = 'C'
        $proc = Start-Process -FilePath $blink -WorkingDirectory $rootfs `
            -ArgumentList '-s -C . /sbin/apk update' -NoNewWindow -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    } finally {
        $env:PATH = $previousPath
        $env:HOME = $previousHome
        $env:LANG = $previousLang
        $env:LC_ALL = $previousLc
    }
    if ($null -eq $proc) { throw "No arranco apk: $scheme" }
    try {
        if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
            Write-Warning "APK $scheme no termino en $TimeoutSeconds segundos."
            & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $proc.Id /T /F | Out-Null
        } else {
            Write-Host "APK $scheme finalizo con codigo: $($proc.ExitCode)"
        }
    } finally { $proc.Dispose() }
    Write-Host "=== $scheme: ultimas lineas de salida ==="
    if (Test-Path $stdout) { Get-Content -LiteralPath $stdout -Tail 15 }
    Write-Host "=== $scheme: ultimas syscalls/errores ==="
    if (Test-Path $stderr) { Get-Content -LiteralPath $stderr -Tail 30 }
}

# Se alteran las fuentes SOLO durante el diagnostico; se restauran los bytes originales.
# Ejecutar sin otras instancias de Alpine activas para evitar carreras.
$originalBytes = [IO.File]::ReadAllBytes($repoFile)
$original = [Text.Encoding]::UTF8.GetString($originalBytes)
$backup = "$repoFile.portablebox-apk.bak"
if (-not (Test-Path -LiteralPath $backup)) {
    [IO.File]::WriteAllBytes($backup, $originalBytes)
}
try {
    foreach ($scheme in @('https', 'http')) {
        Write-Host "=== Probando $scheme dentro de Blink ==="
        $revised = [regex]::Replace($original, 'https?://', "$scheme" + '://')
        [IO.File]::WriteAllText($repoFile, $revised, [Text.Encoding]::UTF8)
        Test-Apk $scheme
    }
} finally {
    [IO.File]::WriteAllBytes($repoFile, $originalBytes)
    Write-Host 'Fuentes APK originales restauradas.'
}
Write-Host 'Revisa y comparte solo los errores relevantes de los registros; pueden incluir rutas privadas.'
