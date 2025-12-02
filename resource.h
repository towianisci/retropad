// ============================================================================
// resource.h - Resource Identifiers for retropad
// ============================================================================
// This file defines all resource IDs used in the application including:
// - Icons and images
// - Menu items and commands
// - Dialog boxes and controls
// These IDs link the .rc resource file to the C code.
// ============================================================================

#pragma once

// ============================================================================
// Application Icons
// ============================================================================
#define IDI_RETROPAD            101    // Main application icon

// ============================================================================
// Main Menu and Accelerator Table
// ============================================================================
#define IDC_RETROPAD            1000   // Main menu resource ID

// ============================================================================
// File Menu Commands (40001-40009)
// ============================================================================
#define IDM_FILE_NEW            40001  // Create a new document (Ctrl+N)
#define IDM_FILE_OPEN           40002  // Open an existing file (Ctrl+O)
#define IDM_FILE_SAVE           40003  // Save current document (Ctrl+S)
#define IDM_FILE_SAVE_AS        40004  // Save document with new name
#define IDM_FILE_PAGE_SETUP     40005  // Page setup dialog
#define IDM_FILE_PRINT          40006  // Print document (Ctrl+P)
#define IDM_FILE_EXIT           40007  // Exit application

// ============================================================================
// Edit Menu Commands (40010-40020)
// ============================================================================
#define IDM_EDIT_UNDO           40010  // Undo last edit (Ctrl+Z)
#define IDM_EDIT_CUT            40011  // Cut selection to clipboard (Ctrl+X)
#define IDM_EDIT_COPY           40012  // Copy selection to clipboard (Ctrl+C)
#define IDM_EDIT_PASTE          40013  // Paste from clipboard (Ctrl+V)
#define IDM_EDIT_DELETE         40014  // Delete selection (Del)
#define IDM_EDIT_FIND           40015  // Open Find dialog (Ctrl+F)
#define IDM_EDIT_FIND_NEXT      40016  // Find next occurrence (F3)
#define IDM_EDIT_REPLACE        40017  // Open Find/Replace dialog (Ctrl+H)
#define IDM_EDIT_GOTO           40018  // Go to line number (Ctrl+G)
#define IDM_EDIT_SELECT_ALL     40019  // Select all text (Ctrl+A)
#define IDM_EDIT_TIME_DATE      40020  // Insert current time/date (F5)

// ============================================================================
// Format Menu Commands (40030-40039)
// ============================================================================
#define IDM_FORMAT_WORD_WRAP    40030  // Toggle word wrap on/off
#define IDM_FORMAT_FONT         40031  // Select font for editor

// ============================================================================
// View Menu Commands (40040-40049)
// ============================================================================
#define IDM_VIEW_STATUS_BAR     40040  // Toggle status bar visibility

// ============================================================================
// Help Menu Commands (40050-40059)
// ============================================================================
#define IDM_HELP_VIEW_HELP      40050  // View help (not implemented)
#define IDM_HELP_ABOUT          40051  // Show About dialog

// ============================================================================
// Dialog Boxes and Controls (50001-50099)
// ============================================================================
#define IDD_GOTO                50001  // Go To Line dialog
#define IDD_ABOUT               50002  // About dialog
#define IDC_GOTO_EDIT           50010  // Edit control in Go To dialog

