# Player RT/PT smoke probe: export the RT probe project (path-tracer variant),
# copy the freshly built EnjinPlayer.exe next to game.enjpak, boot it under
# validation for ~25s, kill it, then verify via logs:
#   - RT initialized + TLAS valid + path tracer dispatching
#   - "Path tracer display active" (the PT image reached the swapchain)
#   - 0 validation errors
#
# Prereqs: probe project emitted (TestRTSceneEmit), EnjinPlayer target built.
$root      = 'D:\GitHub\enjin'
$probeDir  = 'D:\TEGE_Projects\_RTProbe'
$exportDir = Join-Path $probeDir '_export'
$playerSrc = Join-Path $root 'build\bin\Release\EnjinPlayer.exe'

# Stamp the scene to the raw path-tracer variant, then export via the tool test
python (Join-Path $root '_rt_variants.py') pathtracer_raw | Out-Null
$env:ENJIN_RT_PROBE_DIR  = $probeDir
$env:ENJIN_RT_EXPORT_DIR = $exportDir
ctest --test-dir (Join-Path $root 'build') -C Release -R TestRTPlayerExport --output-on-failure | Out-Null
if (-not (Test-Path (Join-Path $exportDir 'game.enjpak'))) { Write-Host 'EXPORT FAILED: no game.enjpak'; exit 2 }

# The pipeline's player lookup can't see build\bin\Release from the test exe dir — copy manually
Copy-Item $playerSrc (Join-Path $exportDir 'EnjinPlayer.exe') -Force

# Boot under validation
$vlog = Join-Path $root 'vk_validation.log'
if (Test-Path $vlog) { Clear-Content $vlog }
$plog = Join-Path $exportDir 'enjin.log'
if (Test-Path $plog) { Remove-Item $plog -Force }
$env:VK_ADD_LAYER_PATH       = 'C:\VulkanSDK\1.4.335.0\Bin'
$env:VK_LOADER_LAYERS_ENABLE = '*validation*'
$env:VK_INSTANCE_LAYERS      = 'VK_LAYER_KHRONOS_validation'
$env:VK_LAYER_SETTINGS_PATH  = Join-Path $root 'vk_layer_settings.txt'

$p = Start-Process -FilePath (Join-Path $exportDir 'EnjinPlayer.exe') -WorkingDirectory $exportDir -PassThru
Start-Sleep -Seconds 25
$crashed = $p.HasExited
if ($crashed) { Write-Host "PLAYER EXITED EARLY code=$($p.ExitCode)" } else { Stop-Process -Id $p.Id -Force }
Start-Sleep -Seconds 2

# Report
$log = Get-Content $plog -ErrorAction SilentlyContinue
$v   = Get-Content $vlog -ErrorAction SilentlyContinue
Write-Host "RT init:        $(($log | Select-String 'Ray tracing subsystems initialized').Count -gt 0)"
Write-Host "TLAS valid:     $(($log | Select-String 'TLAS valid').Count -gt 0)"
Write-Host "PT dispatching: $(($log | Select-String 'Path tracer dispatching').Count -gt 0)"
Write-Host "PT display:     $(($log | Select-String 'Path tracer display active').Count -gt 0)"
Write-Host "Crashed early:  $crashed"
Write-Host "Validation errors: $(($v | Select-String 'Validation Error').Count)"
