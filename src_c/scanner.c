#include "scanner.h"
#include "hash.h"

// Supported file extensions
static const wchar_t* SUPPORTED_EXTENSIONS[] = {
    L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tiff", L".tif",
    L".arw", L".cr2", L".nef", L".dng", L".orf", L".rw2", L".raw",
    L".mp4", L".mov", L".avi", L".mkv", L".wmv", L".mpg", L".mpeg",
    NULL
};

bool IsSupportedFile(const wchar_t* filename) {
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (ext == NULL) return false;

    for (int i = 0; SUPPORTED_EXTENSIONS[i] != NULL; i++) {
        if (_wcsicmp(ext, SUPPORTED_EXTENSIONS[i]) == 0) {
            return true;
        }
    }
    return false;
}

void AddFile(const wchar_t* filepath, const WIN32_FIND_DATAW* findData) {
    EnterCriticalSection(&g_app.csFiles);
    
    if (g_app.fileCount >= g_app.fileCapacity) {
        // Expand capacity
        int newCapacity = g_app.fileCapacity * 2;
        FileInfo* newFiles = (FileInfo*)realloc(g_app.files, newCapacity * sizeof(FileInfo));
        if (newFiles == NULL) {
            LeaveCriticalSection(&g_app.csFiles);
            return;
        }
        g_app.files = newFiles;
        g_app.fileCapacity = newCapacity;
    }
    
    FileInfo* file = &g_app.files[g_app.fileCount];
    wcsncpy_s(file->path, MAX_PATH_LEN, filepath, _TRUNCATE);
    file->fileTime = findData->ftLastWriteTime;
    file->fileSize = ((ULONGLONG)findData->nFileSizeHigh << 32) | findData->nFileSizeLow;
    file->hashComputed = false;
    memset(file->hash, 0, HASH_SIZE);
    
    g_app.fileCount++;
    
    LeaveCriticalSection(&g_app.csFiles);
    
    // Update UI on main thread (synchronous for safety)
    SetWindowTextW(g_app.hwndStatusText, L"Scanning...");
}

void ScanDirectory(const wchar_t* path, bool recursive) {
    wchar_t searchPath[MAX_PATH_LEN];
    swprintf_s(searchPath, MAX_PATH_LEN, L"%s\\*", path);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        if (g_app.stopRequested) break;
        
        // Skip "." and ".."
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        wchar_t fullPath[MAX_PATH_LEN];
        swprintf_s(fullPath, MAX_PATH_LEN, L"%s\\%s", path, findData.cFileName);
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive) {
                ScanDirectory(fullPath, recursive);
            }
        } else {
            if (IsSupportedFile(findData.cFileName)) {
                AddFile(fullPath, &findData);
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

DWORD WINAPI ScanThread(LPVOID lpParam) {
    wchar_t* path = (wchar_t*)lpParam;
    
    // Clear existing files
    EnterCriticalSection(&g_app.csFiles);
    g_app.fileCount = 0;
    LeaveCriticalSection(&g_app.csFiles);
    
    g_app.stopRequested = false;
    
    // Update status (synchronous to avoid lifetime issues)
    SetWindowTextW(g_app.hwndStatusText, L"Scanning files...");
    
    // Scan directory
    ScanDirectory(path, g_app.isRecursive);
    
    // Update file list view
    SendMessage(g_app.hwndFileList, LB_RESETCONTENT, 0, 0);
    
    EnterCriticalSection(&g_app.csFiles);
    for (int i = 0; i < g_app.fileCount; i++) {
        const wchar_t* filename = wcsrchr(g_app.files[i].path, L'\\');
        if (filename) filename++;
        else filename = g_app.files[i].path;
        
        SendMessage(g_app.hwndFileList, LB_ADDSTRING, 0, (LPARAM)filename);
    }
    LeaveCriticalSection(&g_app.csFiles);
    
    // Update status (allocate string for async update)
    wchar_t* statusText = (wchar_t*)malloc(256 * sizeof(wchar_t));
    if (statusText) {
        swprintf_s(statusText, 256, L"Found %d files", g_app.fileCount);
        // Use synchronous update to avoid memory management complexity
        SetWindowTextW(g_app.hwndStatusText, statusText);
        free(statusText);
    }
    
    // Trigger preview update if organize by date is enabled
    extern void UpdatePreviewTree();
    if (g_app.fileCount > 0) {
        UpdatePreviewTree();
    }
    
    free(path);
    return 0;
}
