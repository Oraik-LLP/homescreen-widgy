param([switch]$Inno)
$ErrorActionPreference = 'Stop'
$toolsRoot = $PSScriptRoot
$archive = Join-Path $toolsRoot 'toolchain.zip'
$name = 'llvm-mingw-20261104-ucrt-x86_64'
if (!(Test-Path (Join-Path $toolsRoot "$name/bin/clang++.exe"))) {
    Invoke-WebRequest "https://github.com/mstorsjo/llvm-mingw/releases/download/20261104/$name.zip" -OutFile $archive
    $hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne '552191b5754c49a5a20723276a6fc56541ea1922c4c50b7b669022333fc1b40f') { throw 'Toolchain checksum mismatch' }
    Expand-Archive -LiteralPath $archive -DestinationPath $toolsRoot -Force
}
if ($Inno -and !(Test-Path (Join-Path $toolsRoot 'InnoSetup/ISCC.exe'))) {
    $setupPath = Join-Path $toolsRoot 'innosetup.exe'
    Invoke-WebRequest 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_4/innosetup-6.7.4.exe' -OutFile $setupPath
    $signature = Get-AuthenticodeSignature $setupPath
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Pyrsys B.V.') { throw 'Inno Setup publisher verification failed' }
    $destination = Join-Path $toolsRoot 'InnoSetup'
    $setupArgs = @('/VERYSILENT','/SUPPRESSMSGBOXES','/SP-','/NORESTART','/CURRENTUSER','/NOICONS',"/DIR=`"$destination`"")
    $process = Start-Process -FilePath $setupPath -ArgumentList $setupArgs -PassThru -Wait -WindowStyle Hidden
    if ($process.ExitCode) { throw "Inno Setup failed: $($process.ExitCode)" }
}
Write-Host 'Build tools ready. Run ../build.ps1 -Package.'
