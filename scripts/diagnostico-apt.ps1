# Pruebas no destructivas para PortableBox. PowerShell 5.1+.
[CmdletBinding()]
param(
    [string]$PortableBoxPath = (Split-Path -Parent $PSScriptRoot),
    [switch]$SyncDns
)
$ErrorActionPreference = 'Stop'
$base = (Resolve-Path -LiteralPath $PortableBoxPath).Path
$rootfs = Join-Path $base 'distros\debian-12'
$blink = Join-Path $base 'blink.exe'
if (!(Test-Path $blink) -or !(Test-Path (Join-Path $rootfs 'usr\bin\apt-get'))) {
    throw 'No se encuentra blink.exe o el rootfs Debian; usa -PortableBoxPath.'
}
Write-Host '=== DNS del rootfs ==='
$resolv = Join-Path $rootfs 'etc\resolv.conf'
if (Test-Path $resolv) { Get-Content $resolv } else { Write-Warning 'No existe resolv.conf' }
if ($SyncDns) {
    $servers = @(Get-DnsClientServerAddress -AddressFamily IPv4 |
        ForEach-Object { $_.ServerAddresses } |
        Where-Object { $_ -and $_ -ne '0.0.0.0' } |
        Select-Object -Unique -First 3)
    if ($servers.Count -eq 0) { throw 'Windows no tiene servidores DNS IPv4 configurados.' }
    $backup = "$resolv.portablebox.bak"
    if ((Test-Path $resolv) -and !(Test-Path $backup)) {
        Copy-Item -LiteralPath $resolv -Destination $backup
    }
    $entries = $servers | ForEach-Object { "nameserver $_" }
    [IO.File]::WriteAllText($resolv, (($entries -join "`n") + "`n"), [Text.Encoding]::ASCII)
    Write-Host "DNS del invitado sincronizado: $($servers -join ', ')"
    Write-Host "Copia del archivo anterior (si existia): $backup"
}
Write-Host '=== DNS y TCP del anfitrion ==='
try {
    $ips = @([Net.Dns]::GetHostAddresses('deb.debian.org') | Where-Object AddressFamily -eq InterNetwork)
} catch {
    Write-Warning ('DNS de Windows: ' + $_.Exception.Message)
    $ips = @()
}
Write-Host ('DNS de Windows: ' + ($ips -join ', '))
if ($ips.Count -gt 0) {
    $tcp = New-Object Net.Sockets.TcpClient
    try {
        $pending = $tcp.BeginConnect($ips[0], 80, $null, $null)
        if ($pending.AsyncWaitHandle.WaitOne(5000)) {
            $tcp.EndConnect($pending)
            Write-Host 'TCP puerto 80: conectado'
        } else { Write-Warning 'TCP puerto 80: timeout' }
    } catch {
        Write-Warning ('TCP de Windows: ' + $_.Exception.Message)
    } finally { $tcp.Close() }
}
$logs = Join-Path $env:TEMP 'portablebox-apt-diagnostico'
New-Item -Force -ItemType Directory $logs | Out-Null
Write-Host "Registros: $logs"
function Test-Guest([string]$stage, [string]$guestArgs, [int]$seconds) {
    $stdout = Join-Path $logs "$stage.stdout.log"
    $stderr = Join-Path $logs "$stage.stderr.log"
    Write-Host "=== $stage ($seconds segundos maximo) ==="
    $priorPath = $env:PATH
    $priorHome = $env:HOME
    $priorLang = $env:LANG
    $priorLc = $env:LC_ALL
    try {
        $env:PATH = '/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
        $env:HOME = '/root'
        $env:LANG = 'C'
        $env:LC_ALL = 'C'
        $proc = Start-Process -FilePath $blink -WorkingDirectory $rootfs `
            -ArgumentList "-s -C . $guestArgs" -PassThru -NoNewWindow `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    } finally {
        $env:PATH = $priorPath
        $env:HOME = $priorHome
        $env:LANG = $priorLang
        $env:LC_ALL = $priorLc
    }
    try {
        if (-not $proc.WaitForExit($seconds * 1000)) {
            Write-Warning "${stage}: timeout; se interrumpira este proceso de prueba."
            & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $proc.Id /T /F | Out-Null
        } else { Write-Host "Codigo de salida: $($proc.ExitCode)" }
    } finally { $proc.Dispose() }
    Write-Host 'Salida (ultimas 15 lineas):'
    if (Test-Path $stdout) { Get-Content $stdout -Tail 15 }
    Write-Host 'Syscalls y errores (ultimas 35 lineas):'
    if (Test-Path $stderr) { Get-Content $stderr -Tail 35 }
}
Test-Guest 'apt-version' '/usr/bin/apt-get --version' 12
Test-Guest 'getent' '/usr/bin/getent ahostsv4 deb.debian.org' 15
Test-Guest 'apt-update' '/usr/bin/apt-get -o Acquire::Retries=0 -o Acquire::http::Timeout=8 -o Acquire::https::Timeout=8 update' 40
Write-Host 'El diagnostico ha terminado. Revisa los registros antes de compartirlos.'
