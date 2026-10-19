# SkinStop custom case icons

This folder contains all **44 individual transparent PNGs**, under the existing
`crate_csgogc_*.png` filenames. Each is 256 × 192 pixels. The approved yellow
CS:GO-style cases use smaller name-based emblems; Black Market and Gauntlet use
the approved charcoal shell with an orange side stripe.

## Install the pictures

Close the game and download this whole folder, including its PNGs and installer.
From this folder, run PowerShell with your actual game installation path:

```powershell
powershell -ExecutionPolicy Bypass -File .\install_case_icons.ps1 -GameDir "G:\Games\Counter-Strike Global Offensive"
```

Alternatively, copy all 44 `crate_csgogc_*.png` files into:

```text
<game folder>\csgo\resource\flash\econ\weapon_cases\
```

Accept replacement of the previous custom case pictures and restart the game.
The filenames already match the schema, so no custom case definitions need to
be changed. These are container icons, not skin thumbnails for the opening reel.

The larger/bolder UI text and native-agent lineup are code changes in the same
pull request: rebuild and install `csgo_gc` to apply those. Installing only these
PNGs updates the case pictures without rebuilding.
