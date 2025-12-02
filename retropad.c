// ============================================================================
// retropad.c - A Classic Win32 Notepad Clone
// ============================================================================
// A Petzold-style text editor implemented in plain C using the Win32 API.
// Features:
// - Classic Windows menus and keyboard accelerators
// - Word wrap toggle with automatic status bar management
// - Find and Replace functionality
// - Font selection
// - Status bar showing line/column position
// - File operations with encoding detection (UTF-8, UTF-16, ANSI)
// - Drag-and-drop file support
// - "Go To Line" navigation
// - Time/Date insertion
// ============================================================================

// Windows API Headers
#include <windows.h>     // Core Windows API
#include <commdlg.h>     // Common dialogs (Open, Save, Font, Find/Replace)
#include <commctrl.h>    // Common controls (Status bar)
#include <shellapi.h>    // Shell functions (Drag-drop)
#include <strsafe.h>     // Safe string operations

// Application Headers
#include "resource.h"    // Resource IDs (menu items, dialogs, etc.)
#include "file_io.h"     // File I/O with encoding support

// ============================================================================
// Application Constants
// ============================================================================
#define APP_TITLE      L"retropad"      // Application name for title bar
#define UNTITLED_NAME  L"Untitled"      // Name for unsaved documents
#define MAX_PATH_BUFFER 1024            // Buffer size for file paths
#define DEFAULT_WIDTH  640              // Default window width in pixels
#define DEFAULT_HEIGHT 480              // Default window height in pixels

// Registry settings
#define REG_KEY_PATH   L"Software\\retropad"  // Registry path for settings
#define REG_WORD_WRAP  L"WordWrap"            // Word wrap setting name

// ============================================================================
// Application State Structure
// ============================================================================
// This structure holds all the application's state including window handles,
// current file information, and UI settings. Using a single structure makes
// state management cleaner and avoids excessive global variables.
// ============================================================================
typedef struct AppState {
    // Window Handles
    HWND hwndMain;                      // Main window handle
    HWND hwndEdit;                      // Edit control (multi-line text box)
    HWND hwndStatus;                    // Status bar at bottom
    HFONT hFont;                        // Current font for editor
    
    // Document State
    WCHAR currentPath[MAX_PATH_BUFFER]; // Full path of current file (empty = unsaved)
    BOOL modified;                      // TRUE if document has unsaved changes
    TextEncoding encoding;              // Encoding of current file
    
    // UI State
    BOOL wordWrap;                      // TRUE if word wrap is enabled
    BOOL statusVisible;                 // TRUE if status bar is visible
    BOOL statusBeforeWrap;              // Remembers status visibility before word wrap
    
    // Find/Replace State
    FINDREPLACEW find;                  // Windows find/replace dialog structure
    HWND hFindDlg;                      // Handle to Find dialog (modeless)
    HWND hReplaceDlg;                   // Handle to Replace dialog (modeless)
    UINT findFlags;                     // Find flags (match case, direction, etc.)
    WCHAR findText[128];                // Current find string
    WCHAR replaceText[128];             // Current replace string
    
    // Print State
    PAGESETUPDLGW pageSetup;            // Page setup settings (margins, orientation)
    PRINTDLGW printDlg;                 // Print dialog settings
} AppState;

// ============================================================================
// Global Variables
// ============================================================================
// Kept minimal - only truly global state that must be accessed from callbacks
// ============================================================================
static AppState g_app = {0};           // Main application state
static HINSTANCE g_hInst = NULL;       // Instance handle for this process
static UINT g_findMsg = 0;             // Registered message ID for find/replace dialogs

// ============================================================================
// Forward Declarations
// ============================================================================
// Functions declared here are defined later in the file
// ============================================================================

// UI Update Functions
static void UpdateTitle(HWND hwnd);                    // Update window title with filename
static void CreateEditControl(HWND hwnd);              // Create/recreate edit control
static void UpdateLayout(HWND hwnd);                   // Resize controls to fit window
static void UpdateStatusBar(HWND hwnd);                // Update status bar with cursor info
static void ToggleStatusBar(HWND hwnd, BOOL visible);  // Show/hide status bar

// File Operations
static BOOL PromptSaveChanges(HWND hwnd);              // Ask to save if modified
static BOOL DoFileOpen(HWND hwnd);                     // Open file dialog and load
static BOOL DoFileSave(HWND hwnd, BOOL saveAs);        // Save file (with optional dialog)
static void DoFileNew(HWND hwnd);                      // Start new document
static BOOL LoadDocumentFromPath(HWND hwnd, LPCWSTR path); // Load file from path

// Edit Operations
static void SetWordWrap(HWND hwnd, BOOL enabled);      // Toggle word wrap mode
static void DoSelectFont(HWND hwnd);                   // Show font selection dialog
static void InsertTimeDate(HWND hwnd);                 // Insert current time/date at cursor

// Settings Persistence
static BOOL LoadWordWrapSetting(void);                 // Load word wrap from registry
static void SaveWordWrapSetting(BOOL enabled);         // Save word wrap to registry

// Print Operations
static void DoPageSetup(HWND hwnd);                    // Show page setup dialog
static void DoPrint(HWND hwnd);                        // Show print dialog and print

// Find/Replace Operations
static void ShowFindDialog(HWND hwnd);                 // Show modeless Find dialog
static void ShowReplaceDialog(HWND hwnd);              // Show modeless Replace dialog
static BOOL DoFindNext(BOOL reverse);                  // Find next occurrence
static void HandleFindReplace(LPFINDREPLACE lpfr);     // Process Find/Replace messages

// Dialog Procedures
static INT_PTR CALLBACK GoToDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK HelpDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK AboutDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// GetEditText - Extract All Text from Edit Control
// ============================================================================
// Allocates memory and retrieves all text from an edit control.
// Caller must free the returned buffer using HeapFree().
// Parameters:
//   hwndEdit  - Handle to edit control
//   bufferOut - Receives pointer to allocated text buffer
//   lengthOut - Receives text length in characters (can be NULL)
// Returns: TRUE on success, FALSE on failure
// ============================================================================
static BOOL GetEditText(HWND hwndEdit, WCHAR **bufferOut, int *lengthOut) {
    // Query length first to allocate appropriate buffer size
    int length = GetWindowTextLengthW(hwndEdit);
    WCHAR *buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (length + 1) * sizeof(WCHAR));
    if (!buffer) return FALSE;
    // Retrieve the actual text
    GetWindowTextW(hwndEdit, buffer, length + 1);
    if (lengthOut) *lengthOut = length;
    *bufferOut = buffer;
    return TRUE;
}

// ============================================================================
// FindInEdit - Search for Text in Edit Control
// ============================================================================
// Searches for a substring within the edit control's text. Supports:
// - Case-sensitive and case-insensitive search
// - Forward and backward (reverse) search
// - Wrap-around search from start position
// Parameters:
//   hwndEdit  - Handle to edit control
//   needle    - Text to search for
//   matchCase - TRUE for case-sensitive search
//   searchDown- TRUE to search forward, FALSE for backward
//   startPos  - Character position to start searching from
//   outStart  - Receives start position of found text
//   outEnd    - Receives end position of found text
// Returns: TRUE if found, FALSE if not found
// ============================================================================
static BOOL FindInEdit(HWND hwndEdit, const WCHAR *needle, BOOL matchCase, BOOL searchDown, DWORD startPos, DWORD *outStart, DWORD *outEnd) {
    // Validate search string
    if (!needle || needle[0] == L'\0') return FALSE;

    // Get all text from edit control
    WCHAR *text = NULL;
    int len = 0;
    if (!GetEditText(hwndEdit, &text, &len)) return FALSE;

    // Prepare search - need separate buffers for case-insensitive search
    size_t needleLen = wcslen(needle);
    WCHAR *haystack = text;  // Text to search in
    WCHAR *needleBuf = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (needleLen + 1) * sizeof(WCHAR));
    if (!needleBuf) {
        HeapFree(GetProcessHeap(), 0, text);
        return FALSE;
    }
    StringCchCopyW(needleBuf, needleLen + 1, needle);

    // Convert both to lowercase for case-insensitive search
    if (!matchCase) {
        CharLowerBuffW(haystack, len);
        CharLowerBuffW(needleBuf, (DWORD)needleLen);
    }

    // Clamp start position to valid range
    if (startPos > (DWORD)len) startPos = (DWORD)len;

    WCHAR *found = NULL;
    if (searchDown) {
        // Forward search: Start from startPos and wrap to beginning if needed
        // Search from startPos to end
        found = wcsstr(haystack + startPos, needleBuf);
        // If not found and we didn't start at beginning, wrap around
        if (!found && startPos > 0) {
            found = wcsstr(haystack, needleBuf);
        }
    } else {
        // Backward search: Find last occurrence before startPos
        WCHAR *p = haystack;
        while ((p = wcsstr(p, needleBuf)) != NULL) {
            DWORD idx = (DWORD)(p - haystack);
            if (idx < startPos) {
                found = p;  // Keep updating with each match before startPos
                p++;
            } else {
                break;      // Stop when we reach/pass startPos
            }
        }
        // If not found before startPos, wrap around and find last occurrence
        if (!found && startPos < (DWORD)len) {
            p = haystack + startPos;
            while ((p = wcsstr(p, needleBuf)) != NULL) {
                found = p;
                p++;
            }
        }
    }

    // If found, calculate positions and return TRUE
    BOOL result = FALSE;
    if (found) {
        DWORD pos = (DWORD)(found - haystack);
        *outStart = pos;
        *outEnd = pos + (DWORD)needleLen;
        result = TRUE;
    }

    // Clean up allocated memory
    HeapFree(GetProcessHeap(), 0, text);
    HeapFree(GetProcessHeap(), 0, needleBuf);
    return result;
}

// ============================================================================
// ReplaceAllOccurrences - Replace All Instances of Text
// ============================================================================
// Finds all occurrences of search text and replaces them with replacement text.
// Efficiently handles the replacement by:
//   1. Counting occurrences first
//   2. Allocating appropriate buffer for result
//   3. Building new text with replacements
//   4. Setting edit control text once
// Parameters:
//   hwndEdit    - Handle to edit control
//   needle      - Text to search for
//   replacement - Text to replace with
//   matchCase   - TRUE for case-sensitive search
// Returns: Number of replacements made
// ============================================================================
static int ReplaceAllOccurrences(HWND hwndEdit, const WCHAR *needle, const WCHAR *replacement, BOOL matchCase) {
    // Validate search string
    if (!needle || needle[0] == L'\0') return 0;

    // Get all text from edit control
    WCHAR *text = NULL;
    int len = 0;
    if (!GetEditText(hwndEdit, &text, &len)) return 0;

    size_t needleLen = wcslen(needle);
    size_t replLen = replacement ? wcslen(replacement) : 0;

    // Create separate buffers for case-insensitive comparison
    // searchBuf: lowercase copy for searching (if case-insensitive)
    // needleBuf: lowercase copy of search string (if case-insensitive)
    WCHAR *searchBuf = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    WCHAR *needleBuf = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (needleLen + 1) * sizeof(WCHAR));
    if (!searchBuf || !needleBuf) {
        HeapFree(GetProcessHeap(), 0, text);
        if (searchBuf) HeapFree(GetProcessHeap(), 0, searchBuf);
        if (needleBuf) HeapFree(GetProcessHeap(), 0, needleBuf);
        return 0;
    }
    StringCchCopyW(searchBuf, len + 1, text);
    StringCchCopyW(needleBuf, needleLen + 1, needle);

    // Convert to lowercase for case-insensitive search
    if (!matchCase) {
        CharLowerBuffW(searchBuf, len);
        CharLowerBuffW(needleBuf, (DWORD)needleLen);
    }

    // First pass: Count occurrences to calculate result buffer size
    int count = 0;
    WCHAR *p = searchBuf;
    while ((p = wcsstr(p, needleBuf)) != NULL) {
        count++;
        p += needleLen;
    }
    
    // Nothing to replace
    if (count == 0) {
        HeapFree(GetProcessHeap(), 0, text);
        HeapFree(GetProcessHeap(), 0, searchBuf);
        HeapFree(GetProcessHeap(), 0, needleBuf);
        return 0;
    }

    // Calculate new length: original - (count * oldLen) + (count * newLen)
    size_t newLen = (size_t)len - (size_t)count * needleLen + (size_t)count * replLen;
    WCHAR *result = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (newLen + 1) * sizeof(WCHAR));
    if (!result) {
        HeapFree(GetProcessHeap(), 0, text);
        HeapFree(GetProcessHeap(), 0, searchBuf);
        HeapFree(GetProcessHeap(), 0, needleBuf);
        return 0;
    }

    // Second pass: Build result string with replacements
    // We iterate through searchBuf to find matches, but copy from origCur (text)
    // to preserve original case for non-matching parts
    WCHAR *dst = result;          // Destination pointer in result buffer
    WCHAR *searchCur = searchBuf; // Current position in lowercase search buffer
    WCHAR *origCur = text;        // Current position in original text
    
    while ((p = wcsstr(searchCur, needleBuf)) != NULL) {
        // Copy everything before the match
        size_t delta = (size_t)(p - searchCur);
        CopyMemory(dst, origCur, delta * sizeof(WCHAR));
        dst += delta;
        origCur += delta;
        searchCur += delta;

        // Insert replacement text
        if (replLen) {
            CopyMemory(dst, replacement, replLen * sizeof(WCHAR));
            dst += replLen;
        }
        
        // Skip past the matched text in original
        origCur += needleLen;
        searchCur += needleLen;
    }
    
    // Copy any remaining text after last match
    size_t tail = wcslen(origCur);
    CopyMemory(dst, origCur, tail * sizeof(WCHAR));
    dst += tail;
    *dst = L'\0';  // Null-terminate the result

    // Update the edit control with new text
    SetWindowTextW(hwndEdit, result);
    
    // Clean up all allocated memory
    HeapFree(GetProcessHeap(), 0, text);
    HeapFree(GetProcessHeap(), 0, searchBuf);
    HeapFree(GetProcessHeap(), 0, needleBuf);
    HeapFree(GetProcessHeap(), 0, result);
    
    // Mark document as modified
    SendMessageW(hwndEdit, EM_SETMODIFY, TRUE, 0);
    g_app.modified = TRUE;
    UpdateTitle(g_app.hwndMain);
    return count;
}

// ============================================================================
// UpdateTitle - Update Window Title Bar
// ============================================================================
// Sets the window title to show:
// - "*" prefix if document is modified (unsaved changes)
// - Filename (or "Untitled" for new documents)
// - Application name ("retropad")
// Example: "*Document.txt - retropad"
// ============================================================================
static void UpdateTitle(HWND hwnd) {
    WCHAR name[MAX_PATH_BUFFER];
    
    // Extract just the filename from the full path
    if (g_app.currentPath[0]) {
        // Find last backslash to get filename part
        WCHAR *fileName = wcsrchr(g_app.currentPath, L'\\');
        fileName = fileName ? fileName + 1 : g_app.currentPath;
        StringCchCopyW(name, MAX_PATH_BUFFER, fileName);
    } else {
        // No file path = new unsaved document
        StringCchCopyW(name, MAX_PATH_BUFFER, UNTITLED_NAME);
    }

    // Build title: "[*]filename - retropad"
    WCHAR title[MAX_PATH_BUFFER + 32];
    StringCchPrintfW(title, ARRAYSIZE(title), L"%s%s - %s", (g_app.modified ? L"*" : L""), name, APP_TITLE);
    SetWindowTextW(hwnd, title);
}

// ============================================================================
// ApplyFontToEdit - Set Font for Edit Control
// ============================================================================
// Applies a font to the edit control, causing it to redraw with new font.
// ============================================================================
static void ApplyFontToEdit(HWND hwndEdit, HFONT font) {
    // WM_SETFONT with TRUE forces immediate redraw
    SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)font, TRUE);
}

// ============================================================================
// CreateEditControl - Create or Recreate the Edit Control
// ============================================================================
// Creates the main multi-line edit control that serves as the text editor.
// This function is called:
// - On startup to create the initial control
// - When toggling word wrap (requires recreating with different styles)
// The function destroys the old control if it exists, creates a new one with
// appropriate styles, and resizes it to fill the available space.
// ============================================================================
static void CreateEditControl(HWND hwnd) {
    // Destroy existing edit control if present
    if (g_app.hwndEdit) {
        DestroyWindow(g_app.hwndEdit);
    }

    // Build style flags for the edit control
    // Base styles:
    //   WS_CHILD - Child window of main window
    //   WS_VISIBLE - Visible by default
    //   WS_VSCROLL - Vertical scrollbar
    //   ES_MULTILINE - Multiple lines (essential for text editor)
    //   ES_AUTOVSCROLL - Auto-scroll vertically when typing past end
    //   ES_WANTRETURN - Enter key adds newline (vs. default button)
    //   ES_NOHIDESEL - Show selection even when control loses focus
    DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL;
    
    // Add horizontal scroll only when word wrap is off
    if (!g_app.wordWrap) {
        style |= WS_HSCROLL | ES_AUTOHSCROLL;
    }

    // Create the edit control
    // WS_EX_CLIENTEDGE gives it a sunken 3D border
    // "EDIT" is the Windows built-in edit control class
    g_app.hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL, style, 0, 0, 0, 0, hwnd, (HMENU)1, g_hInst, NULL);
    
    // Apply current font if one is set
    if (g_app.hwndEdit && g_app.hFont) {
        ApplyFontToEdit(g_app.hwndEdit, g_app.hFont);
    }
    
    // Remove text length limit (default is ~32KB) to allow large files
    SendMessageW(g_app.hwndEdit, EM_SETLIMITTEXT, 0, 0);
    
    // Position and size the edit control
    UpdateLayout(hwnd);
}

// ============================================================================
// ToggleStatusBar - Show or Hide the Status Bar
// ============================================================================
// Controls the visibility of the status bar at the bottom of the window.
// When shown, the status bar displays line number, column, and total lines.
// ============================================================================
static void ToggleStatusBar(HWND hwnd, BOOL visible) {
    g_app.statusVisible = visible;
    if (visible) {
        // Create status bar if it doesn't exist
        if (!g_app.hwndStatus) {
            // SBARS_SIZEGRIP adds the resize grip in bottom-right corner
            g_app.hwndStatus = CreateStatusWindowW(WS_CHILD | SBARS_SIZEGRIP, L"", hwnd, 2);
        }
        ShowWindow(g_app.hwndStatus, SW_SHOW);
    } else if (g_app.hwndStatus) {
        ShowWindow(g_app.hwndStatus, SW_HIDE);
    }
    // Resize edit control to account for status bar visibility
    UpdateLayout(hwnd);
    UpdateStatusBar(hwnd);
}

// ============================================================================
// UpdateLayout - Resize Child Windows to Fit Parent
// ============================================================================
// Calculates and applies proper sizes and positions for the edit control and
// status bar based on the current window size. Called when:
// - Window is resized
// - Status bar visibility changes
// - Edit control is recreated (e.g., word wrap toggle)
// ============================================================================
static void UpdateLayout(HWND hwnd) {
    // Get the client area size of main window
    RECT rc;
    GetClientRect(hwnd, &rc);

    int statusHeight = 0;
    if (g_app.statusVisible && g_app.hwndStatus) {
        // Tell status bar to resize itself (calculates its own height)
        SendMessageW(g_app.hwndStatus, WM_SIZE, 0, 0);
        // Get the calculated height
        RECT sbrc;
        GetWindowRect(g_app.hwndStatus, &sbrc);
        statusHeight = sbrc.bottom - sbrc.top;
        // Position status bar at bottom of window
        MoveWindow(g_app.hwndStatus, 0, rc.bottom - statusHeight, rc.right, statusHeight, TRUE);
    }

    // Resize edit control to fill remaining space (excluding status bar)
    if (g_app.hwndEdit) {
        MoveWindow(g_app.hwndEdit, 0, 0, rc.right, rc.bottom - statusHeight, TRUE);
    }
}

// ============================================================================
// PromptSaveChanges - Ask User to Save Unsaved Changes
// ============================================================================
// If the document has been modified, shows a Yes/No/Cancel dialog asking
// if the user wants to save changes. Used before:
// - Opening a new file
// - Creating a new document
// - Closing the application
// Returns: TRUE if safe to proceed (saved, or no changes, or user said no),
//          FALSE if user cancelled (wants to go back)
// ============================================================================
static BOOL PromptSaveChanges(HWND hwnd) {
    // If not modified, safe to proceed
    if (!g_app.modified) return TRUE;

    // Build prompt message with filename
    WCHAR prompt[MAX_PATH_BUFFER + 64];
    const WCHAR *name = g_app.currentPath[0] ? g_app.currentPath : UNTITLED_NAME;
    StringCchPrintfW(prompt, ARRAYSIZE(prompt), L"Do you want to save changes to %s?", name);
    
    // Show Yes/No/Cancel dialog
    int res = MessageBoxW(hwnd, prompt, APP_TITLE, MB_ICONQUESTION | MB_YESNOCANCEL);
    if (res == IDYES) {
        // Yes: Try to save, return TRUE only if save succeeds
        return DoFileSave(hwnd, FALSE);
    }
    // No: Discard changes and proceed
    // Cancel: Return FALSE to abort operation
    return res == IDNO;
}

// ============================================================================
// LoadDocumentFromPath - Load File into Editor
// ============================================================================
// Loads a text file from the specified path, detects its encoding, and
// displays it in the edit control. Updates the application state with the
// new file path and encoding.
// Parameters:
//   hwnd - Main window handle
//   path - Full path to file to load
// Returns: TRUE on success, FALSE on failure
// ============================================================================
static BOOL LoadDocumentFromPath(HWND hwnd, LPCWSTR path) {
    // Load file using file_io module (handles encoding detection)
    WCHAR *text = NULL;
    TextEncoding enc = ENC_UTF8;
    if (!LoadTextFile(hwnd, path, &text, NULL, &enc)) {
        return FALSE;  // Error message already shown by LoadTextFile
    }

    // Set the loaded text into edit control
    SetWindowTextW(g_app.hwndEdit, text);
    HeapFree(GetProcessHeap(), 0, text);  // Free the loaded text buffer
    
    // Update application state with new file info
    StringCchCopyW(g_app.currentPath, ARRAYSIZE(g_app.currentPath), path);
    g_app.encoding = enc;
    
    // Mark document as unmodified (just loaded)
    SendMessageW(g_app.hwndEdit, EM_SETMODIFY, FALSE, 0);
    g_app.modified = FALSE;
    
    // Update UI to reflect new document
    UpdateTitle(hwnd);
    UpdateStatusBar(hwnd);
    return TRUE;
}

// ============================================================================
// DoFileOpen - Open File Dialog and Load Selected File
// ============================================================================
// Shows the Open File dialog, and if user selects a file, loads it into
// the editor. First prompts to save any unsaved changes.
// Returns: TRUE if file opened successfully, FALSE otherwise
// ============================================================================
static BOOL DoFileOpen(HWND hwnd) {
    // Check if user wants to save current changes first
    if (!PromptSaveChanges(hwnd)) return FALSE;

    // Show Open File dialog
    WCHAR path[MAX_PATH_BUFFER] = L"";
    if (!OpenFileDialog(hwnd, path, ARRAYSIZE(path))) {
        return FALSE;  // User cancelled
    }
    
    // Load the selected file
    return LoadDocumentFromPath(hwnd, path);
}

// ============================================================================
// DoFileSave - Save Current Document
// ============================================================================
// Saves the current document. If saveAs is TRUE or no file path exists,
// shows the Save As dialog. Otherwise saves to the current path.
// Parameters:
//   hwnd   - Main window handle
//   saveAs - TRUE to force "Save As" dialog, FALSE for regular save
// Returns: TRUE if saved successfully, FALSE on failure or cancel
// ============================================================================
static BOOL DoFileSave(HWND hwnd, BOOL saveAs) {
    WCHAR path[MAX_PATH_BUFFER];
    
    // Determine if we need to show Save As dialog
    if (saveAs || g_app.currentPath[0] == L'\0') {
        // Save As: Show dialog with current filename as default
        path[0] = L'\0';
        if (g_app.currentPath[0]) {
            StringCchCopyW(path, ARRAYSIZE(path), g_app.currentPath);
        }
        if (!SaveFileDialog(hwnd, path, ARRAYSIZE(path))) {
            return FALSE;  // User cancelled
        }
        // Update current path with new filename
        StringCchCopyW(g_app.currentPath, ARRAYSIZE(g_app.currentPath), path);
    } else {
        // Regular Save: Use existing path
        StringCchCopyW(path, ARRAYSIZE(path), g_app.currentPath);
    }

    // Get text from edit control
    int len = GetWindowTextLengthW(g_app.hwndEdit);
    WCHAR *buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    if (!buffer) return FALSE;
    GetWindowTextW(g_app.hwndEdit, buffer, len + 1);

    // Save using file_io module (preserves encoding)
    BOOL ok = SaveTextFile(hwnd, path, buffer, len, g_app.encoding);
    HeapFree(GetProcessHeap(), 0, buffer);
    
    if (ok) {
        // Mark document as unmodified
        SendMessageW(g_app.hwndEdit, EM_SETMODIFY, FALSE, 0);
        g_app.modified = FALSE;
        UpdateTitle(hwnd);
    }
    return ok;
}

// ============================================================================
// DoFileNew - Create New Document
// ============================================================================
// Clears the editor and starts a new document. Prompts to save any unsaved
// changes first. Resets the file path and encoding to defaults.
// ============================================================================
static void DoFileNew(HWND hwnd) {
    // Check if user wants to save current document
    if (!PromptSaveChanges(hwnd)) return;
    
    // Clear the editor
    SetWindowTextW(g_app.hwndEdit, L"");
    
    // Reset file state to defaults
    g_app.currentPath[0] = L'\0';  // Empty = "Untitled"
    g_app.encoding = ENC_UTF8;     // Default encoding
    
    // Mark as unmodified
    SendMessageW(g_app.hwndEdit, EM_SETMODIFY, FALSE, 0);
    g_app.modified = FALSE;
    
    // Update UI
    UpdateTitle(hwnd);
    UpdateStatusBar(hwnd);
}

// ============================================================================
// LoadWordWrapSetting - Load Word Wrap Setting from Registry
// ============================================================================
// Reads the saved word wrap preference from the Windows registry.
// Returns TRUE if word wrap was previously enabled, FALSE otherwise.
// Default is FALSE if no setting exists.
// ============================================================================
static BOOL LoadWordWrapSetting(void) {
    HKEY hKey = NULL;
    BOOL wordWrap = FALSE;  // Default to OFF
    
    // Try to open registry key
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return wordWrap;  // Key doesn't exist or can't be opened
    }
    
    // Read the word wrap setting
    DWORD value = 0;
    DWORD size = sizeof(DWORD);
    DWORD type = REG_DWORD;
    
    if (RegQueryValueExW(hKey, REG_WORD_WRAP, NULL, &type, (BYTE*)&value, &size) == ERROR_SUCCESS) {
        wordWrap = (value != 0);
    }
    
    // Always close the key before returning
    RegCloseKey(hKey);
    return wordWrap;
}

// ============================================================================
// SaveWordWrapSetting - Save Word Wrap Setting to Registry
// ============================================================================
// Saves the current word wrap preference to the Windows registry so it
// persists across application sessions.
// ============================================================================
static void SaveWordWrapSetting(BOOL enabled) {
    HKEY hKey;
    
    // Create or open registry key
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY_PATH, 0, NULL, 0, 
                        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = enabled ? 1 : 0;
        
        // Write the word wrap setting
        RegSetValueExW(hKey, REG_WORD_WRAP, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        
        RegCloseKey(hKey);
    }
}

// ============================================================================
// SetWordWrap - Toggle Word Wrap Mode
// ============================================================================
// Enables or disables word wrap in the editor. When word wrap changes:
// - Edit control must be recreated (different styles required)
// - Text and cursor position are preserved
// - "Go To" is disabled when word wrap is ON (line numbers change with wrapping)
// - Status bar remains visible and shows current position
// ============================================================================
static void SetWordWrap(HWND hwnd, BOOL enabled) {
    // Nothing to do if already in desired state
    if (g_app.wordWrap == enabled) return;
    
    g_app.wordWrap = enabled;
    HWND edit = g_app.hwndEdit;
    
    // Save current text and cursor position
    WCHAR *text = NULL;
    int len = 0;
    if (!GetEditText(edit, &text, &len)) {
        return;
    }
    DWORD start = 0, end = 0;
    SendMessageW(edit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    // Recreate edit control with new word wrap setting
    CreateEditControl(hwnd);
    
    // Restore text and cursor position
    SetWindowTextW(g_app.hwndEdit, text);
    SendMessageW(g_app.hwndEdit, EM_SETSEL, start, end);
    HeapFree(GetProcessHeap(), 0, text);

    if (enabled) {
        // Word wrap ON: Disable "Go To" (line numbers change with wrapping)
        EnableMenuItem(GetMenu(hwnd), IDM_EDIT_GOTO, MF_BYCOMMAND | MF_GRAYED);
    } else {
        // Word wrap OFF: Re-enable "Go To"
        EnableMenuItem(GetMenu(hwnd), IDM_EDIT_GOTO, MF_BYCOMMAND | MF_ENABLED);
    }
    
    // Save word wrap preference to registry
    SaveWordWrapSetting(enabled);
    
    UpdateTitle(hwnd);
    UpdateStatusBar(hwnd);
}

// ============================================================================
// UpdateStatusBar - Refresh Status Bar with Current Position Info
// ============================================================================
// Updates the status bar to show:
// - Current line number (Ln)
// - Current column number (Col)
// - Total number of lines in document
// Called whenever cursor moves or text changes.
// ============================================================================
static void UpdateStatusBar(HWND hwnd) {
    UNREFERENCED_PARAMETER(hwnd);
    
    // Do nothing if status bar is hidden
    if (!g_app.statusVisible || !g_app.hwndStatus) return;
    
    // Get current selection/cursor position
    DWORD selStart = 0, selEnd = 0;
    SendMessageW(g_app.hwndEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    
    // Calculate line number (1-based)
    int line = (int)SendMessageW(g_app.hwndEdit, EM_LINEFROMCHAR, selStart, 0) + 1;
    
    // Calculate column number within line (1-based)
    // EM_LINEINDEX gets character position of start of line
    int col = (int)(selStart - SendMessageW(g_app.hwndEdit, EM_LINEINDEX, line - 1, 0)) + 1;
    
    // Get total line count
    int lines = (int)SendMessageW(g_app.hwndEdit, EM_GETLINECOUNT, 0, 0);

    // Format and display status text
    WCHAR status[128];
    StringCchPrintfW(status, ARRAYSIZE(status), L"Ln %d, Col %d    Lines: %d", line, col, lines);
    SendMessageW(g_app.hwndStatus, SB_SETTEXT, 0, (LPARAM)status);
}

// ============================================================================
// ShowFindDialog - Display the Find Dialog
// ============================================================================
// Shows the Windows common Find dialog (modeless). If dialog is already open,
// brings it to the foreground. The dialog communicates results through a
// registered window message handled by HandleFindReplace().
// ============================================================================
static void ShowFindDialog(HWND hwnd) {
    // If Find dialog is already open, just bring it to front
    if (g_app.hFindDlg) {
        SetForegroundWindow(g_app.hFindDlg);
        return;
    }

    // Initialize FINDREPLACE structure
    ZeroMemory(&g_app.find, sizeof(g_app.find));
    g_app.find.lStructSize = sizeof(FINDREPLACEW);
    g_app.find.hwndOwner = hwnd;                     // Parent window
    g_app.find.lpstrFindWhat = g_app.findText;       // Buffer for search text
    g_app.find.wFindWhatLen = ARRAYSIZE(g_app.findText);
    g_app.find.Flags = g_app.findFlags;              // Restore previous flags

    // Create modeless Find dialog
    // Dialog sends messages to parent via registered g_findMsg
    g_app.hFindDlg = FindTextW(&g_app.find);
}

// ============================================================================
// ShowReplaceDialog - Display the Find/Replace Dialog
// ============================================================================
// Shows the Windows common Replace dialog (modeless). Similar to Find dialog
// but includes a replacement text field and Replace/Replace All buttons.
// ============================================================================
static void ShowReplaceDialog(HWND hwnd) {
    // If Replace dialog is already open, just bring it to front
    if (g_app.hReplaceDlg) {
        SetForegroundWindow(g_app.hReplaceDlg);
        return;
    }

    // Initialize FINDREPLACE structure with both find and replace buffers
    ZeroMemory(&g_app.find, sizeof(g_app.find));
    g_app.find.lStructSize = sizeof(FINDREPLACEW);
    g_app.find.hwndOwner = hwnd;
    g_app.find.lpstrFindWhat = g_app.findText;         // Search text buffer
    g_app.find.lpstrReplaceWith = g_app.replaceText;   // Replacement text buffer
    g_app.find.wFindWhatLen = ARRAYSIZE(g_app.findText);
    g_app.find.wReplaceWithLen = ARRAYSIZE(g_app.replaceText);
    g_app.find.Flags = g_app.findFlags;                // Restore previous flags

    // Create modeless Replace dialog
    g_app.hReplaceDlg = ReplaceTextW(&g_app.find);
}

// ============================================================================
// DoFindNext - Find Next Occurrence (F3 Key)
// ============================================================================
// Finds the next occurrence of the last search string. If no search has been
// performed yet, opens the Find dialog. Supports reverse searching.
// Parameters:
//   reverse - TRUE to search backward, FALSE to search forward
// Returns: TRUE if found, FALSE if not found or no search string
// ============================================================================
static BOOL DoFindNext(BOOL reverse) {
    // If no previous search, open Find dialog
    if (g_app.findText[0] == L'\0') {
        ShowFindDialog(g_app.hwndMain);
        return FALSE;
    }

    // Get current selection (cursor position)
    DWORD start = 0, end = 0;
    SendMessageW(g_app.hwndEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    // Extract flags
    BOOL matchCase = (g_app.findFlags & FR_MATCHCASE) != 0;
    BOOL down = (g_app.findFlags & FR_DOWN) != 0;
    
    // Reverse search direction if requested (Shift+F3)
    if (reverse) down = !down;
    
    // Start searching from end of selection (forward) or start (backward)
    DWORD searchStart = down ? end : start;
    DWORD outStart = 0, outEnd = 0;
    
    // Perform the search
    if (FindInEdit(g_app.hwndEdit, g_app.findText, matchCase, down, searchStart, &outStart, &outEnd)) {
        // Found: Select the found text
        SendMessageW(g_app.hwndEdit, EM_SETSEL, outStart, outEnd);
        // Scroll to make selection visible
        SendMessageW(g_app.hwndEdit, EM_SCROLLCARET, 0, 0);
        return TRUE;
    }
    
    // Not found: Show message
    MessageBoxW(g_app.hwndMain, L"Cannot find the text.", APP_TITLE, MB_ICONINFORMATION);
    return FALSE;
}

// ============================================================================
// GoToDlgProc - Dialog Procedure for "Go To Line" Dialog
// ============================================================================
// Handles messages for the Go To Line dialog box. Allows user to enter a
// line number and jump to that line in the editor.
// ============================================================================
static INT_PTR CALLBACK GoToDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (msg) {
    // Initialize dialog when first shown
    case WM_INITDIALOG: {
        // Set default line number to 1
        SetDlgItemInt(dlg, IDC_GOTO_EDIT, 1, FALSE);
        // Limit input to 10 digits (max line number)
        HWND edit = GetDlgItem(dlg, IDC_GOTO_EDIT);
        SendMessageW(edit, EM_SETLIMITTEXT, 10, 0);
        return TRUE;
    }
    
    // Handle button clicks
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            // Get line number from edit control
            BOOL ok = FALSE;
            UINT line = GetDlgItemInt(dlg, IDC_GOTO_EDIT, &ok, FALSE);
            
            // Validate input
            if (!ok || line == 0) {
                MessageBoxW(dlg, L"Enter a valid line number.", APP_TITLE, MB_ICONWARNING);
                return TRUE;
            }
            
            // Clamp to valid range (1 to maxLine)
            int maxLine = (int)SendMessageW(g_app.hwndEdit, EM_GETLINECOUNT, 0, 0);
            if ((int)line > maxLine) line = (UINT)maxLine;
            
            // Get character index for start of requested line
            // EM_LINEINDEX: line number -> character position
            int charIndex = (int)SendMessageW(g_app.hwndEdit, EM_LINEINDEX, line - 1, 0);
            if (charIndex >= 0) {
                // Move cursor to start of line
                SendMessageW(g_app.hwndEdit, EM_SETSEL, charIndex, charIndex);
                // Scroll to make cursor visible
                SendMessageW(g_app.hwndEdit, EM_SCROLLCARET, 0, 0);
            }
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// ============================================================================
// DoSelectFont - Show Font Selection Dialog
// ============================================================================
// Opens the Windows common font chooser dialog and applies the selected font
// to the edit control. Preserves current font settings as defaults.
// ============================================================================
static void DoSelectFont(HWND hwnd) {
    UNREFERENCED_PARAMETER(hwnd);
    
    // Get current font settings to use as defaults in dialog
    LOGFONTW lf = {0};
    if (g_app.hFont) {
        // Use current editor font
        GetObjectW(g_app.hFont, sizeof(LOGFONTW), &lf);
    } else {
        // No font set yet, use system default
        SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &lf, 0);
    }

    // Initialize font chooser dialog
    CHOOSEFONTW cf = {0};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hwnd;
    cf.lpLogFont = &lf;  // LOGFONT structure to fill
    // CF_SCREENFONTS: Show screen fonts only (not printer fonts)
    // CF_INITTOLOGFONTSTRUCT: Initialize dialog with values in lf
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;

    // Show dialog and check if user clicked OK
    if (ChooseFontW(&cf)) {
        // Create GDI font object from selected LOGFONT
        HFONT newFont = CreateFontIndirectW(&lf);
        if (newFont) {
            // Delete old font to avoid resource leak
            if (g_app.hFont) DeleteObject(g_app.hFont);
            // Apply new font
            g_app.hFont = newFont;
            ApplyFontToEdit(g_app.hwndEdit, g_app.hFont);
            // Redraw to show new font
            UpdateLayout(hwnd);
        }
    }
}

// ============================================================================
// InsertTimeDate - Insert Current Time and Date at Cursor (F5)
// ============================================================================
// Inserts the current system time and date at the cursor position using
// the user's locale settings for formatting.
// ============================================================================
static void InsertTimeDate(HWND hwnd) {
    UNREFERENCED_PARAMETER(hwnd);
    
    // Get current local time
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Format time and date using user's locale preferences
    WCHAR date[64], time[64], stamp[128];
    // DATE_SHORTDATE: e.g., "12/1/2025"
    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, ARRAYSIZE(date));
    // TIME_NOSECONDS: e.g., "8:30 PM" (without seconds)
    GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, time, ARRAYSIZE(time));
    
    // Combine: "8:30 PM 12/1/2025"
    StringCchPrintfW(stamp, ARRAYSIZE(stamp), L"%s %s", time, date);
    
    // Insert at cursor position (replaces selection if any)
    // TRUE parameter marks as undoable
    SendMessageW(g_app.hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)stamp);
}

// ============================================================================
// HandleFindReplace - Process Messages from Find/Replace Dialogs
// ============================================================================
// Handles the custom messages sent by the modeless Find and Replace dialogs.
// This function processes:
// - FR_DIALOGTERM: Dialog is closing
// - FR_FINDNEXT: Find next occurrence
// - FR_REPLACE: Replace current selection
// - FR_REPLACEALL: Replace all occurrences
// Parameters:
//   lpfr - Pointer to FINDREPLACE structure with operation details
// ============================================================================
static void HandleFindReplace(LPFINDREPLACE lpfr) {
    // Check if dialog is closing
    if (lpfr->Flags & FR_DIALOGTERM) {
        // Clear dialog handles so we know they're closed
        g_app.hFindDlg = NULL;
        g_app.hReplaceDlg = NULL;
        return;
    }

    // Save flags and strings for future searches
    g_app.findFlags = lpfr->Flags;
    if (lpfr->lpstrFindWhat && lpfr->lpstrFindWhat[0]) {
        StringCchCopyW(g_app.findText, ARRAYSIZE(g_app.findText), lpfr->lpstrFindWhat);
    }
    if (lpfr->lpstrReplaceWith) {
        StringCchCopyW(g_app.replaceText, ARRAYSIZE(g_app.replaceText), lpfr->lpstrReplaceWith);
    }

    // Extract search options
    BOOL matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
    BOOL down = (lpfr->Flags & FR_DOWN) != 0;

    // Handle "Find Next" button
    if (lpfr->Flags & FR_FINDNEXT) {
        DWORD start = 0, end = 0;
        SendMessageW(g_app.hwndEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        DWORD searchStart = down ? end : start;  // Search from after selection
        DWORD outStart = 0, outEnd = 0;
        if (FindInEdit(g_app.hwndEdit, g_app.findText, matchCase, down, searchStart, &outStart, &outEnd)) {
            // Found: Select the match
            SendMessageW(g_app.hwndEdit, EM_SETSEL, outStart, outEnd);
            SendMessageW(g_app.hwndEdit, EM_SCROLLCARET, 0, 0);
        } else {
            // Not found
            MessageBoxW(g_app.hwndMain, L"Cannot find the text.", APP_TITLE, MB_ICONINFORMATION);
        }
    }
    // Handle "Replace" button (replace current selection only)
    else if (lpfr->Flags & FR_REPLACE) {
        DWORD start = 0, end = 0;
        SendMessageW(g_app.hwndEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        DWORD outStart = 0, outEnd = 0;
        // Find the match at current position
        if (FindInEdit(g_app.hwndEdit, g_app.findText, matchCase, down, start, &outStart, &outEnd)) {
            // Select the match
            SendMessageW(g_app.hwndEdit, EM_SETSEL, outStart, outEnd);
            // Replace with new text (TRUE makes it undoable)
            SendMessageW(g_app.hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)g_app.replaceText);
            SendMessageW(g_app.hwndEdit, EM_SCROLLCARET, 0, 0);
            g_app.modified = TRUE;
            UpdateTitle(g_app.hwndMain);
        } else {
            MessageBoxW(g_app.hwndMain, L"Cannot find the text.", APP_TITLE, MB_ICONINFORMATION);
        }
    }
    // Handle "Replace All" button
    else if (lpfr->Flags & FR_REPLACEALL) {
        // Replace all occurrences in entire document
        int replaced = ReplaceAllOccurrences(g_app.hwndEdit, g_app.findText, g_app.replaceText, matchCase);
        // Show result count
        WCHAR msg[64];
        StringCchPrintfW(msg, ARRAYSIZE(msg), L"Replaced %d occurrence%s.", replaced, replaced == 1 ? L"" : L"s");
        MessageBoxW(g_app.hwndMain, msg, APP_TITLE, MB_OK | MB_ICONINFORMATION);
    }
}

// ============================================================================
// UpdateMenuStates - Update Menu Item States Before Display
// ============================================================================
// Called before menus are shown to update checkmarks and enable/disable states
// based on current application state. Updates:
// - Word Wrap checkmark
// - Status Bar checkmark  
// - Go To enabled/disabled (based on word wrap)
// - Save enabled/disabled (based on modified flag)
// ============================================================================
static void UpdateMenuStates(HWND hwnd) {
    HMENU menu = GetMenu(hwnd);
    if (!menu) return;

    // Update checkmarks for toggle menu items
    UINT wrapState = g_app.wordWrap ? MF_CHECKED : MF_UNCHECKED;
    UINT statusState = g_app.statusVisible ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem(menu, IDM_FORMAT_WORD_WRAP, MF_BYCOMMAND | wrapState);
    CheckMenuItem(menu, IDM_VIEW_STATUS_BAR, MF_BYCOMMAND | statusState);

    // "Go To" is only available when word wrap is OFF
    // (line numbers change with word wrap)
    BOOL canGoTo = !g_app.wordWrap;
    EnableMenuItem(menu, IDM_EDIT_GOTO, MF_BYCOMMAND | (canGoTo ? MF_ENABLED : MF_GRAYED));

    // "Save" enabled only if document has been modified
    BOOL modified = (SendMessageW(g_app.hwndEdit, EM_GETMODIFY, 0, 0) != 0);
    EnableMenuItem(menu, IDM_FILE_SAVE, MF_BYCOMMAND | (modified ? MF_ENABLED : MF_GRAYED));
}

// ============================================================================
// DoPageSetup - Configure Page Setup for Printing
// ============================================================================
// Displays the Windows Page Setup dialog allowing the user to configure
// margins, paper size, orientation, and printer for printing documents.
// Settings are stored in g_app.pageSetup and persist across sessions.
// ============================================================================
static void DoPageSetup(HWND hwnd) {
    UNREFERENCED_PARAMETER(hwnd);
    
    // Initialize page setup structure if first time
    if (g_app.pageSetup.lStructSize == 0) {
        ZeroMemory(&g_app.pageSetup, sizeof(PAGESETUPDLGW));
        g_app.pageSetup.lStructSize = sizeof(PAGESETUPDLGW);
        g_app.pageSetup.hwndOwner = g_app.hwndMain;
        g_app.pageSetup.Flags = PSD_MARGINS | PSD_INWININIINTLMEASURE;
        // Default margins: 0.75 inches (1000 = 1 inch)
        g_app.pageSetup.rtMargin.left = 750;
        g_app.pageSetup.rtMargin.top = 1000;
        g_app.pageSetup.rtMargin.right = 750;
        g_app.pageSetup.rtMargin.bottom = 1000;
    }
    
    // Show page setup dialog
    PageSetupDlgW(&g_app.pageSetup);
}

// ============================================================================
// DoPrint - Print the Current Document
// ============================================================================
// Displays the Windows Print dialog and prints the document contents.
// Handles text pagination, header/footer, and print job management.
// ============================================================================
static void DoPrint(HWND hwnd) {
    UNREFERENCED_PARAMETER(hwnd);
    
    // Get text from edit control
    WCHAR *text = NULL;
    int len = 0;
    if (!GetEditText(g_app.hwndEdit, &text, &len)) {
        MessageBoxW(hwnd, L"Unable to get text for printing.", APP_TITLE, MB_ICONERROR);
        return;
    }
    
    // Initialize print dialog if first time
    if (g_app.printDlg.lStructSize == 0) {
        ZeroMemory(&g_app.printDlg, sizeof(PRINTDLGW));
        g_app.printDlg.lStructSize = sizeof(PRINTDLGW);
        g_app.printDlg.hwndOwner = g_app.hwndMain;
        g_app.printDlg.Flags = PD_RETURNDC | PD_ALLPAGES | PD_USEDEVMODECOPIESANDCOLLATE;
        g_app.printDlg.nCopies = 1;
        g_app.printDlg.nFromPage = 1;
        g_app.printDlg.nToPage = 1;
        g_app.printDlg.nMinPage = 1;
        g_app.printDlg.nMaxPage = 0xFFFF;
    }
    
    // Show print dialog
    if (!PrintDlgW(&g_app.printDlg)) {
        free(text);
        return; // User cancelled
    }
    
    HDC hdc = g_app.printDlg.hDC;
    if (!hdc) {
        MessageBoxW(hwnd, L"Unable to get printer device context.", APP_TITLE, MB_ICONERROR);
        free(text);
        return;
    }
    
    // Get document name for print job
    WCHAR docName[MAX_PATH];
    if (g_app.currentPath[0]) {
        // Use filename if document is saved
        WCHAR *filename = wcsrchr(g_app.currentPath, L'\\');
        StringCchCopyW(docName, MAX_PATH, filename ? filename + 1 : g_app.currentPath);
    } else {
        StringCchCopyW(docName, MAX_PATH, UNTITLED_NAME);
    }
    
    // Start print job
    DOCINFOW di = {0};
    di.cbSize = sizeof(DOCINFOW);
    di.lpszDocName = docName;
    
    if (StartDocW(hdc, &di) <= 0) {
        MessageBoxW(hwnd, L"Unable to start print job.", APP_TITLE, MB_ICONERROR);
        DeleteDC(hdc);
        free(text);
        return;
    }
    
    // Get printer capabilities
    int pageHeight = GetDeviceCaps(hdc, VERTRES);
    int logPixelsY = GetDeviceCaps(hdc, LOGPIXELSY);
    
    // Calculate margins (convert from 1/1000 inch to pixels)
    int leftMargin = (g_app.pageSetup.rtMargin.left * logPixelsY) / 1000;
    int topMargin = (g_app.pageSetup.rtMargin.top * logPixelsY) / 1000;
    int bottomMargin = (g_app.pageSetup.rtMargin.bottom * logPixelsY) / 1000;
    
    // Calculate printable area
    int printHeight = pageHeight - topMargin - bottomMargin;
    
    // Select font for printing  
    HFONT hPrintFont = g_app.hFont ? g_app.hFont : (HFONT)GetStockObject(SYSTEM_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hPrintFont);
    
    // Get font metrics
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    int linesPerPage = printHeight / lineHeight;
    
    if (linesPerPage < 1) linesPerPage = 1;
    
    // Print the text page by page
    WCHAR *line = text;
    int lineCount = 0;
    BOOL inPage = FALSE;
    
    while (*line) {
        // Start new page if needed
        if (!inPage) {
            if (StartPage(hdc) <= 0) break;
            inPage = TRUE;
            lineCount = 0;
        }
        
        // Find end of line
        WCHAR *lineEnd = line;
        while (*lineEnd && *lineEnd != L'\r' && *lineEnd != L'\n') lineEnd++;
        
        // Print this line
        int lineLen = (int)(lineEnd - line);
        TextOutW(hdc, leftMargin, topMargin + (lineCount * lineHeight), line, lineLen);
        
        lineCount++;
        
        // Move to next line
        line = lineEnd;
        if (*line == L'\r') line++;
        if (*line == L'\n') line++;
        
        // End page if full
        if (lineCount >= linesPerPage && *line) {
            EndPage(hdc);
            inPage = FALSE;
        }
    }
    
    // End last page if needed
    if (inPage) {
        EndPage(hdc);
    }
    
    // Finish print job
    EndDoc(hdc);
    
    // Cleanup
    SelectObject(hdc, hOldFont);
    DeleteDC(hdc);
    free(text);
}

// ============================================================================
// HandleCommand - Process Menu and Accelerator Commands
// ============================================================================
// Dispatches WM_COMMAND messages to appropriate handler functions based on
// the command ID. Handles all menu items and keyboard accelerators.
// Parameters:
//   hwnd   - Main window handle
//   wParam - Contains command ID in LOWORD
//   lParam - Additional command-specific data
// ============================================================================
static void HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    // Extract command ID from wParam
    switch (LOWORD(wParam)) {
    // ------------------------------------------------------------------------
    // File Menu Commands
    // ------------------------------------------------------------------------
    case IDM_FILE_NEW:      // Ctrl+N
        DoFileNew(hwnd);
        break;
    case IDM_FILE_OPEN:     // Ctrl+O
        DoFileOpen(hwnd);
        break;
    case IDM_FILE_SAVE:     // Ctrl+S
        DoFileSave(hwnd, FALSE);
        break;
    case IDM_FILE_SAVE_AS:  // Save As...
        DoFileSave(hwnd, TRUE);
        break;
    case IDM_FILE_PAGE_SETUP:  // Page Setup...
        DoPageSetup(hwnd);
        break;
    case IDM_FILE_PRINT:    // Ctrl+P
        DoPrint(hwnd);
        break;
    case IDM_FILE_EXIT:     // Alt+F4 or File->Exit
        // Post WM_CLOSE to trigger save prompt
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        break;

    // ------------------------------------------------------------------------
    // Edit Menu Commands
    // ------------------------------------------------------------------------
    case IDM_EDIT_UNDO:     // Ctrl+Z
        // Edit control has built-in undo
        SendMessageW(g_app.hwndEdit, EM_UNDO, 0, 0);
        break;
    case IDM_EDIT_CUT:      // Ctrl+X
        SendMessageW(g_app.hwndEdit, WM_CUT, 0, 0);
        break;
    case IDM_EDIT_COPY:     // Ctrl+C
        SendMessageW(g_app.hwndEdit, WM_COPY, 0, 0);
        break;
    case IDM_EDIT_PASTE:    // Ctrl+V
        SendMessageW(g_app.hwndEdit, WM_PASTE, 0, 0);
        break;
    case IDM_EDIT_DELETE:   // Del
        SendMessageW(g_app.hwndEdit, WM_CLEAR, 0, 0);
        break;
    case IDM_EDIT_FIND:     // Ctrl+F
        ShowFindDialog(hwnd);
        break;
    case IDM_EDIT_FIND_NEXT:  // F3
        DoFindNext(FALSE);
        break;
    case IDM_EDIT_REPLACE:  // Ctrl+H
        ShowReplaceDialog(hwnd);
        break;
    case IDM_EDIT_GOTO:     // Ctrl+G
        // Only available when word wrap is OFF
        if (g_app.wordWrap) {
            MessageBoxW(hwnd, L"Go To is unavailable when Word Wrap is on.", APP_TITLE, MB_ICONINFORMATION);
        } else {
            // Show modal dialog
            DialogBoxW(g_hInst, MAKEINTRESOURCE(IDD_GOTO), hwnd, GoToDlgProc);
        }
        break;
    case IDM_EDIT_SELECT_ALL:  // Ctrl+A
        // Select from start (0) to end (-1)
        SendMessageW(g_app.hwndEdit, EM_SETSEL, 0, -1);
        break;
    case IDM_EDIT_TIME_DATE:   // F5
        InsertTimeDate(hwnd);
        break;

    // ------------------------------------------------------------------------
    // Format Menu Commands
    // ------------------------------------------------------------------------
    case IDM_FORMAT_WORD_WRAP:
        // Toggle word wrap state
        SetWordWrap(hwnd, !g_app.wordWrap);
        break;
    case IDM_FORMAT_FONT:
        DoSelectFont(hwnd);
        break;

    // ------------------------------------------------------------------------
    // View Menu Commands
    // ------------------------------------------------------------------------
    case IDM_VIEW_STATUS_BAR:
        // Toggle status bar visibility
        ToggleStatusBar(hwnd, !g_app.statusVisible);
        break;

    // ------------------------------------------------------------------------
    // Help Menu Commands
    // ------------------------------------------------------------------------
    case IDM_HELP_VIEW_HELP:
        // Show modal Help dialog with keyboard shortcuts and usage
        DialogBoxW(g_hInst, MAKEINTRESOURCE(IDD_HELP), hwnd, HelpDlgProc);
        break;
    case IDM_HELP_ABOUT:
        // Show modal About dialog
        DialogBoxW(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
        break;
    }
}

// ============================================================================
// HelpDlgProc - Dialog Procedure for Help Dialog
// ============================================================================
// Displays usage instructions and keyboard shortcuts for the application.
// ============================================================================
static INT_PTR CALLBACK HelpDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (msg) {
    // Dialog initialization
    case WM_INITDIALOG:
        // Return TRUE to set focus to default control
        return TRUE;
    
    // Button clicks
    case WM_COMMAND:
        // Close dialog on OK or Cancel button
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dlg, LOWORD(wParam));
            return TRUE;  // Message handled
        }
        break;
    }
    // Return FALSE for unhandled messages
    return FALSE;
}

// ============================================================================
// AboutDlgProc - Dialog Procedure for About Box
// ============================================================================
// Simple dialog that displays application information. Just handles
// initialization and OK/Cancel button clicks.
// ============================================================================
static INT_PTR CALLBACK AboutDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (msg) {
    // Dialog initialization
    case WM_INITDIALOG:
        // Return TRUE to set focus to default control
        return TRUE;
    
    // Button clicks
    case WM_COMMAND:
        // Close dialog on OK or Cancel button
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dlg, LOWORD(wParam));
            return TRUE;  // Message handled
        }
        break;
    }
    // Return FALSE for unhandled messages
    return FALSE;
}

// ============================================================================
// MainWndProc - Main Window Procedure
// ============================================================================
// Processes all messages for the main application window. This is the heart
// of the Windows message loop, handling user input, system events, and
// inter-window communication.
// Key messages handled:
// - WM_CREATE: Initialize window and child controls
// - WM_COMMAND: Menu and accelerator commands
// - WM_SIZE: Window resize
// - WM_CLOSE: Window close (with save prompt)
// - WM_DROPFILES: Drag-and-drop file handling
// - Find/Replace messages: From modeless Find/Replace dialogs
// ============================================================================
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Check for registered Find/Replace message first
    // This is a custom message registered by Windows for common dialogs
    if (msg == g_findMsg) {
        HandleFindReplace((LPFINDREPLACE)lParam);
        return 0;
    }

    switch (msg) {
    // ------------------------------------------------------------------------
    // WM_CREATE: Window Creation
    // Sent once when window is first created. Initialize all child controls.
    // ------------------------------------------------------------------------
    case WM_CREATE: {
        // Initialize common controls library (needed for status bar)
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);
        
        // Create the main edit control
        CreateEditControl(hwnd);
        
        // Create and show status bar
        ToggleStatusBar(hwnd, TRUE);
        
        // Load and apply saved word wrap setting
        BOOL savedWordWrap = LoadWordWrapSetting();
        if (savedWordWrap) {
            SetWordWrap(hwnd, TRUE);
        }
        
        // Set initial title and status
        UpdateTitle(hwnd);
        UpdateStatusBar(hwnd);
        
        // Enable drag-and-drop for files
        DragAcceptFiles(hwnd, TRUE);
        return 0;
    }
    
    // ------------------------------------------------------------------------
    // WM_SETFOCUS: Window Received Focus
    // Forward focus to edit control so user can type immediately
    // ------------------------------------------------------------------------
    case WM_SETFOCUS:
        if (g_app.hwndEdit) SetFocus(g_app.hwndEdit);
        return 0;
    
    // ------------------------------------------------------------------------
    // WM_SIZE: Window Resized
    // Reposition child controls to fit new window size
    // ------------------------------------------------------------------------
    case WM_SIZE:
        UpdateLayout(hwnd);
        UpdateStatusBar(hwnd);
        return 0;
    
    // ------------------------------------------------------------------------
    // WM_DROPFILES: File Drag-and-Drop
    // User dragged a file onto the window. Load the first file dropped.
    // ------------------------------------------------------------------------
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;  // Drop handle
        WCHAR path[MAX_PATH_BUFFER];
        // Get first dropped file path (index 0)
        if (DragQueryFileW(hDrop, 0, path, ARRAYSIZE(path))) {
            // Prompt to save current file, then load dropped file
            if (PromptSaveChanges(hwnd)) {
                LoadDocumentFromPath(hwnd, path);
            }
        }
        DragFinish(hDrop);  // Clean up drop operation
        return 0;
    }
    
    // ------------------------------------------------------------------------
    // WM_COMMAND: Menu Items, Accelerators, and Control Notifications
    // This message handles:
    // 1. Menu item selections (File, Edit, Format, View, Help menus)
    // 2. Keyboard accelerators (Ctrl+S, Ctrl+F, etc.)
    // 3. Control notifications from child windows (edit control changes)
    // ------------------------------------------------------------------------
    case WM_COMMAND:
        // Handle notifications from edit control
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_app.hwndEdit) {
            // Text changed - update modified flag
            g_app.modified = (SendMessageW(g_app.hwndEdit, EM_GETMODIFY, 0, 0) != 0);
            UpdateTitle(hwnd);
            UpdateStatusBar(hwnd);
            return 0;
        } else if (HIWORD(wParam) == EN_UPDATE && (HWND)lParam == g_app.hwndEdit) {
            // Edit control about to be redrawn - update status bar
            UpdateStatusBar(hwnd);
            return 0;
        }
        // Handle menu commands and accelerators
        HandleCommand(hwnd, wParam, lParam);
        return 0;
    
    // ------------------------------------------------------------------------
    // WM_INITMENUPOPUP: Menu About to be Displayed
    // Update menu item states (checked/unchecked, enabled/disabled)
    // before showing menu to user
    // ------------------------------------------------------------------------
    case WM_INITMENUPOPUP:
        UpdateMenuStates(hwnd);
        return 0;
    
    // ------------------------------------------------------------------------
    // WM_CLOSE: User Requested Window Close
    // Prompt to save unsaved changes before allowing window to close
    // ------------------------------------------------------------------------
    case WM_CLOSE:
        if (PromptSaveChanges(hwnd)) {
            DestroyWindow(hwnd);  // Okay to close
        }
        // Otherwise user cancelled - stay open
        return 0;
    
    // ------------------------------------------------------------------------
    // WM_DESTROY: Window Being Destroyed
    // Post quit message to exit application message loop
    // ------------------------------------------------------------------------
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    
    // Default processing for any messages we don't handle
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// wWinMain - Application Entry Point (Unicode)
// ============================================================================
// This is the main entry point for a Windows GUI application with Unicode
// support. It:
// 1. Initializes global state
// 2. Registers the window class
// 3. Creates the main window
// 4. Runs the message loop until application exits
// Parameters:
//   hInstance     - Handle to current instance of application
//   hPrevInstance - (Unused, always NULL in modern Windows)
//   lpCmdLine     - Command line arguments
//   nCmdShow      - How window should be shown (maximized, minimized, etc.)
// Returns: Exit code (wParam of WM_QUIT message)
// ============================================================================
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Suppress unused parameter warnings
    (void)hPrevInstance;  // Always NULL in Win32
    (void)lpCmdLine;      // Not using command line args in this app

    // Initialize global state
    g_hInst = hInstance;
    // Register custom message ID for Find/Replace dialogs
    g_findMsg = RegisterWindowMessageW(FINDMSGSTRINGW);
    
    // Set default application state
    g_app.wordWrap = FALSE;              // Word wrap off by default
    g_app.statusVisible = TRUE;          // Status bar visible by default
    g_app.statusBeforeWrap = TRUE;       // Remember status bar preference
    g_app.encoding = ENC_UTF8;           // Default to UTF-8 for new files
    g_app.findFlags = FR_DOWN;           // Search down by default

    // Define and register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;  // Redraw on resize
    wc.lpfnWndProc = MainWndProc;        // Window procedure
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_RETROPAD));  // App icon
    wc.hIconSm = wc.hIcon;               // Small icon (taskbar)
    wc.hCursor = LoadCursorW(NULL, IDC_IBEAM);  // I-beam cursor (text editing)
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);  // Default window background
    wc.lpszClassName = L"RETROPAD_WINDOW";  // Unique class name
    wc.lpszMenuName = MAKEINTRESOURCE(IDC_RETROPAD);  // Menu resource

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Failed to register window class.", APP_TITLE, MB_ICONERROR);
        return 0;
    }

    // Create main application window
    // WS_OVERLAPPEDWINDOW = standard window with title bar, borders, and system menu
    // CW_USEDEFAULT = let Windows choose initial position
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, DEFAULT_WIDTH, DEFAULT_HEIGHT,
                                NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create main window.", APP_TITLE, MB_ICONERROR);
        return 0;
    }

    // Save window handle in global state
    g_app.hwndMain = hwnd;
    
    // Show window with specified show state (normal, maximized, minimized)
    ShowWindow(hwnd, nCmdShow);
    // Force immediate paint
    UpdateWindow(hwnd);

    // Load keyboard accelerators (Ctrl+S, Ctrl+O, F3, etc.)
    HACCEL accel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCE(IDC_RETROPAD));

    // Main message loop
    // Continues until GetMessageW returns 0 (received WM_QUIT)
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        // Check for accelerator key (e.g., Ctrl+S)
        // If not accelerator, translate and dispatch normally
        if (!accel || !TranslateAcceleratorW(hwnd, accel, &msg)) {
            TranslateMessage(&msg);  // Translate virtual-key messages to character messages
            DispatchMessageW(&msg);  // Dispatch message to window procedure
        }
    }

    // Return exit code from WM_QUIT message
    return (int)msg.wParam;
}
