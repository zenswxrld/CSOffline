[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GameDir
)

$ErrorActionPreference = 'Stop'
$game = (Resolve-Path -LiteralPath $GameDir).Path
if (-not (Test-Path -LiteralPath (Join-Path $game 'csgo') -PathType Container)) {
    throw 'GameDir must be the CS:GO installation folder containing the csgo subfolder.'
}

$icons = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'crate_csgogc_*.png' -File)
if ($icons.Count -ne 44) {
    throw "Expected all 44 case icons beside this script; found $($icons.Count). Extract the full case_icons folder first."
}
$destination = Join-Path $game 'csgo\resource\flash\econ\weapon_cases'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach ($icon in $icons) {
    Copy-Item -LiteralPath $icon.FullName -Destination (Join-Path $destination $icon.Name) -Force
}
Write-Host "Installed $($icons.Count) custom case icons in $destination"
Write-Host 'Fully close and restart CS:GO to refresh the cached item pictures.'
