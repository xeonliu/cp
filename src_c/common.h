#ifndef COMMON_H
#define COMMON_H

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdbool.h>

// Constants
#define MAX_FILES 100000
#define BUFFER_SIZE 65536
#define HASH_SIZE 16
#define MAX_PATH_LEN 32768
#define DATE_FORMAT_CUSTOM 7  // Index for custom template format

// UI Constants
#define ID_SCAN_BUTTON 1001
#define ID_IMPORT_BUTTON 1002
#define ID_SOURCE_TREE 1003
#define ID_FILE_LIST 1004
#define ID_TARGET_EDIT 1005
#define ID_TARGET_BROWSE 1006
#define ID_PROGRESS_BAR 1007
#define ID_RECURSIVE_CHECK 1008
#define ID_ORGANIZE_CHECK 1009
#define ID_COPY_RADIO 1010
#define ID_MOVE_RADIO 1011
#define ID_DATE_FORMAT_COMBO 1012
#define ID_STATUS_TEXT 1013
#define ID_CUSTOM_TEMPLATE_EDIT 1014
#define ID_PREVIEW_BUTTON 1015
#define ID_PREVIEW_TREE 1016

// File information structure
typedef struct {
    wchar_t path[MAX_PATH_LEN];
    FILETIME fileTime;
    ULONGLONG fileSize;
    unsigned char hash[HASH_SIZE];
    bool hashComputed;
} FileInfo;

// Application state
typedef struct {
    HWND hwndMain;
    HWND hwndSourceTree;
    HWND hwndFileList;
    HWND hwndTargetEdit;
    HWND hwndProgressBar;
    HWND hwndStatusText;
    HWND hwndRecursiveCheck;
    HWND hwndOrganizeCheck;
    HWND hwndCopyRadio;
    HWND hwndMoveRadio;
    HWND hwndDateFormatCombo;
    HWND hwndCustomTemplateEdit;
    HWND hwndPreviewTree;
    
    FileInfo* files;
    int fileCount;
    int fileCapacity;
    
    wchar_t targetPath[MAX_PATH_LEN];
    wchar_t customTemplate[256];
    bool isRecursive;
    bool organizeByDate;
    bool isMoving;
    int dateFormatIndex;
    
    CRITICAL_SECTION csFiles;
    HANDLE hScanThread;
    HANDLE hImportThread;
    volatile bool stopRequested;
} AppState;

// Global state
extern AppState g_app;

// Function declarations
void UpdatePreviewTree();
void GetDateSubdirectory(const FILETIME* ft, int formatIndex, wchar_t* outPath, size_t outSize);
void GetDateSubdirectoryFromTemplate(const FILETIME* ft, const wchar_t* customTemplate, wchar_t* outPath, size_t outSize);

#endif // COMMON_H
