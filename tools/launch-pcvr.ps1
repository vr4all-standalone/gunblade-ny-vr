param([string]$RomDirectory = (Join-Path $PSScriptRoot 'roms'))
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!(Test-Path -LiteralPath (Join-Path $taskRoot 'gunblade.exe'))) {$taskRoot=Split-Path -Parent $PSScriptRoot}
$taskRom=[IO.Path]::GetFullPath($RomDirectory)
foreach($taskFile in @('gunblade.zip')) {
    if(!(Test-Path -LiteralPath (Join-Path $taskRom $taskFile) -PathType Leaf)) {throw "Missing owned game file: $taskRom\$taskFile"}
}
Push-Location -LiteralPath $taskRoot
try {
    & (Join-Path $taskRoot 'gunblade.exe') -rompath $taskRom -verifyroms gunblade
    if($LASTEXITCODE){throw 'MAME rejected the game data. Use the original Gunblade NY Revision A arcade set plus segabill.zip billboard firmware, or one complete 18-ROM gunblade.zip. No CHD is used.'}
    $taskData=Join-Path $taskRoot 'userdata'
    foreach($taskDirectory in @('nvram','cfg','diff','state','snap')) {New-Item -ItemType Directory -Path (Join-Path $taskData $taskDirectory) -Force | Out-Null}
    $env:GUNBLADE_VR='1'
    $env:GUNBLADE_XR_LIBRARY=Join-Path $taskRoot 'gunblade_xr.dll'
    $env:GUNBLADE_ASSETS=Join-Path $taskRoot 'assets'
    $env:GUNBLADE_CUSTOM_GUN=Join-Path $taskRoot 'models/guns/custom.glb'
    $env:GUNBLADE_SETTINGS=Join-Path $taskData 'settings.ini'
    $env:GUNBLADE_XR_LOG=Join-Path $taskData 'gunblade-xr.log'
    & (Join-Path $taskRoot 'gunblade.exe') gunblade -rompath $taskRom -nvram_directory (Join-Path $taskData 'nvram') `
        -cfg_directory (Join-Path $taskData 'cfg') -diff_directory (Join-Path $taskData 'diff') `
        -state_directory (Join-Path $taskData 'state') -snapshot_directory (Join-Path $taskData 'snap') `
        -video none -sound auto -skip_gameinfo -noreadconfig
    $taskPlayExit=$LASTEXITCODE
    if(Test-Path -LiteralPath $env:GUNBLADE_XR_LOG) {Get-Content -LiteralPath $env:GUNBLADE_XR_LOG -Tail 15}
    if($taskPlayExit){throw "Gunblade NY session exited with code $taskPlayExit."}
} finally {Pop-Location}
