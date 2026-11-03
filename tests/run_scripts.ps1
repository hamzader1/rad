# Runs rad's scripted tests (script language: src/rad/rad_script.h).
#
#   powershell -ExecutionPolicy Bypass -File tests\run_scripts.ps1 [-Exe build\rad.exe] [-Filter name] [-TimeoutSec 120]
#
# Each script runs on a fresh copy of tests\ in %TEMP%\rad_script_tests\<name>,
# so scripts may edit and save fixture files. rad opens its window without
# taking focus and ignores real keyboard/mouse input while a script runs, so
# tests can run while you work. Exit code = number of failed scripts.

param([string]$Exe = "", [string]$Filter = "*", [int]$TimeoutSec = 120)
$ErrorActionPreference = 'Stop'

$tests = $PSScriptRoot
if ($Exe -eq "") { $Exe = Join-Path $tests "..\build\rad.exe" }
$Exe = (Resolve-Path $Exe).Path
$work = Join-Path $env:TEMP "rad_script_tests"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory $work | Out-Null

$failed = 0
$scripts = @(Get-ChildItem (Join-Path $tests "scripts") -Filter "$Filter.radtest" | Sort-Object Name)
foreach ($script in $scripts) {
  # fresh copy of the fixtures and scripts for this script
  $dir = Join-Path $work $script.BaseName
  New-Item -ItemType Directory $dir | Out-Null
  Copy-Item -Recurse (Join-Path $tests "fixtures") $dir
  Copy-Item -Recurse (Join-Path $tests "scripts") $dir
  $results = Join-Path $dir "results.txt"
  $argline = "--script:`"$dir\scripts\$($script.Name)`" --script_out:`"$results`" --logs:`"$dir\logs`""
  # optional first line "#! args: <more args>"; {work} = the shared work folder
  # (scripts run in name order, so e.g. session_1 can leave a file for session_2)
  $first = Get-Content $script.FullName -TotalCount 1
  if ($first -match '^#!\s*args:\s*(.*)$') { $argline += " " + $Matches[1].Replace("{work}", $work) }

  # run, with a time limit
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $p = Start-Process -FilePath $Exe -ArgumentList $argline -PassThru
  $exited = $p.WaitForExit($TimeoutSec * 1000)
  if (-not $exited) { Stop-Process -Id $p.Id -Force }
  $seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)

  # report
  $lines = @(if (Test-Path $results) { Get-Content $results } else { @() })
  $done = $lines | Where-Object { $_ -like 'DONE *' } | Select-Object -Last 1
  $ok = $exited -and ($done -match '^DONE pass \d+ fail 0 error 0$')
  $status = if ($ok) { "OK  " } else { "FAIL" }
  $summary = if ($done) { $done.Substring(5) } else { "no DONE line" }
  if (-not $exited) { $summary += ", did not exit within $TimeoutSec s (stopped)" }
  Write-Output ("{0} {1} ({2}, {3} s)" -f $status, $script.BaseName, $summary, $seconds)
  foreach ($line in $lines) {
    if ($line -like 'FAIL *' -or $line -like 'ERROR *' -or $line -like 'MEASURE *' -or $line -like 'PRINT *') {
      Write-Output "     $line"
    }
  }
  if (-not $ok) { $failed += 1 }
}

if ($scripts.Count -eq 0) { Write-Output "no scripts match '$Filter'"; $failed = 1 }
Write-Output ("{0} of {1} scripts passed. Results: {2}" -f ($scripts.Count - $failed), $scripts.Count, $work)
exit $failed
