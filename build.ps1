# ============================================================================
# build.ps1 - Build Script for retropad
# ============================================================================
# This script compiles retropad using Visual Studio's cl.exe compiler.
# It automatically sets up the Visual Studio development environment and
# compiles all source files into retropad.exe.
#
# Usage: .\build.ps1 [-Clean]
#   -Clean: Remove all build artifacts before building
#
# Requirements:
#   - Visual Studio 2022 or later with C++ Desktop Development workload
#     OR Visual Studio Build Tools with C++ build tools
#   - Windows SDK with Resource Compiler (rc.exe)
#
# Installation:
#   If you don't have Visual Studio installed, download and install:
#   - Visual Studio Community (free): https://visualstudio.microsoft.com/
#   - OR Build Tools: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
#   Make sure to select "Desktop development with C++" during installation
# ============================================================================

param(
    [switch]$Clean = $false
)

# Configuration
$ProjectRoot = $PSScriptRoot
$BinariesDir = Join-Path $ProjectRoot "binaries"
$SourceFiles = @("retropad.c", "file_io.c")
$ResourceFile = "retropad.rc"
$OutputExe = "retropad.exe"

# Try to find Visual Studio installations (search newest first)
$VSYears = @("2026", "18", "2025", "17", "2024", "2023", "2022", "2021", "2019")
$VSEditions = @("Community", "Professional", "Enterprise", "BuildTools")
$VSPaths = @()

# Build comprehensive search list for Program Files
foreach ($year in $VSYears) {
    foreach ($edition in $VSEditions) {
        $VSPaths += "C:\Program Files\Microsoft Visual Studio\$year\$edition"
    }
}

# Add Program Files (x86) paths for older versions
foreach ($year in @("2019", "2017")) {
    foreach ($edition in $VSEditions) {
        $VSPaths += "C:\Program Files (x86)\Microsoft Visual Studio\$year\$edition"
    }
}

$VsDevCmd = $null
$VSPath = $null
foreach ($path in $VSPaths) {
    $candidate = "$path\Common7\Tools\VsDevCmd.bat"
    if (Test-Path $candidate) {
        $VsDevCmd = $candidate
        $VSPath = $path
        # Extract version info for display
        if ($path -match "\\(\d{4}|1[678])\\(\w+)$") {
            $VSVersion = $matches[1]
            $VSEdition = $matches[2]
        }
        break
    }
}

# Compiler flags
$CFlags = "/nologo /DUNICODE /D_UNICODE /W4 /EHsc /Zi /Od"
$LDFlags = "/nologo"
$Libs = "user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  retropad Build Script" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Check if Visual Studio is installed
if (-not $VsDevCmd) {
    Write-Host "ERROR: No C++ compiler found" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Visual Studio with C++ Desktop Development workload:" -ForegroundColor Yellow
    Write-Host "  1. Download Visual Studio Community: https://visualstudio.microsoft.com/" -ForegroundColor Cyan
    Write-Host "  2. Run installer and select 'Desktop development with C++'" -ForegroundColor Cyan
    Write-Host "  3. Or install Build Tools: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Searched for Visual Studio versions: 2026, 2025, 2024, 2023, 2022, 2019, 2017" -ForegroundColor Yellow
    Write-Host "Editions: Community, Professional, Enterprise, BuildTools" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found Visual Studio $VSVersion $VSEdition" -ForegroundColor Green
Write-Host "Location: $VSPath" -ForegroundColor DarkGray
Write-Host ""

# Change to project directory
Set-Location $ProjectRoot

# Create binaries directory if it doesn't exist
if (-not (Test-Path $BinariesDir)) {
    New-Item -ItemType Directory -Path $BinariesDir -Force | Out-Null
    Write-Host "Created binaries directory" -ForegroundColor Green
}

# Always clean binaries folder before building for a fresh compile
Write-Host "Cleaning binaries folder..." -ForegroundColor Yellow
$artifacts = @("*.obj", "*.res", "*.pdb", "*.exe", "*.ilk")
$cleaned = 0
foreach ($pattern in $artifacts) {
    $files = Get-ChildItem -Path $BinariesDir -Filter $pattern -File -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        Remove-Item -Path $file.FullName -Force -ErrorAction SilentlyContinue
        $cleaned++
    }
}
if ($cleaned -gt 0) {
    Write-Host "  Removed $cleaned file(s) from binaries\" -ForegroundColor Green
} else {
    Write-Host "  binaries\ folder is clean" -ForegroundColor DarkGray
}
Write-Host ""



# Check if source files exist
$missingFiles = @()
foreach ($file in $SourceFiles + $ResourceFile) {
    if (-not (Test-Path $file)) {
        $missingFiles += $file
    }
}
if ($missingFiles.Count -gt 0) {
    Write-Host "ERROR: Missing source files:" -ForegroundColor Red
    $missingFiles | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}

Write-Host "Setting up Visual Studio environment..." -ForegroundColor Yellow

# Create a temporary batch file to run the build commands
$tempBatch = [System.IO.Path]::GetTempFileName() + ".bat"
$buildCommands = @"
@echo off
call "$VsDevCmd" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to initialize Visual Studio environment
    exit /b 1
)

cd /d "$ProjectRoot"

echo Compiling source files...
"@

# Add compilation commands for each source file (output to binaries folder)
foreach ($source in $SourceFiles) {
    $obj = [System.IO.Path]::GetFileNameWithoutExtension($source) + ".obj"
    $buildCommands += "`necho   - $source"
    $buildCommands += "`ncl $CFlags /c /Fo:binaries\$obj /Fd:binaries\ `"$source`""
    $buildCommands += "`nif errorlevel 1 exit /b 1"
}

# Add resource compilation (output to binaries folder)
$buildCommands += @"

echo Compiling resources...
echo   - $ResourceFile
rc /fo binaries\retropad.res "$ResourceFile"
if errorlevel 1 exit /b 1
"@

# Add linking command (output to binaries folder)
$objectFiles = ($SourceFiles | ForEach-Object { "binaries\" + [System.IO.Path]::GetFileNameWithoutExtension($_) + ".obj" }) -join " "
$buildCommands += @"

echo Linking executable...
cl $LDFlags $objectFiles binaries\retropad.res $Libs /Fe:binaries\$OutputExe /Fd:binaries\
if errorlevel 1 exit /b 1

echo Build completed successfully!
exit /b 0
"@

# Write and execute the batch file
$buildCommands | Out-File -FilePath $tempBatch -Encoding ASCII

try {
    # Execute the batch file and capture output
    $process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$tempBatch`"" -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\build_stdout.txt" -RedirectStandardError "$env:TEMP\build_stderr.txt"
    
    # Display output
    if (Test-Path "$env:TEMP\build_stdout.txt") {
        Get-Content "$env:TEMP\build_stdout.txt" | ForEach-Object {
            if ($_ -match "error|failed" -and $_ -notmatch "errorlevel") {
                Write-Host $_ -ForegroundColor Red
            } elseif ($_ -match "warning") {
                Write-Host $_ -ForegroundColor Yellow
            } elseif ($_ -match "Build completed successfully") {
                Write-Host $_ -ForegroundColor Green
            } else {
                Write-Host $_
            }
        }
    }
    
    if (Test-Path "$env:TEMP\build_stderr.txt") {
        $stderr = Get-Content "$env:TEMP\build_stderr.txt"
        if ($stderr) {
            Write-Host ""
            Write-Host "Errors/Warnings:" -ForegroundColor Yellow
            $stderr | ForEach-Object { Write-Host $_ -ForegroundColor Yellow }
        }
    }
    
    # Check build result
    if ($process.ExitCode -eq 0) {
        Write-Host ""
        Write-Host "============================================" -ForegroundColor Green
        Write-Host "  Build Successful!" -ForegroundColor Green
        Write-Host "============================================" -ForegroundColor Green
        
        $outputPath = Join-Path $BinariesDir $OutputExe
        if (Test-Path $outputPath) {
            $exeInfo = Get-Item $outputPath
            Write-Host ""
            Write-Host "Output:" -ForegroundColor Cyan
            Write-Host "  File: binaries\$OutputExe" -ForegroundColor White
            Write-Host "  Size: $([math]::Round($exeInfo.Length / 1KB, 2)) KB" -ForegroundColor White
            Write-Host "  Date: $($exeInfo.LastWriteTime)" -ForegroundColor White
        }
        
        exit 0
    } else {
        Write-Host ""
        Write-Host "============================================" -ForegroundColor Red
        Write-Host "  Build Failed!" -ForegroundColor Red
        Write-Host "============================================" -ForegroundColor Red
        exit 1
    }
}
finally {
    # Cleanup temporary files
    Remove-Item $tempBatch -Force -ErrorAction SilentlyContinue
    Remove-Item "$env:TEMP\build_stdout.txt" -Force -ErrorAction SilentlyContinue
    Remove-Item "$env:TEMP\build_stderr.txt" -Force -ErrorAction SilentlyContinue
}
