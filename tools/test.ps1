param([string]$PythonPath='D:/Python314/python.exe',[string]$MsysRoot='D:/msys64')
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build'
$taskTemp=Join-Path $taskBuild 'test-temp'
New-Item -ItemType Directory -Path $taskTemp -Force | Out-Null
$taskOldPath,$taskOldTemp,$taskOldTmp=$env:PATH,$env:TEMP,$env:TMP
try {
    $env:PATH=(Join-Path $MsysRoot 'mingw64/bin')+';'+$env:PATH
    $env:TEMP=$env:TMP=$taskTemp
    & $PythonPath -B -m unittest discover -s (Join-Path $taskProject 'tests') -v
    if($LASTEXITCODE){throw 'Python regressions failed.'}
    foreach($taskTest in @('cabinet_io','input_policy','game_state','aim_policy','raster_policy','settings_io','launch_policy','audio_probe','uart_clock_policy','adaptive_audio_bridge','capture','trace_commit','presentation_policy','frame_cadence','diagnostic_capture','quest_foveation')) {
        $taskExe=Join-Path $taskBuild ($taskTest+'_test.exe')
        & (Join-Path $MsysRoot 'mingw64/bin/g++.exe') -std=c++20 -Wall -Wextra -I (Join-Path $taskProject 'vendor/glm') -I (Join-Path $taskProject 'vendor/openxr/include') (Join-Path $taskProject ('tests/'+$taskTest+'_test.cpp')) -o $taskExe
        if($LASTEXITCODE){throw "Cannot compile $taskTest"}
        if($taskTest -in @('trace_commit','diagnostic_capture')){& $taskExe $taskTemp}else{& $taskExe}
        if($LASTEXITCODE){throw "Native regression failed: $taskTest"}
    }
} finally {$env:PATH=$taskOldPath;$env:TEMP=$taskOldTemp;$env:TMP=$taskOldTmp}
