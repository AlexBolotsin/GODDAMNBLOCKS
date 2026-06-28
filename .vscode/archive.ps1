param(
    [string]$WorkspaceFolder
)

$date = (Get-Date).ToString('yyyyMMdd')
$name = "MazeGame-$date"
$out  = "$WorkspaceFolder\dist\$name"

New-Item -ItemType Directory -Force $out | Out-Null

Copy-Item "$WorkspaceFolder\build\Release\MazeGame.exe" $out
Copy-Item "$WorkspaceFolder\build\Release\Shaders"      $out -Recurse
Copy-Item "$WorkspaceFolder\build\Release\Sprites"      $out -Recurse

$zip = "$WorkspaceFolder\dist\$name.zip"
Compress-Archive -Path $out -DestinationPath $zip -Force
Remove-Item $out -Recurse -Force

Write-Host "Archived to $zip"
