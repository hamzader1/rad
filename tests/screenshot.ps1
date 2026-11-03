# Screenshot of rad while it runs a script, without taking focus.
#
#   powershell -ExecutionPolicy Bypass -File tests\screenshot.ps1 -Script <file.radtest> -Out <file.png>
#              [-Exe build\rad.exe] [-AfterMs 3500]
#
# The script should end with `wait <ms>` (longer than -AfterMs) and `quit`, so
# the window is still open when the picture is taken. It runs on a fresh copy
# of tests\ in %TEMP%\rad_screenshot (paths in it are relative to tests\scripts\).
# rad renders every frame while a script runs, so PrintWindow gets current
# pixels even though the window is in the background.

param([Parameter(Mandatory)][string]$Script, [Parameter(Mandatory)][string]$Out,
      [string]$Exe = "", [int]$AfterMs = 3500)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class RadShot {
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Rt, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
[void][RadShot]::SetProcessDPIAware()

$tests = $PSScriptRoot
if ($Exe -eq "") { $Exe = Join-Path $tests "..\build\rad.exe" }
$Exe = (Resolve-Path $Exe).Path
$Script = (Resolve-Path $Script).Path
$Out = [IO.Path]::GetFullPath($Out)
$work = Join-Path $env:TEMP "rad_screenshot"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory $work | Out-Null
Copy-Item -Recurse (Join-Path $tests "fixtures") $work
New-Item -ItemType Directory (Join-Path $work "scripts") | Out-Null
Copy-Item $Script (Join-Path $work "scripts")
$scriptCopy = Join-Path $work ("scripts\" + (Split-Path $Script -Leaf))

$p = Start-Process -FilePath $Exe -ArgumentList "--script:`"$scriptCopy`" --script_out:`"$work\results.txt`" --logs:`"$work\logs`"" -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 100 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100; $p.Refresh(); if ($p.HasExited) { break }; $h = $p.MainWindowHandle }
Start-Sleep -Milliseconds $AfterMs
$p.Refresh()
if ($h -eq [IntPtr]::Zero -or $p.HasExited) { throw "rad exited before the screenshot; end the script with a longer 'wait <ms>'" }
$r = New-Object RadShot+R
[void][RadShot]::GetWindowRect($h, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap ($r.Rt - $r.L), ($r.B - $r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc(); [void][RadShot]::PrintWindow($h, $hdc, 2); $g.ReleaseHdc($hdc)
$bmp.Save($Out); $g.Dispose(); $bmp.Dispose()
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force }
Write-Output "saved $Out ($($r.Rt - $r.L)x$($r.B - $r.T))"
