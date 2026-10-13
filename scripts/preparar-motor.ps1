param(
  [switch]$ConservarInstalador
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $root 'engine'
$installer = Join-Path $env:TEMP 'portablevm-qemu-20260811.exe'
$url = 'https://qemu.weilnetz.de/w64/qemu-w64-setup-20260811.exe'
$expected = '5bcf9eed634e8575a37b74f445af41a2fe4106da512d0c30c368301d4c105037fdfab40a5287367a28a957624cddebbc8c07e16c88ab6634f554cdf3d16bf543'
$seven = Get-Command 7z.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
if (-not $seven) {
  foreach ($p in @("$env:ProgramFiles\7-Zip\7z.exe", "${env:ProgramFiles(x86)}\7-Zip\7z.exe")) {
    if (Test-Path $p) { $seven = $p; break }
  }
}
if (-not $seven) {
  throw 'Falta 7-Zip. Instala 7-Zip desde https://www.7-zip.org/ y vuelve a ejecutar este script. El gestor no instala nada en Windows.'
}
if (-not (Test-Path $installer)) {
  Write-Host 'Descargando QEMU para Windows desde qemu.weilnetz.de (198 MiB)...'
  Invoke-WebRequest -Uri $url -OutFile $installer
}
$actual = (Get-FileHash -Path $installer -Algorithm SHA512).Hash.ToLowerInvariant()
if ($actual -ne $expected) {
  Remove-Item $installer -Force
  throw 'SHA-512 incorrecto. Se ha borrado la descarga y NO se instalara QEMU.'
}
New-Item -ItemType Directory -Force -Path $engine | Out-Null
Write-Host 'Extrayendo QEMU, DLL y firmware a engine (sin instalacion en Windows)...'
& $seven x -y "-o$engine" $installer 'qemu-system-x86_64.exe' 'qemu-img.exe' '*.dll' 'share/*'
if ($LASTEXITCODE -ne 0) { throw 'No se pudo extraer QEMU.' }
foreach ($file in @('qemu-system-x86_64.exe', 'qemu-img.exe')) {
  if (-not (Test-Path (Join-Path $engine $file))) { throw "Falta $file en engine" }
}
& (Join-Path $engine 'qemu-img.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'QEMU no pudo iniciarse. Revisa las DLL extraidas.' }
if (-not $ConservarInstalador) { Remove-Item $installer -Force }
Write-Host 'Motor portable preparado. Puedes copiar toda esta carpeta a una USB.'
