# Golden-image regression harness.
# The editor renders each scene, captures the game view after N frames
# (--golden), writes PNG+PPM, and exits; this script compares against the
# recorded references in goldens/.
#
# Usage: pwsh -File _golden.ps1            -> compare all scenes vs goldens/
#        pwsh -File _golden.ps1 -Record    -> (re)record the references
#
# NOTES:
# - Do NOT run while an interactive editor has one of these projects open
#   (the editor auto-saves the open scene on a timer).
# - Goldens are RESOLUTION-DEPENDENT: they capture the game view panel at
#   whatever size the saved editor layout gives it. Keep the window/layout
#   stable, or re-record after layout changes.
# - Temporal effects (TAA jitter, animated fog/caustics) mean captures are
#   not byte-identical; the comparer allows 1% of pixels to drift by default.
param(
    [switch]$Record,
    [int]$Frames = 240,
    [int]$TimeoutSec = 90
)
$root = 'D:\GitHub\enjin'
$exe  = Join-Path $root 'build\bin\Release\EnjinEditor.exe'
$goldenDir = Join-Path $root 'goldens'
$outDir    = Join-Path $root 'goldens\_candidates'
New-Item -ItemType Directory -Force $goldenDir | Out-Null
New-Item -ItemType Directory -Force $outDir | Out-Null

# Scene list: name, project path, whether to capture in play mode
$scenes = @(
    @{ name = 'shells_edit'; project = 'D:\TEGE_Projects\Shells\Shells.enjinproject'; play = $false },
    @{ name = 'shells_play'; project = 'D:\TEGE_Projects\Shells\Shells.enjinproject'; play = $true }
)

$failed = 0
foreach ($s in $scenes) {
    $base = Join-Path $outDir $s.name
    Remove-Item "$base.png", "$base.ppm" -ErrorAction SilentlyContinue
    $args = @($s.project, '--golden', $base, '--golden-frames', $Frames)
    if ($s.play) { $args += '--play' }

    Write-Host "[$($s.name)] rendering..." -NoNewline
    $p = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory (Join-Path $root 'build\bin\Release') -PassThru
    $exited = $p.WaitForExit($TimeoutSec * 1000)
    if (-not $exited) { Stop-Process -Id $p.Id -Force; Write-Host " TIMEOUT (killed)"; $failed++; continue }
    if (-not (Test-Path "$base.ppm")) { Write-Host " NO CAPTURE"; $failed++; continue }

    if ($Record) {
        Copy-Item "$base.ppm" (Join-Path $goldenDir "$($s.name).ppm") -Force
        Copy-Item "$base.png" (Join-Path $goldenDir "$($s.name).png") -Force
        Write-Host " recorded reference"
    } else {
        $ref = Join-Path $goldenDir "$($s.name).ppm"
        if (-not (Test-Path $ref)) { Write-Host " NO REFERENCE (run -Record first)"; $failed++; continue }
        Write-Host ""
        python (Join-Path $root '_golden_compare.py') $ref "$base.ppm"
        if ($LASTEXITCODE -ne 0) { $failed++ }
    }
}

Write-Host ""
if ($Record) { Write-Host "References recorded to $goldenDir" }
elseif ($failed -eq 0) { Write-Host "ALL GOLDEN CHECKS PASSED" }
else { Write-Host "$failed golden check(s) FAILED - diff the PNGs in goldens\ vs goldens\_candidates\" }
exit $(if ($failed -gt 0) { 1 } else { 0 })
