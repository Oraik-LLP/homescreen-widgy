$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $projectRoot 'dist/panes.exe'
$runTag = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$firstRoot = Join-Path $projectRoot "test-output/instance-$runTag/primary"
$secondRoot = Join-Path $projectRoot "test-output/instance-$runTag/secondary"
$first = Start-Process -FilePath $exe -ArgumentList @('--diagnostic','--duration=12','--test-root',"`"$firstRoot`"") -PassThru -WindowStyle Hidden
try {
    $ready = $false
    for ($attempt=0; $attempt -lt 50; $attempt++) {
        if (Test-Path (Join-Path $firstRoot 'settings.json')) { $ready=$true; break }
        Start-Sleep -Milliseconds 100
    }
    if (!$ready) { throw 'First instance did not initialize' }
    $second = Start-Process -FilePath $exe -ArgumentList @('--diagnostic','--duration=5','--test-root',"`"$secondRoot`"") -PassThru -Wait -WindowStyle Hidden
    if ($second.ExitCode -ne 2) { throw "Second presenter was not rejected: $($second.ExitCode)" }
    if (!(Test-Path (Join-Path $secondRoot 'instance-conflict.txt'))) { throw 'Missing conflict explanation' }
    $first.WaitForExit()
    if ($first.ExitCode) { throw 'First presenter failed' }
    @{passed=$true;secondInstanceExitCode=$second.ExitCode;message='Only one visible widget set is allowed, including across isolated test roots.'} | ConvertTo-Json | Set-Content (Join-Path $firstRoot 'instance-tests.json')
    Write-Host 'Duplicate presenter rejection passed'
} finally {
    if (!$first.HasExited) {
        $close = Start-Process -FilePath $exe -ArgumentList @('--quit','--test-root',"`"$firstRoot`"") -Wait -PassThru -WindowStyle Hidden
        $first.WaitForExit(5000) | Out-Null
    }
}
