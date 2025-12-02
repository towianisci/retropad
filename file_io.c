// ============================================================================
// file_io.c - Text File I/O Implementation with Encoding Detection
// ============================================================================
// This module handles all file loading and saving operations for retropad.
// It includes:
// - Automatic encoding detection via BOM (Byte Order Mark) inspection
// - Support for UTF-8, UTF-16LE, UTF-16BE, and ANSI encodings
// - Conversion between different encodings and Windows wide char (UTF-16LE)
// - Standard Windows file open/save dialogs
// ============================================================================

#include "file_io.h"
#include <commdlg.h>   // For GetOpenFileNameW, GetSaveFileNameW dialogs
#include <strsafe.h>   // For safe string operations
#include <stdlib.h>    // For standard library functions

// ============================================================================
// DetectEncoding - Automatically Detect Text File Encoding
// ============================================================================
// Examines the first few bytes of a file to determine its encoding by looking
// for a Byte Order Mark (BOM) or attempting UTF-8 validation.
// Detection logic:
//   1. Check for UTF-16LE BOM (0xFF 0xFE)
//   2. Check for UTF-16BE BOM (0xFE 0xFF)
//   3. Check for UTF-8 BOM (0xEF 0xBB 0xBF)
//   4. Try to convert as UTF-8 - if successful, assume UTF-8
//   5. Otherwise, assume ANSI (Windows code page)
// Parameters:
//   data - Pointer to the file data buffer
//   size - Size of the data in bytes
// Returns: Detected TextEncoding value
// ============================================================================
static TextEncoding DetectEncoding(const BYTE *data, DWORD size) {
    // Check for UTF-16 Little Endian BOM (most common on Windows)
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        return ENC_UTF16LE;
    }
    // Check for UTF-16 Big Endian BOM (less common)
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        return ENC_UTF16BE;
    }
    // Check for UTF-8 BOM (optional for UTF-8, but indicates encoding)
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return ENC_UTF8;
    }
    // No BOM found - try to validate as UTF-8
    // If conversion succeeds with strict validation, assume UTF-8
    // MB_ERR_INVALID_CHARS causes failure if invalid UTF-8 sequences exist
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (LPCSTR)data, size, NULL, 0);
    return (wlen > 0) ? ENC_UTF8 : ENC_ANSI;
}

// ============================================================================
// DecodeToWide - Convert File Data to Wide Character String
// ============================================================================
// Converts raw file bytes to Windows wide character (UTF-16LE) format based
// on the detected or specified encoding. Allocates memory for the result
// which must be freed by the caller using HeapFree().
// Parameters:
//   data        - Raw file data buffer
//   size        - Size of data in bytes
//   encoding    - Encoding type to use for conversion
//   outText     - Receives pointer to allocated wide char string
//   outLength   - Receives length of string in characters (can be NULL)
// Returns: TRUE on success, FALSE on failure
// ============================================================================
static BOOL DecodeToWide(const BYTE *data, DWORD size, TextEncoding encoding, WCHAR **outText, size_t *outLength) {
    int chars = 0;
    WCHAR *buffer = NULL;

    switch (encoding) {
    // ------------------------------------------------------------------------
    // UTF-16 Little Endian - Native Windows Unicode format
    // ------------------------------------------------------------------------
    case ENC_UTF16LE: {
        if (size < 2) return FALSE;
        // Skip BOM if present (2 bytes)
        DWORD byteOffset = (data[0] == 0xFF && data[1] == 0xFE) ? 2 : 0;
        DWORD wcharCount = (size - byteOffset) / 2;  // Each WCHAR is 2 bytes
        buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (wcharCount + 1) * sizeof(WCHAR));
        if (!buffer) return FALSE;
        // Direct memory copy - no conversion needed (already UTF-16LE)
        CopyMemory(buffer, data + byteOffset, wcharCount * sizeof(WCHAR));
        buffer[wcharCount] = L'\0';
        chars = (int)wcharCount;
        break;
    }
    // ------------------------------------------------------------------------
    // UTF-16 Big Endian - Requires byte swapping for Windows
    // ------------------------------------------------------------------------
    case ENC_UTF16BE: {
        if (size < 2) return FALSE;
        // Skip BOM if present (2 bytes)
        DWORD byteOffset = (data[0] == 0xFE && data[1] == 0xFF) ? 2 : 0;
        DWORD wcharCount = (size - byteOffset) / 2;
        buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (wcharCount + 1) * sizeof(WCHAR));
        if (!buffer) return FALSE;
        // Swap bytes from big endian to little endian
        for (DWORD i = 0; i < wcharCount; ++i) {
            buffer[i] = (WCHAR)((data[byteOffset + i * 2] << 8) | data[byteOffset + i * 2 + 1]);
        }
        buffer[wcharCount] = L'\0';
        chars = (int)wcharCount;
        break;
    }
    // ------------------------------------------------------------------------
    // UTF-8 - Variable-length encoding (1-4 bytes per character)
    // ------------------------------------------------------------------------
    case ENC_UTF8: {
        // Skip UTF-8 BOM if present (3 bytes)
        DWORD byteOffset = (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ? 3 : 0;
        // First pass: determine required buffer size
        chars = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)(data + byteOffset), size - byteOffset, NULL, 0);
        if (chars <= 0) return FALSE;
        buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (chars + 1) * sizeof(WCHAR));
        if (!buffer) return FALSE;
        // Second pass: perform actual conversion
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)(data + byteOffset), size - byteOffset, buffer, chars);
        buffer[chars] = L'\0';
        break;
    }
    // ------------------------------------------------------------------------
    // ANSI - Windows Code Page (typically CP1252 on English systems)
    // ------------------------------------------------------------------------
    case ENC_ANSI:
    default: {
        // CP_ACP = Active Code Page (system default)
        // First pass: determine required buffer size
        chars = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)data, size, NULL, 0);
        if (chars <= 0) return FALSE;
        buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (chars + 1) * sizeof(WCHAR));
        if (!buffer) return FALSE;
        // Second pass: perform actual conversion
        MultiByteToWideChar(CP_ACP, 0, (LPCSTR)data, size, buffer, chars);
        buffer[chars] = L'\0';
        break;
    }
    }

    // Return the converted text and its length
    *outText = buffer;
    if (outLength) {
        *outLength = (size_t)chars;
    }
    return TRUE;
}

// ============================================================================
// LoadTextFile - Load and Decode a Text File
// ============================================================================
// Loads a complete text file into memory, automatically detecting its encoding
// and converting it to wide character format. The function:
//   1. Opens the file for reading
//   2. Reads entire file into memory
//   3. Detects the encoding
//   4. Converts to wide character (UTF-16LE)
//   5. Returns allocated buffer (caller must free with HeapFree)
// Parameters:
//   owner       - Parent window for error dialogs
//   path        - Full path to file to load
//   textOut     - Receives allocated text buffer
//   lengthOut   - Receives text length in characters (optional)
//   encodingOut - Receives detected encoding (optional)
// Returns: TRUE on success, FALSE on failure (shows error message)
// ============================================================================
BOOL LoadTextFile(HWND owner, LPCWSTR path, WCHAR **textOut, size_t *lengthOut, TextEncoding *encodingOut) {
    // Initialize outputs to safe defaults
    *textOut = NULL;
    if (lengthOut) *lengthOut = 0;
    if (encodingOut) *encodingOut = ENC_UTF8;

    // Open the file for reading
    // FILE_SHARE_READ allows other processes to read while we have it open
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxW(owner, L"Unable to open file.", L"retropad", MB_ICONERROR);
        return FALSE;
    }

    // Get file size and verify it's not too large
    // We limit to UINT_MAX (4GB) for practical memory reasons
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > (LONGLONG)UINT_MAX) {
        CloseHandle(file);
        MessageBoxW(owner, L"Unsupported file size.", L"retropad", MB_ICONERROR);
        return FALSE;
    }

    // Allocate buffer for file data (+3 bytes for potential BOM checking safety)
    DWORD bytes = (DWORD)size.QuadPart;
    BYTE *buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, bytes + 3);
    if (!buffer) {
        CloseHandle(file);
        MessageBoxW(owner, L"Out of memory.", L"retropad", MB_ICONERROR);
        return FALSE;
    }

    // Read the entire file into the buffer
    DWORD read = 0;
    BOOL ok = ReadFile(file, buffer, bytes, &read, NULL);
    CloseHandle(file);  // Done with file handle
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        MessageBoxW(owner, L"Failed reading file.", L"retropad", MB_ICONERROR);
        return FALSE;
    }

    // Handle empty file case - return empty string
    if (read == 0) {
        WCHAR *empty = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR));
        if (!empty) {
            HeapFree(GetProcessHeap(), 0, buffer);
            return FALSE;
        }
        empty[0] = L'\0';
        *textOut = empty;
        if (lengthOut) *lengthOut = 0;
        if (encodingOut) *encodingOut = ENC_UTF8;
        HeapFree(GetProcessHeap(), 0, buffer);
        return TRUE;
    }

    // Detect the file's encoding
    TextEncoding enc = DetectEncoding(buffer, read);
    
    // Convert to wide character format
    WCHAR *text = NULL;
    size_t len = 0;
    if (!DecodeToWide(buffer, read, enc, &text, &len)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        MessageBoxW(owner, L"Unable to decode file.", L"retropad", MB_ICONERROR);
        return FALSE;
    }

    // Clean up and return results
    HeapFree(GetProcessHeap(), 0, buffer);
    *textOut = text;
    if (lengthOut) *lengthOut = len;
    if (encodingOut) *encodingOut = enc;
    return TRUE;
}

// ============================================================================
// WriteUTF8WithBOM - Write Text as UTF-8 with BOM
// ============================================================================
// Converts wide character text to UTF-8 and writes it to a file with a BOM.
// The UTF-8 BOM (0xEF 0xBB 0xBF) helps identify the file as UTF-8 encoded.
// Parameters:
//   file   - Open file handle (must have write access)
//   text   - Wide character string to write
//   length - Length of text in characters
// Returns: TRUE on success, FALSE on failure
// ============================================================================
static BOOL WriteUTF8WithBOM(HANDLE file, const WCHAR *text, size_t length) {
    // UTF-8 BOM: 0xEF 0xBB 0xBF
    static const BYTE bom[] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    // Write BOM first
    if (!WriteFile(file, bom, sizeof(bom), &written, NULL)) {
        return FALSE;
    }
    // Calculate required buffer size for UTF-8 conversion
    int bytes = WideCharToMultiByte(CP_UTF8, 0, text, (int)length, NULL, 0, NULL, NULL);
    if (bytes <= 0) return FALSE;
    // Allocate buffer and convert
    BYTE *buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (!buffer) return FALSE;
    WideCharToMultiByte(CP_UTF8, 0, text, (int)length, (LPSTR)buffer, bytes, NULL, NULL);
    // Write converted UTF-8 data
    BOOL ok = WriteFile(file, buffer, bytes, &written, NULL);
    HeapFree(GetProcessHeap(), 0, buffer);
    return ok;
}

// ============================================================================
// WriteUTF16LE - Write Text as UTF-16 Little Endian with BOM
// ============================================================================
// Writes wide character text directly to file as UTF-16LE with BOM.
// This is the most efficient save format since Windows uses UTF-16LE internally.
// Parameters:
//   file   - Open file handle (must have write access)
//   text   - Wide character string to write
//   length - Length of text in characters
// Returns: TRUE on success, FALSE on failure
// ============================================================================
static BOOL WriteUTF16LE(HANDLE file, const WCHAR *text, size_t length) {
    // UTF-16LE BOM: 0xFF 0xFE
    static const BYTE bom[] = {0xFF, 0xFE};
    DWORD written = 0;
    // Write BOM first
    if (!WriteFile(file, bom, sizeof(bom), &written, NULL)) {
        return FALSE;
    }
    // Write text directly (already in UTF-16LE format)
    return WriteFile(file, text, (DWORD)(length * sizeof(WCHAR)), &written, NULL);
}

// ============================================================================
// WriteANSI - Write Text as ANSI (Windows Code Page)
// ============================================================================
// Converts wide character text to ANSI using the system code page and writes
// to file without a BOM. Characters that cannot be represented in the active
// code page will be replaced with a default character (usually '?').
// Parameters:
//   file   - Open file handle (must have write access)
//   text   - Wide character string to write
//   length - Length of text in characters
// Returns: TRUE on success, FALSE on failure
// ============================================================================
static BOOL WriteANSI(HANDLE file, const WCHAR *text, size_t length) {
    // Calculate required buffer size for ANSI conversion
    // CP_ACP = Active Code Page (system default)
    int bytes = WideCharToMultiByte(CP_ACP, 0, text, (int)length, NULL, 0, NULL, NULL);
    if (bytes <= 0) return FALSE;
    // Allocate buffer and convert
    BYTE *buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (!buffer) return FALSE;
    WideCharToMultiByte(CP_ACP, 0, text, (int)length, (LPSTR)buffer, bytes, NULL, NULL);
    // Write ANSI data (no BOM)
    DWORD written = 0;
    BOOL ok = WriteFile(file, buffer, bytes, &written, NULL);
    HeapFree(GetProcessHeap(), 0, buffer);
    return ok;
}

// ============================================================================
// SaveTextFile - Save Text to File with Specified Encoding
// ============================================================================
// Creates (or overwrites) a file and writes text in the specified encoding.
// Automatically adds appropriate BOM for UTF encodings.
// Parameters:
//   owner    - Parent window for error dialogs
//   path     - Full path to file to save
//   text     - Wide character text to save
//   length   - Length of text in characters
//   encoding - Encoding to use when saving
// Returns: TRUE on success, FALSE on failure (shows error message)
// ============================================================================
BOOL SaveTextFile(HWND owner, LPCWSTR path, LPCWSTR text, size_t length, TextEncoding encoding) {
    // Create (or overwrite) the file
    // CREATE_ALWAYS: Creates new file or truncates existing file to zero length
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxW(owner, L"Unable to create file.", L"retropad", MB_ICONERROR);
        return FALSE;
    }

    // Write file using appropriate encoding
    BOOL ok = FALSE;
    switch (encoding) {
    case ENC_UTF16LE:
        ok = WriteUTF16LE(file, text, length);
        break;
    case ENC_ANSI:
        ok = WriteANSI(file, text, length);
        break;
    case ENC_UTF16BE:
        // UTF-16BE is uncommon on Windows; convert to UTF-8 for better compatibility
        ok = WriteUTF8WithBOM(file, text, length);
        encoding = ENC_UTF8;
        break;
    case ENC_UTF8:
    default:
        ok = WriteUTF8WithBOM(file, text, length);
        break;
    }

    CloseHandle(file);
    if (!ok) {
        MessageBoxW(owner, L"Failed writing file.", L"retropad", MB_ICONERROR);
    }
    return ok;
}

// ============================================================================
// OpenFileDialog - Display Standard Windows "Open File" Dialog
// ============================================================================
// Shows the common file open dialog allowing the user to select a file.
// Pre-configured for text files with .txt extension.
// Parameters:
//   owner   - Parent window handle
//   pathOut - Buffer to receive selected file path
//   pathLen - Size of pathOut buffer in WCHARs
// Returns: TRUE if user selected a file, FALSE if cancelled
// ============================================================================
BOOL OpenFileDialog(HWND owner, WCHAR *pathOut, DWORD pathLen) {
    pathOut[0] = L'\0';
    OPENFILENAMEW ofn = {0};
    // Filter string format: "Display Name\0Pattern\0Display Name\0Pattern\0\0"
    WCHAR filter[] = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = pathOut;             // Buffer for selected file path
    ofn.nMaxFile = pathLen;
    // OFN_FILEMUSTEXIST: File must exist (no new file creation in Open dialog)
    // OFN_HIDEREADONLY: Hide the read-only checkbox
    // OFN_PATHMUSTEXIST: Path must be valid
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"txt";            // Default extension if user doesn't specify
    return GetOpenFileNameW(&ofn);
}

// ============================================================================
// SaveFileDialog - Display Standard Windows "Save File" Dialog
// ============================================================================
// Shows the common file save dialog allowing the user to specify save location.
// Pre-configured for text files with .txt extension. Prompts before overwriting.
// Parameters:
//   owner   - Parent window handle
//   pathOut - Buffer with default name and receiving selected file path
//   pathLen - Size of pathOut buffer in WCHARs
// Returns: TRUE if user selected a file, FALSE if cancelled
// ============================================================================
BOOL SaveFileDialog(HWND owner, WCHAR *pathOut, DWORD pathLen) {
    OPENFILENAMEW ofn = {0};
    WCHAR filter[] = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = pathOut;             // Buffer for selected file path
    ofn.nMaxFile = pathLen;
    // OFN_OVERWRITEPROMPT: Ask user before overwriting existing file
    // OFN_PATHMUSTEXIST: Path must be valid
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"txt";            // Default extension if user doesn't specify
    // If no filename provided, suggest "*.txt" pattern
    if (pathOut[0] == L'\0') {
        StringCchCopyW(pathOut, pathLen, L"*.txt");
    }
    return GetSaveFileNameW(&ofn);
}

