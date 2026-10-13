# Prueba sin privilegios elevados. No habilita caracteristicas ni modifica Windows.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'nativevm.exe'
if (!(Test-Path $exe)) { throw "Falta nativevm.exe en $root" }
Write-Host "=== PortableVM Native - diagnostico ==="
Write-Host "Equipo Windows x64 con Windows Hypervisor Platform habilitado requerido."
& $exe doctor
$doctor = $LASTEXITCODE
Write-Host ("Resultado doctor: " + $doctor)
& $exe list
& $exe validate alpine
$validate = $LASTEXITCODE
Write-Host ("Resultado validate: " + $validate)
if ($doctor -eq 0) {
    Write-Host "=== Prueba CPU y UART WHPX ==="
    & $exe smoke
    Write-Host ("Resultado smoke: " + $LASTEXITCODE)
    if ($LASTEXITCODE -eq 0) {
        Write-Host "CPU WHPX OK. Para probar el Linux experimental:"
        Write-Host ".\nativevm.exe run alpine --ram 256 --timeout 120"
    }
} else {
    Write-Host "WHPX no esta listo. Activar Windows Hypervisor Platform puede requerir reinicio."
}
Write-Host "Comparte unicamente los mensajes de PortableVM (sin datos personales)."
