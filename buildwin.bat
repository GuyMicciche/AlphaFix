REM Set paths
set "ROOT=%~dp0"
set "AE_SDK=%ROOT%AfterEffectsSDK"
set "FFMPEG=%ROOT%ffmpeg-shared"

REM Configure
cmake -B Win -S . -G "Visual Studio 17 2022" -A x64 ^
    -DAE_SDK_PATH="%AE_SDK%" ^
    -DFFMPEG_PATH="%FFMPEG%"

REM Build
cmake --build Win --config Release

REM Install (copies to AE plugins folder)
cmake --install Win --config Release

pause