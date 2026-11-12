param([int]$Seconds=305, [switch]$AllWidgets, [switch]$MatchWallpaper)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $projectRoot 'dist/panes.exe'
if($Seconds -lt 10){throw 'Use at least 10 seconds to include samples after warmup.'}
$testRoot=Join-Path $projectRoot ('test-output/diagnostics-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if ($AllWidgets -or $MatchWallpaper) {
    New-Item -ItemType Directory -Force $testRoot | Out-Null
    $widgetSettings = @(0..6 | ForEach-Object { @{id=$_; visible=($_ -lt 3 -or $AllWidgets.IsPresent)} })
    @{version=2; widgets=$widgetSettings; wallpaperMatch=@($MatchWallpaper.IsPresent,$false,$false,$false,$false)} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $testRoot 'settings.json')
}
$appArgs=@('--diagnostic','--export-previews',"--duration=$Seconds",'--test-root',"`"$testRoot`"")
$process=Start-Process -FilePath $exe -ArgumentList $appArgs -Wait -PassThru -WindowStyle Hidden
if($process.ExitCode -eq 2){throw 'A widget instance is already visible. Exit it from its tray menu before testing.'}
if($process.ExitCode){throw "Diagnostic app failed: $($process.ExitCode)"}
$report=Get-Content (Join-Path $testRoot 'diagnostics.json') -Raw | ConvertFrom-Json
$report | Format-List elapsedSeconds,averageTotalCpuPercent,peakPrivateMiB,peakWorkingSetMiB,desktopHostFound
Write-Host "Full report and preview BMPs: $testRoot"
if(!$report.desktopHostFound -or $report.warning){throw 'Desktop integration reported a problem.'}
$memoryTarget = if ($AllWidgets) { 60 } else { 40 }
if($report.averageTotalCpuPercent -ge 0.5 -or $report.peakPrivateMiB -ge $memoryTarget){throw 'Resource targets exceeded.'}
