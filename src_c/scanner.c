#include "scanner.h"
#include "hash.h"
#include <wincodec.h>
#include <propvarutil.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "propsys.lib")

// Supported file extensions
static const wchar_t* SUPPORTED_EXTENSIONS[] = {
    L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tiff", L".tif",
    L".arw", L".cr2", L".nef", L".dng", L".orf", L".rw2", L".raw",
    L".mp4", L".mov", L".avi", L".mkv", L".wmv", L".mpg", L".mpeg",
    NULL
};

bool IsSupportedFile(const wchar_t* filename) {
    // Skip files starting with '.' (hidden/dotfiles)
    if (filename[0] == L'.') {
        return false;
    }
    
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (ext == NULL) return false;

    for (int i = 0; SUPPORTED_EXTENSIONS[i] != NULL; i++) {
        if (_wcsicmp(ext, SUPPORTED_EXTENSIONS[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Extract EXIF date using Windows Imaging Component (Vista+)
// Returns true if EXIF date was successfully extracted, false otherwise
// Note: COM must be initialized before calling this function
bool ExtractExifDate(const wchar_t* filepath, FILETIME* outFileTime) {
    HRESULT hr = S_OK;
    IWICImagingFactory* pFactory = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICMetadataQueryReader* pMetadataReader = NULL;
    bool success = false;
    
    // Create WIC factory
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (LPVOID*)&pFactory);
    if (FAILED(hr)) {
        return false;
    }
    
    // Create decoder from filename
    hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, filepath, NULL,
                                                      GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                                      &pDecoder);
    if (FAILED(hr)) {
        goto cleanup;
    }
    
    // Get first frame
    hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);
    if (FAILED(hr)) {
        goto cleanup;
    }
    
    // Get metadata query reader
    hr = pFrame->lpVtbl->GetMetadataQueryReader(pFrame, &pMetadataReader);
    if (FAILED(hr)) {
        goto cleanup;
    }
    
    // Try to read EXIF date/time original
    PROPVARIANT value;
    PropVariantInit(&value);
    
    // Try different EXIF date tags in order of preference
    const wchar_t* dateTags[] = {
        L"/app1/ifd/exif/{ushort=36867}",  // DateTimeOriginal
        L"/app1/ifd/exif/{ushort=36868}",  // DateTimeDigitized
        L"/app1/ifd/{ushort=306}",         // DateTime
        NULL
    };
    
    for (int i = 0; dateTags[i] != NULL; i++) {
        hr = pMetadataReader->lpVtbl->GetMetadataByName(pMetadataReader, dateTags[i], &value);
        if (SUCCEEDED(hr) && value.vt == VT_LPWSTR) {
            // Parse EXIF date format: "YYYY:MM:DD HH:MM:SS"
            SYSTEMTIME st = {0};
            if (swscanf_s(value.pwszVal, L"%04hu:%02hu:%02hu %02hu:%02hu:%02hu",
                         &st.wYear, &st.wMonth, &st.wDay,
                         &st.wHour, &st.wMinute, &st.wSecond) == 6) {
                // Convert SYSTEMTIME to FILETIME
                SystemTimeToFileTime(&st, outFileTime);
                success = true;
            }
            PropVariantClear(&value);
            if (success) break;
        }
        PropVariantClear(&value);
    }
    
cleanup:
    if (pMetadataReader) pMetadataReader->lpVtbl->Release(pMetadataReader);
    if (pFrame) pFrame->lpVtbl->Release(pFrame);
    if (pDecoder) pDecoder->lpVtbl->Release(pDecoder);
    if (pFactory) pFactory->lpVtbl->Release(pFactory);
    
    return success;
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
    
    // Try to extract EXIF date if enabled and it's an image file
    if (g_app.useExifDate) {
        const wchar_t* ext = wcsrchr(filepath, L'.');
        if (ext != NULL) {
            // Only try EXIF extraction for image files (not videos)
            if (_wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 ||
                _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".tiff") == 0 ||
                _wcsicmp(ext, L".tif") == 0 || _wcsicmp(ext, L".arw") == 0 ||
                _wcsicmp(ext, L".cr2") == 0 || _wcsicmp(ext, L".nef") == 0 ||
                _wcsicmp(ext, L".dng") == 0 || _wcsicmp(ext, L".orf") == 0 ||
                _wcsicmp(ext, L".rw2") == 0 || _wcsicmp(ext, L".raw") == 0) {
                
                FILETIME exifDate;
                if (ExtractExifDate(filepath, &exifDate)) {
                    // Use EXIF date instead of file modification time
                    file->fileTime = exifDate;
                }
            }
        }
    }
    
    file->fileSize = ((ULONGLONG)findData->nFileSizeHigh << 32) | findData->nFileSizeLow;
    file->hashComputed = false;
    memset(file->hash, 0, HASH_SIZE);
    file->isSelected = true;  // Default: all files selected
    
    g_app.fileCount++;
    
    LeaveCriticalSection(&g_app.csFiles);
    
    // Update UI on main thread (synchronous for safety)
    SetWindowTextW(g_app.hwndStatusText, L"Scanning...");
}

// Directory entry for iterative traversal (avoids stack overflow)
typedef struct {
    wchar_t path[MAX_PATH_LEN];
} DirectoryEntry;

void ScanDirectory(const wchar_t* path, bool recursive) {
    // Use iterative approach with manual stack to avoid stack overflow
    // Allocate directory stack on heap (initial capacity: 256 directories)
    int stackCapacity = 256;
    int stackSize = 0;
    DirectoryEntry* dirStack = (DirectoryEntry*)malloc(stackCapacity * sizeof(DirectoryEntry));
    if (!dirStack) {
        // Out of memory - log error and return
        SetWindowTextW(g_app.hwndStatusText, L"Error: Out of memory during scan");
        return;
    }
    
    // Push initial directory
    wcsncpy_s(dirStack[stackSize].path, MAX_PATH_LEN, path, _TRUNCATE);
    stackSize++;
    
    // Process directories iteratively
    while (stackSize > 0 && !g_app.stopRequested) {
        // Pop directory from stack
        stackSize--;
        wchar_t currentPath[MAX_PATH_LEN];
        wcsncpy_s(currentPath, MAX_PATH_LEN, dirStack[stackSize].path, _TRUNCATE);
        
        // Build search path
        wchar_t searchPath[MAX_PATH_LEN];
        swprintf_s(searchPath, MAX_PATH_LEN, L"%s\\*", currentPath);
        
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath, &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) {
            continue;
        }
        
        do {
            if (g_app.stopRequested) break;
            
            // Skip "." and ".."
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            
            wchar_t fullPath[MAX_PATH_LEN];
            swprintf_s(fullPath, MAX_PATH_LEN, L"%s\\%s", currentPath, findData.cFileName);
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (recursive) {
                    // Push subdirectory to stack
                    if (stackSize >= stackCapacity) {
                        // Expand stack capacity
                        int newCapacity = stackCapacity * 2;
                        DirectoryEntry* newStack = (DirectoryEntry*)realloc(dirStack, newCapacity * sizeof(DirectoryEntry));
                        if (newStack) {
                            dirStack = newStack;
                            stackCapacity = newCapacity;
                        } else {
                            // Out of memory - continue with current directories
                            break;
                        }
                    }
                    wcsncpy_s(dirStack[stackSize].path, MAX_PATH_LEN, fullPath, _TRUNCATE);
                    stackSize++;
                }
            } else {
                if (IsSupportedFile(findData.cFileName)) {
                    AddFile(fullPath, &findData);
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    free(dirStack);
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
    ListView_DeleteAllItems(g_app.hwndFileList);
    
    g_app.suppressPreviewOnCheck = true;
    
    EnterCriticalSection(&g_app.csFiles);
    for (int i = 0; i < g_app.fileCount; i++) {
        const wchar_t* filename = wcsrchr(g_app.files[i].path, L'\\');
        if (filename) filename++;
        else filename = g_app.files[i].path;
        
        g_app.files[i].isSelected = true;
        
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)filename;
        lvi.lParam = i;  // Store file index
        ListView_InsertItem(g_app.hwndFileList, &lvi);
        
        ListView_SetCheckState(g_app.hwndFileList, i, TRUE);
    }
    LeaveCriticalSection(&g_app.csFiles);
    
    g_app.suppressPreviewOnCheck = false;
    
    // Update status (allocate string for async update)
    wchar_t* statusText = (wchar_t*)malloc(256 * sizeof(wchar_t));
    if (statusText) {
        swprintf_s(statusText, 256, L"Found %d files", g_app.fileCount);
        // Use synchronous update to avoid memory management complexity
        SetWindowTextW(g_app.hwndStatusText, statusText);
        free(statusText);
    }
    
    // Trigger preview update if we have files
    if (g_app.fileCount > 0) {
        UpdatePreviewTree();
    }
    
    // Close thread handle and mark as complete
    HANDLE hThread = g_app.hScanThread;
    g_app.hScanThread = NULL;
    if (hThread) {
        CloseHandle(hThread);
    }
    
    free(path);
    return 0;
}
