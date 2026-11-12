param([switch]$Package, [string]$CompilerRoot = '', [string]$InnoCompiler = '', [switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
Push-Location $projectRoot
try {
    if (!$CompilerRoot) {
        $found = Get-ChildItem (Join-Path $projectRoot 'tools') -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' | Sort-Object Name -Descending | Select-Object -First 1
        if ($found) { $CompilerRoot = $found.FullName }
    }
    New-Item -ItemType Directory -Force build,dist,'test-output/core' | Out-Null
    if (!(Test-Path assets/app.ico)) { throw 'Missing app icon. Run assets/make_icon.py with Pillow.' }
    if ($CompilerRoot) {
        $clang = Join-Path $CompilerRoot 'bin/x86_64-w64-mingw32-clang++.exe'
        $rc = Join-Path $CompilerRoot 'bin/llvm-rc.exe'
        $windres = Join-Path $CompilerRoot 'bin/x86_64-w64-mingw32-windres.exe'
        if (Test-Path $windres) {
            & $windres -I assets assets/app.rc -o build/app.res.o
        } else {
            & $rc /I assets /FO build/app.res assets/app.rc
        }
        if ($LASTEXITCODE) { throw 'Resource compilation failed' }
        $resource = if (Test-Path build/app.res.o) { 'build/app.res.o' } else { 'build/app.res' }
        $flags = @('-std=c++20','-O2','-Wall','-Wextra','-Wpedantic','-DUNICODE','-D_UNICODE','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00','-municode','-static')
        & $clang @flags -mwindows src/app.cpp src/config.cpp src/features.cpp src/wallpaper.cpp src/render.cpp $resource -o dist/panes.exe -ld2d1 -ldwrite -ldwmapi -luser32 -lgdi32 -lshell32 -lshlwapi -lole32 -lcomctl32 -ladvapi32 -lwtsapi32 -lshcore -lpsapi -luuid -lwindowscodecs -lcomdlg32 -lmsimg32 '-Wl,--dynamicbase,--nxcompat'
        if ($LASTEXITCODE) { throw 'App compilation failed' }
        if (!$SkipTests) {
            & $clang @flags tests/core_tests.cpp src/config.cpp src/features.cpp src/wallpaper.cpp -o build/CoreTests.exe -lshell32 -lole32 -luuid -lwindowscodecs -lpsapi
            if ($LASTEXITCODE) { throw 'Test compilation failed' }
            & ./build/CoreTests.exe (Join-Path $projectRoot 'test-output/core')
            if ($LASTEXITCODE) { throw 'Core tests failed' }
            & $clang @flags tests/render_tests.cpp src/config.cpp src/features.cpp src/render.cpp $resource -o build/RenderTests.exe -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32 -lole32 -luuid -lmsimg32
            if ($LASTEXITCODE) { throw 'Renderer test compilation failed' }
            & ./build/RenderTests.exe (Join-Path $projectRoot 'test-output/render-previews')
            if ($LASTEXITCODE) { throw 'Renderer checks failed' }
        }
    } else {
        cmake -S . -B build/msvc -A x64
        if ($LASTEXITCODE) { throw 'CMake configuration failed. Install MSVC + Windows SDK, or run tools/bootstrap.ps1.' }
        cmake --build build/msvc --config Release
        if ($LASTEXITCODE) { throw 'MSVC build failed' }
        if (!$SkipTests) {
            & ./build/msvc/Release/CoreTests.exe (Join-Path $projectRoot 'test-output/core')
            if ($LASTEXITCODE) { throw 'Core tests failed' }
            & ./build/msvc/Release/RenderTests.exe (Join-Path $projectRoot 'test-output/render-previews')
            if ($LASTEXITCODE) { throw 'Renderer checks failed' }
        }
        Copy-Item build/msvc/Release/panes.exe dist/panes.exe -Force
    }
    Copy-Item README.md,VALIDATION.md,THIRD-PARTY-NOTICES.txt,CHANGELOG.md dist -Force
    Copy-Item third_party/json.LICENSE dist -Force
    Copy-Item assets/app.ico dist/panes.ico -Force
    Copy-Item assets/panes.svg dist/panes.svg -Force
    Copy-Item assets/panes-icon-1024.png dist/panes-icon.png -Force
    New-Item -ItemType Directory -Force dist/licenses | Out-Null
    Copy-Item assets/fonts/*-OFL.txt dist/licenses -Force
    Copy-Item assets/fonts/*-TRADEMARKS.md dist/licenses -Force -ErrorAction SilentlyContinue
    Copy-Item assets/fonts/SOURCES.json dist/licenses/font-sources.json -Force
    if ($CompilerRoot) {
        Copy-Item (Join-Path $CompilerRoot 'LICENSE.TXT') dist/licenses/LLVM.txt -Force
        Copy-Item (Join-Path $CompilerRoot 'x86_64-w64-mingw32/share/mingw32/COPYING*') dist/licenses -Force
    }
    if ($Package) {
        if (!$InnoCompiler) { $InnoCompiler = Join-Path $projectRoot 'tools/InnoSetup/ISCC.exe' }
        if (!(Test-Path $InnoCompiler)) { throw 'Inno Setup compiler not found. Pass -InnoCompiler or run tools/bootstrap.ps1 -Inno.' }
        & $InnoCompiler installer/panes.iss
        if ($LASTEXITCODE) { throw 'Installer compilation failed' }
        Compress-Archive -Path dist/panes.exe,dist/panes.ico,dist/panes.svg,dist/panes-icon.png,dist/README.md,dist/VALIDATION.md,dist/CHANGELOG.md,dist/THIRD-PARTY-NOTICES.txt,dist/json.LICENSE,dist/licenses -DestinationPath dist/panes-1.2.1-x64.zip -Force
    }
} finally { Pop-Location }
