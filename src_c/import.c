#include "import.h"
#include "hash.h"

void GetDateSubdirectory(const FILETIME* ft, int formatIndex, wchar_t* outPath, size_t outSize) {
    SYSTEMTIME st;
    FileTimeToSystemTime(ft, &st);
    
    switch (formatIndex) {
        case 0: // YYYY-MM-DD
            swprintf_s(outPath, outSize, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            break;
        case 1: // YYYY/MM/DD
            swprintf_s(outPath, outSize, L"%04d\\%02d\\%02d", st.wYear, st.wMonth, st.wDay);
            break;
        case 2: // YYYY-MM
            swprintf_s(outPath, outSize, L"%04d-%02d", st.wYear, st.wMonth);
            break;
        case 3: // YYYY/MM
            swprintf_s(outPath, outSize, L"%04d\\%02d", st.wYear, st.wMonth);
            break;
        case 4: // YYYY
            swprintf_s(outPath, outSize, L"%04d", st.wYear);
            break;
        default:
            swprintf_s(outPath, outSize, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            break;
    }
}

bool FilesAreIdentical(const wchar_t* file1, const wchar_t* file2) {
    unsigned char hash1[HASH_SIZE], hash2[HASH_SIZE];
    
    if (!ComputeFileHashFast(file1, hash1)) return false;
    if (!ComputeFileHashFast(file2, hash2)) return false;
    
    return HashesEqual(hash1, hash2);
}

bool ImportFile(const wchar_t* sourcePath, const wchar_t* targetBase, bool move, 
                bool organizeByDate, int dateFormatIndex, const FILETIME* fileTime) {
    wchar_t targetPath[MAX_PATH_LEN];
    wcscpy_s(targetPath, MAX_PATH_LEN, targetBase);
    
    // If organizing by date, create subdirectory
    if (organizeByDate) {
        wchar_t dateSubdir[256];
        GetDateSubdirectory(fileTime, dateFormatIndex, dateSubdir, 256);
        
        wchar_t fullTargetDir[MAX_PATH_LEN];
        swprintf_s(fullTargetDir, MAX_PATH_LEN, L"%s\\%s", targetBase, dateSubdir);
        
        // Create directory if it doesn't exist
        SHCreateDirectoryExW(NULL, fullTargetDir, NULL);
        
        wcscpy_s(targetPath, MAX_PATH_LEN, fullTargetDir);
    }
    
    // Get filename from source path
    const wchar_t* filename = wcsrchr(sourcePath, L'\\');
    if (filename) filename++;
    else filename = sourcePath;
    
    wchar_t destFile[MAX_PATH_LEN];
    swprintf_s(destFile, MAX_PATH_LEN, L"%s\\%s", targetPath, filename);
    
    // Check if destination file exists
    if (PathFileExistsW(destFile)) {
        // Check if files are identical (deduplication)
        if (FilesAreIdentical(sourcePath, destFile)) {
            // Files are identical, skip
            return true;
        } else {
            // Files are different, add suffix to avoid overwrite
            wchar_t baseName[MAX_PATH_LEN];
            wchar_t extension[MAX_PATH_LEN];
            wcscpy_s(baseName, MAX_PATH_LEN, filename);
            wchar_t* ext = wcsrchr(baseName, L'.');
            if (ext) {
                wcscpy_s(extension, MAX_PATH_LEN, ext);
                *ext = L'\0';
            } else {
                extension[0] = L'\0';
            }
            
            swprintf_s(destFile, MAX_PATH_LEN, L"%s\\%s_copy%s", targetPath, baseName, extension);
        }
    }
    
    // Perform copy or move
    BOOL result;
    if (move) {
        result = MoveFileW(sourcePath, destFile);
    } else {
        result = CopyFileW(sourcePath, destFile, FALSE);
    }
    
    return result != 0;
}

DWORD WINAPI ImportThread(LPVOID lpParam) {
    g_app.stopRequested = false;
    
    int totalFiles = g_app.fileCount;
    int processedFiles = 0;
    int skippedDuplicates = 0;
    
    // Update progress bar range
    SendMessage(g_app.hwndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, totalFiles));
    SendMessage(g_app.hwndProgressBar, PBM_SETPOS, 0, 0);
    
    for (int i = 0; i < totalFiles && !g_app.stopRequested; i++) {
        FileInfo* file = &g_app.files[i];
        
        // Compute hash if not already computed for deduplication
        if (!file->hashComputed) {
            ComputeFileHashFast(file->path, file->hash);
            file->hashComputed = true;
        }
        
        // Check for duplicates in already imported files
        bool isDuplicate = false;
        for (int j = 0; j < i; j++) {
            if (g_app.files[j].hashComputed && HashesEqual(file->hash, g_app.files[j].hash)) {
                isDuplicate = true;
                skippedDuplicates++;
                break;
            }
        }
        
        if (!isDuplicate) {
            bool success = ImportFile(file->path, g_app.targetPath, g_app.isMoving, 
                                     g_app.organizeByDate, g_app.dateFormatIndex, &file->fileTime);
            if (!success) {
                wchar_t errorMsg[512];
                swprintf_s(errorMsg, 512, L"Failed to import: %s", file->path);
                PostMessage(g_app.hwndStatusText, WM_SETTEXT, 0, (LPARAM)errorMsg);
            }
        }
        
        processedFiles++;
        SendMessage(g_app.hwndProgressBar, PBM_SETPOS, processedFiles, 0);
        
        // Update status every 10 files
        if (processedFiles % 10 == 0 || processedFiles == totalFiles) {
            wchar_t statusText[256];
            swprintf_s(statusText, 256, L"Importing: %d/%d (Skipped %d duplicates)", 
                      processedFiles, totalFiles, skippedDuplicates);
            PostMessage(g_app.hwndStatusText, WM_SETTEXT, 0, (LPARAM)statusText);
        }
    }
    
    // Final status
    wchar_t finalStatus[256];
    swprintf_s(finalStatus, 256, L"Import complete: %d files, %d duplicates skipped", 
              processedFiles, skippedDuplicates);
    PostMessage(g_app.hwndStatusText, WM_SETTEXT, 0, (LPARAM)finalStatus);
    
    // Reset progress bar
    SendMessage(g_app.hwndProgressBar, PBM_SETPOS, 0, 0);
    
    return 0;
}
