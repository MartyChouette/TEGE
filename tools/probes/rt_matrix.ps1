# RT/PT capability matrix: run the RT probe scene through every ray-tracing
# variant, capturing a golden image + a validation log per capability.
#
# First time: emit the probe project, then run this. Each variant's PNG lands
# in goldens\rt\_candidates for a one-time visual blessing; -Record copies the
# blessed captures to goldens\rt as references, after which plain runs are an
# automated RT regression matrix.
#
#   $env:ENJIN_RT_PROBE_DIR = 'D:\TEGE_Projects\_RTProbe'
#   ctest --test-dir build -C Release -R TestRTSceneEmit
#   pwsh -File _rt_matrix.ps1            -> capture + validate all variants
#   pwsh -File _rt_matrix.ps1 -Record    -> bless current captures as references
#   pwsh -File _rt_matrix.ps1 -Only rt_shadows,pathtracer_raw
#
# NOTE: do not run while an interactive editor has the RT probe project open.
param(
    [switch]$Record,
    [string]$Only = "",
    [int]$Frames = 300,
    [int]$TimeoutSec = 120
)
$root = 'D:\GitHub\enjin'
$exe  = Join-Path $root 'build\bin\Release\EnjinEditor.exe'
$proj = 'D:\TEGE_Projects\_RTProbe\RTProbe.enjinproject'
$refDir = Join-Path $root 'goldens\rt'
$outDir = Join-Path $root 'goldens\rt\_candidates'
New-Item -ItemType Directory -Force $refDir | Out-Null
New-Item -ItemType Directory -Force $outDir | Out-Null

if (-not (Test-Path $proj)) {
    Write-Host "RT probe project missing. Emit it first:"
    Write-Host "  `$env:ENJIN_RT_PROBE_DIR = 'D:\TEGE_Projects\_RTProbe'"
    Write-Host "  ctest --test-dir build -C Release -R TestRTSceneEmit"
    exit 2
}

$variants = (python (Join-Path $root '_rt_variants.py') --list) -split "`n" | Where-Object { $_ }
if ($Only) { $variants = $Only -split ',' | ForEach-Object { $_.Trim() } }

# Validation layer env (same as the standard probes)
$env:VK_ADD_LAYER_PATH       = 'C:\VulkanSDK\1.4.335.0\Bin'
$env:VK_LOADER_LAYERS_ENABLE = '*validation*'
$env:VK_INSTANCE_LAYERS      = 'VK_LAYER_KHRONOS_validation'
$env:VK_LAYER_SETTINGS_PATH  = Join-Path $root 'vk_layer_settings.txt'
$vlog = Join-Path $root 'vk_validation.log'

$rows = @()
foreach ($v in $variants) {
    python (Join-Path $root '_rt_variants.py') $v | Out-Null
    if (Test-Path $vlog) { Clear-Content $vlog }
    $base = Join-Path $outDir $v
    Remove-Item "$base.png", "$base.ppm" -ErrorAction SilentlyContinue

    Write-Host "[$v] rendering..." -NoNewline
    $p = Start-Process -FilePath $exe -ArgumentList $proj,'--golden',$base,'--golden-frames',$Frames -WorkingDirectory (Join-Path $root 'build\bin\Release') -PassThru
    $exited = $p.WaitForExit($TimeoutSec * 1000)
    if (-not $exited) { Stop-Process -Id $p.Id -Force }

    $vl = Get-Content $vlog -ErrorAction SilentlyContinue
    $errs = ($vl | Select-String 'Validation Error').Count
    $captured = Test-Path "$base.ppm"
    $status = if (-not $exited) { "TIMEOUT" }
              elseif (-not $captured) { "NO CAPTURE" }
              elseif ($errs -gt 0) { "CAPTURED, $errs validation errors" }
              else { "OK" }
    Write-Host " $status"
    $rows += [pscustomobject]@{ Variant = $v; Captured = $captured; ValidationErrors = $errs; Status = $status }

    if ($Record -and $captured) {
        Copy-Item "$base.ppm" (Join-Path $refDir "$v.ppm") -Force
        Copy-Item "$base.png" (Join-Path $refDir "$v.png") -Force
    } elseif ($captured -and (Test-Path (Join-Path $refDir "$v.ppm"))) {
        python (Join-Path $root '_golden_compare.py') (Join-Path $refDir "$v.ppm") "$base.ppm" 2.0 10
    }
}

# Restore the scene to the raster control so the project isn't left in a
# random variant state
python (Join-Path $root '_rt_variants.py') raster_base | Out-Null

Write-Host ""
$rows | Format-Table -AutoSize
if ($Record) { Write-Host "References blessed into $refDir" }
Write-Host "Captures for eyeballing: $outDir"
