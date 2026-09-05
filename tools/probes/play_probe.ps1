# Boot the editor into a project under validation, auto-enter play mode (--play),
# let it run, then kill and summarize. Usage: pwsh -File _play_probe.ps1 [seconds]
param([int]$RunSeconds = 60)
$root = 'D:\GitHub\enjin'
$log  = Join-Path $root 'vk_validation.log'
if (Test-Path $log) { Clear-Content $log }
$env:VK_ADD_LAYER_PATH       = 'C:\VulkanSDK\1.4.335.0\Bin'
$env:VK_LOADER_LAYERS_ENABLE = '*validation*'
$env:VK_INSTANCE_LAYERS      = 'VK_LAYER_KHRONOS_validation'
$env:VK_LAYER_SETTINGS_PATH  = Join-Path $root 'vk_layer_settings.txt'
$p = Start-Process -FilePath (Join-Path $root 'build\bin\Release\EnjinEditor.exe') -ArgumentList 'D:\TEGE_Projects\Shells\Shells.enjinproject','--play' -WorkingDirectory (Join-Path $root 'build\bin\Release') -PassThru
Start-Sleep -Seconds $RunSeconds
if ($p.HasExited) { Write-Host "EXITED code=$($p.ExitCode)" } else { Write-Host "alive after ${RunSeconds}s, killing"; Stop-Process -Id $p.Id -Force }
Start-Sleep -Seconds 2
$v = Get-Content $log -ErrorAction SilentlyContinue
Write-Host "validation lines: $($v.Count)"
Write-Host "errors: $(($v | Select-String 'Validation Error').Count)"
Write-Host "invalidations: $(($v | Select-String 'was destroy or updated').Count)"
$el = Get-Content (Join-Path $root 'build\bin\Release\enjin.log') -ErrorAction SilentlyContinue | Select-String 'auto-entered play mode'
Write-Host "play-mode entered: $(if ($el) { 'YES' } else { 'NO' })"
