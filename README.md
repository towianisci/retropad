# retropad

A Petzold-style Win32 Notepad clone written in plain C. Features classic menus, accelerators, word wrap toggle, status bar, find/replace, font picker, time/date insertion, BOM-aware file I/O, and full printing support with page setup.

## Prerequisites (Windows)
- Git
- Visual Studio 2022 or later (or Build Tools) with the "Desktop development with C++" workload
- PowerShell (included with Windows)

Optional: MinGW-w64 for `make` + `gcc` (a separate POSIX-style `Makefile` is included).

## Get the code
```powershell
git clone https://github.com/towianisci/retropad.git
cd retropad
```

## Build with PowerShell Script (Recommended)

The easiest way to build retropad is using the included `build.ps1` script. It automatically:
- Detects your Visual Studio installation (2017-2026, any edition)
- Sets up the build environment
- Cleans previous build artifacts
- Compiles all source files
- Outputs everything to the `binaries\` folder

**To build:**
```powershell
.\build.ps1
```

The script will:
1. Search for Visual Studio installations (newest first)
2. Clean the `binaries\` folder
3. Compile `retropad.c` and `file_io.c`
4. Compile resources from `retropad.rc`
5. Link everything into `binaries\retropad.exe`

**Build output:**
- `binaries\retropad.exe` - The executable
- `binaries\*.obj` - Object files
- `binaries\retropad.res` - Compiled resources
- `binaries\*.pdb` - Debug symbols

**Troubleshooting:**
- If Visual Studio isn't found, install the "Desktop development with C++" workload
- The script searches these VS versions: 2026, 2025, 2024, 2023, 2022, 2021, 2019, 2017
- It checks all editions: Community, Professional, Enterprise, BuildTools

## Build with MSVC (`nmake`) - Alternative
From a Developer Command Prompt:
```bat
nmake /f makefile
```
This runs `rc` then `cl` and produces `retropad.exe` in the repo root. Clean with:
```bat
nmake /f makefile clean
```

## Build with MinGW (optional)
If you have `gcc`, `windres`, and `make` on PATH:
```bash
make
```
Artifacts end up in the repo root (`retropad.exe`, object files, and `retropad.res`). Clean with `make clean`.

## Run
Double-click `retropad.exe` or start from a prompt:
```bat
.\retropad.exe
```

## Features
- **Classic Menus & Shortcuts**: File, Edit, Format, View, Help with standard Notepad key bindings (Ctrl+N/O/S, Ctrl+F, F3, Ctrl+H, Ctrl+G, F5, etc.)
- **Word Wrap**: Toggles horizontal scrolling; status bar remains visible when word wrap is enabled
- **Find/Replace**: Standard Windows find/replace dialogs with match case and direction options
- **Go To Line**: Jump to specific line number (disabled when word wrap is on)
- **Font Selection**: Choose any installed font via Windows font picker
- **Time/Date**: Insert current time and date at cursor position (F5)
- **Drag & Drop**: Drop files directly into the window to open them
- **Smart File I/O**: Detects UTF-8/UTF-16/ANSI BOMs, saves with UTF-8 BOM by default
- **Printing**: Full printing support with page setup dialog for margins and orientation
- **Settings Persistence**: Word wrap, status bar visibility, and font preferences are saved to the Windows registry and restored on next launch
- **Application Icon**: Custom icon from `res/retropad.ico`

## Project Layout
- `retropad.c` — Main application: WinMain, window procedure, UI logic, find/replace, menus, printing
- `file_io.c/.h` — File operations with encoding detection and conversion
- `resource.h` — Resource ID definitions
- `retropad.rc` — Resource definitions: menus, accelerators, dialogs, version info, icon
- `res/retropad.ico` — Application icon
- `build.ps1` — PowerShell build script (recommended)
- `makefile` — MSVC `nmake` build script (alternative)
- `Makefile` — MinGW/GNU make build script (optional)
- `binaries/` — Build output directory (not in source control)

## Notes
- All source code is fully commented for easy understanding
- Build script automatically detects Visual Studio installation
- Clean build performed automatically before each compile
- Debug symbols (PDB files) included for debugging support
