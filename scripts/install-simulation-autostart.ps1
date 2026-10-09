$ErrorActionPreference = 'Stop'
$taskName = 'AGV Simulation Auto Start'
$oldHealthTask = 'AGV Simulation Health Check'
$scriptFile = Join-Path $PSScriptRoot 'keep-simulation-ready.ps1'
$projectRoot = Split-Path -Parent $PSScriptRoot
$powershellFile = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$account = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$action = New-ScheduledTaskAction -Execute $powershellFile `
    -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $scriptFile + '" -Once') `
    -WorkingDirectory $projectRoot
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $account
$principal = New-ScheduledTaskPrincipal -UserId $account -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -MultipleInstances IgnoreNew `
    -ExecutionTimeLimit (New-TimeSpan -Minutes 2)
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger `
    -Principal $principal -Settings $settings `
    -Description 'Chỉ mở điểm truy cập web mô phỏng; Core, broker và hai xe chạy khi có tab xem.' -Force | Out-Null
$old = Get-ScheduledTask -TaskName $oldHealthTask -ErrorAction SilentlyContinue
if ($old) { Unregister-ScheduledTask -TaskName $oldHealthTask -Confirm:$false }
& $scriptFile -Once
Write-Output "Đã cập nhật $taskName; bỏ tác vụ tự mở lại mô phỏng."
