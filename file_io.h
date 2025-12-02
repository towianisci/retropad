// ============================================================================
// file_io.h - File I/O Helper Functions Header
// ============================================================================
// This header provides text file loading and saving with encoding detection.
// Supports UTF-8, UTF-16LE, UTF-16BE, and ANSI encodings with BOM detection.
// ============================================================================

#pragma once

#include <windows.h>

// ============================================================================
// Text Encoding Types
// ============================================================================
// Defines the various text encodings supported by retropad for file I/O.
// These encodings are automatically detected when opening files based on
// the presence of a Byte Order Mark (BOM) or character analysis.
// ============================================================================
typedef enum TextEncoding {
    ENC_UTF8 = 1,      // UTF-8 encoding (with or without BOM: 0xEF, 0xBB, 0xBF)
    ENC_UTF16LE = 2,   // UTF-16 Little Endian (BOM: 0xFF, 0xFE)
    ENC_UTF16BE = 3,   // UTF-16 Big Endian (BOM: 0xFE, 0xFF)
    ENC_ANSI = 4       // ANSI/Windows Code Page encoding (no BOM)
} TextEncoding;

// ============================================================================
// File Result Structure
// ============================================================================
// Stores the result of a file operation, including the full path and the
// detected or selected encoding. Currently used internally for file operations.
// ============================================================================
typedef struct FileResult {
    WCHAR path[MAX_PATH];         // Full path to the file
    TextEncoding encoding;         // Encoding type detected or selected
} FileResult;

// ============================================================================
// File Dialog Functions
// ============================================================================

// Opens a standard Windows "Open File" dialog for selecting a file to open.
// Parameters:
//   owner    - Parent window handle for the dialog
//   pathOut  - Buffer to receive the selected file path
//   pathLen  - Size of the pathOut buffer in WCHARs
// Returns: TRUE if user selected a file, FALSE if cancelled
BOOL OpenFileDialog(HWND owner, WCHAR *pathOut, DWORD pathLen);

// Opens a standard Windows "Save File" dialog for selecting a file to save.
// Parameters:
//   owner    - Parent window handle for the dialog
//   pathOut  - Buffer containing default filename and receiving selected path
//   pathLen  - Size of the pathOut buffer in WCHARs
// Returns: TRUE if user selected a file, FALSE if cancelled
BOOL SaveFileDialog(HWND owner, WCHAR *pathOut, DWORD pathLen);

// ============================================================================
// File I/O Functions
// ============================================================================

// Loads a text file from disk with automatic encoding detection.
// The function detects the encoding by examining the BOM (Byte Order Mark)
// at the start of the file, or by attempting UTF-8 validation.
// Memory is allocated for the text; caller must free with HeapFree().
// Parameters:
//   owner       - Parent window for error message boxes
//   path        - Full path to the file to load
//   textOut     - Receives pointer to allocated text buffer (wide char)
//   lengthOut   - Receives length of text in characters (can be NULL)
//   encodingOut - Receives detected encoding (can be NULL)
// Returns: TRUE on success, FALSE on failure (displays error message)
BOOL LoadTextFile(HWND owner, LPCWSTR path, WCHAR **textOut, size_t *lengthOut, TextEncoding *encodingOut);

// Saves text to a file with the specified encoding.
// Automatically adds appropriate BOM (Byte Order Mark) for UTF encodings.
// Parameters:
//   owner    - Parent window for error message boxes
//   path     - Full path to the file to save
//   text     - Text to save (wide char string)
//   length   - Length of text in characters
//   encoding - Encoding to use when saving
// Returns: TRUE on success, FALSE on failure (displays error message)
BOOL SaveTextFile(HWND owner, LPCWSTR path, LPCWSTR text, size_t length, TextEncoding encoding);
