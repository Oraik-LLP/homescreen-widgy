$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $projectRoot 'test-output/installer'
$installRoot = Join-Path $testRoot 'app'
$dataRoot = Join-Path $testRoot 'data'
$setup = Join-Path $projectRoot 'dist/panes-Setup-1.2.1-x64.exe'
New-Item -ItemType Directory -Force $testRoot,$dataRoot | Out-Null
$registryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$prior = (Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue).panes
$results = [System.Collections.Generic.List[object]]::new()
function Assert-Test([bool]$Condition, [string]$Name) {
    $results.Add([pscustomobject]@{name=$Name; passed=$Condition})
    if (!$Condition) { throw $Name }
}
function Invoke-Setup([string]$LogName) {
    $setupArgs = @('/VERYSILENT','/SUPPRESSMSGBOXES','/SP-','/NORESTART','/CURRENTUSER','/NOICONS','/TASKS=',"/DIR=`"$installRoot`"","/LOG=`"$(Join-Path $testRoot $LogName)`"")
    $process = Start-Process -FilePath $setup -ArgumentList $setupArgs -PassThru -Wait -WindowStyle Hidden
    Assert-Test ($process.ExitCode -eq 0) "Installer completed: $LogName"
}
$success = $false
try {
    Invoke-Setup 'install.log'
    $installedExe = Join-Path $installRoot 'panes.exe'
    Assert-Test (Test-Path $installedExe) 'Per-user installer creates executable'
    Assert-Test ((Get-Item $installedExe).VersionInfo.ProductName -eq 'panes') 'Installed executable carries panes branding'
    Assert-Test (Test-Path (Join-Path $installRoot 'panes.ico')) 'Windows icon included in installation'
    Assert-Test (Test-Path (Join-Path $installRoot 'panes.svg')) 'Editable vector icon included in installation'
    $after = (Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue).panes
    Assert-Test ($after -eq $prior) 'Launch at sign-in remains unchanged by default'
    $appArgs = @('--application-test','--test-root',"`"$dataRoot`"")
    $app = Start-Process -FilePath $installedExe -ArgumentList $appArgs -PassThru -Wait -WindowStyle Hidden
    Assert-Test ($app.ExitCode -eq 0) 'Installed executable runs without build tools'
    $appReport = Get-Content (Join-Path $dataRoot 'application-tests.json') -Raw | ConvertFrom-Json
    Assert-Test $appReport.passed 'Installed executable passes app integration checks'
    $settingsPath = Join-Path $dataRoot 'settings.json'
    $beforeHash = (Get-FileHash $settingsPath).Hash
    Invoke-Setup 'upgrade.log'
    Assert-Test ((Get-FileHash $settingsPath).Hash -eq $beforeHash) 'Upgrade preserves saved settings'
    Assert-Test ((Get-FileHash $installedExe).Hash -eq (Get-FileHash (Join-Path $projectRoot 'dist/panes.exe')).Hash) 'Upgrade installs the expected binary'
    # Exercise cleanup of startup enabled AFTER installation, the Settings path.
    New-ItemProperty -LiteralPath $registryPath -Name panes -Value "`"$installedExe`"" -PropertyType String -Force | Out-Null
    $uninstaller = Join-Path $installRoot 'unins000.exe'
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$(Join-Path $testRoot 'uninstall.log')`"") -Wait -PassThru -WindowStyle Hidden
    Assert-Test ($uninstall.ExitCode -eq 0) 'Uninstaller completes without administrator rights'
    Assert-Test (!(Test-Path $installedExe)) 'Uninstall removes app binary'
    $afterUninstall = (Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue).panes
    Assert-Test ($null -eq $afterUninstall) 'Uninstall removes startup enabled after installation'
    Assert-Test ((Get-FileHash $settingsPath).Hash -eq $beforeHash) 'Uninstall preserves saved settings'
    $success = $true
} finally {
    if ($null -ne $prior) {
        New-ItemProperty -LiteralPath $registryPath -Name panes -Value $prior -PropertyType String -Force | Out-Null
    } elseif ((Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue).panes) {
        Remove-ItemProperty -LiteralPath $registryPath -Name panes
    }
    [pscustomobject]@{passed=$success;checks=$results} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $testRoot 'installer-tests.json')
}
Write-Host "$($results.Count) installer checks passed"
