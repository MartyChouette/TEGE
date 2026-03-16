$staging = "D:\GitHub\enjin\installer\output\TEGE-0.9.0"
$zip = "D:\GitHub\enjin\installer\output\TEGE-0.9.0.zip"

if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }

New-Item -ItemType Directory -Path "$staging\bin" -Force | Out-Null
New-Item -ItemType Directory -Path "$staging\shaders" -Force | Out-Null
New-Item -ItemType Directory -Path "$staging\scripts" -Force | Out-Null

Copy-Item "D:\GitHub\enjin\build\bin\Release\EnjinEditor.exe" "$staging\bin\"
Copy-Item "D:\GitHub\enjin\build\bin\Release\EnjinPlayer.exe" "$staging\bin\"
Copy-Item "D:\GitHub\enjin\Engine\shaders\*.spv" "$staging\shaders\"
Copy-Item "D:\GitHub\enjin\build\bin\Release\scripts\*" "$staging\scripts\"
Copy-Item "D:\GitHub\enjin\installer\enjin.ico" "$staging\"
Copy-Item "D:\GitHub\enjin\LICENSE" "$staging\"

Compress-Archive -Path "$staging\*" -DestinationPath $zip

Write-Host "Contents:"
Get-ChildItem $staging -Recurse | ForEach-Object { Write-Host $_.FullName }
Write-Host ""
$size = (Get-Item $zip).Length / 1MB
Write-Host "Zip size: $([math]::Round($size, 1)) MB"
Write-Host "Zip path: $zip"
