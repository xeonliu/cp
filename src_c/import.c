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
        case 5: // YYYY/YYYY-MM-DD (nested)
            swprintf_s(outPath, outSize, L"%04d\\%04d-%02d-%02d", st.wYear, st.wYear, st.wMonth, st.wDay);
            break;
        case 6: // YYYY-MM/DD (nested)
            swprintf_s(outPath, outSize, L"%04d-%02d\\%02d", st.wYear, st.wMonth, st.wDay);
            break;
        case 7: // Custom template
            // Will be handled separately
            outPath[0] = L'\0';
            break;
        default:
            swprintf_s(outPath, outSize, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            break;
    }
}

// Parse custom template like {year}/{year}-{month:02d}-{day:02d}
void GetDateSubdirectoryFromTemplate(const FILETIME* ft, const wchar_t* customTemplate, wchar_t* outPath, size_t outSize) {
    SYSTEMTIME st;
    FileTimeToSystemTime(ft, &st);
    
    wchar_t result[512] = L"";
    size_t resultLen = 0;
    size_t templateLen = wcslen(customTemplate);
    
    for (size_t i = 0; i < templateLen && resultLen < outSize - 1; i++) {
        if (customTemplate[i] == L'{') {
            // Find the closing brace
            size_t j = i + 1;
            while (j < templateLen && customTemplate[j] != L'}') j++;
            
            if (j < templateLen) {
                // Extract placeholder content
                wchar_t placeholder[64];
                size_t placeholderLen = j - i - 1;
                if (placeholderLen < 64) {
                    wcsncpy_s(placeholder, 64, &customTemplate[i + 1], placeholderLen);
                    placeholder[placeholderLen] = L'\0';
                    
                    // Parse placeholder: key or key:format
                    wchar_t key[32] = L"";
                    wchar_t format[32] = L"";
                    wchar_t* colonPos = wcschr(placeholder, L':');
                    
                    if (colonPos) {
                        size_t keyLen = colonPos - placeholder;
                        wcsncpy_s(key, 32, placeholder, keyLen);
                        key[keyLen] = L'\0';
                        wcscpy_s(format, 32, colonPos + 1);
                    } else {
                        wcscpy_s(key, 32, placeholder);
                    }
                    
                    // Get the value based on key
                    int value = 0;
                    bool validKey = true;
                    if (_wcsicmp(key, L"year") == 0) value = st.wYear;
                    else if (_wcsicmp(key, L"month") == 0) value = st.wMonth;
                    else if (_wcsicmp(key, L"day") == 0) value = st.wDay;
                    else if (_wcsicmp(key, L"hour") == 0) value = st.wHour;
                    else if (_wcsicmp(key, L"minute") == 0) value = st.wMinute;
                    else if (_wcsicmp(key, L"second") == 0) value = st.wSecond;
                    else validKey = false;
                    
                    if (validKey) {
                        wchar_t valueStr[16];
                        // Format the value
                        if (wcslen(format) > 0 && wcscmp(format, L"02d") == 0) {
                            swprintf_s(valueStr, 16, L"%02d", value);
                        } else if (wcslen(format) > 0 && wcscmp(format, L"04d") == 0) {
                            swprintf_s(valueStr, 16, L"%04d", value);
                        } else {
                            // Default formatting: pad month, day, hour, minute, second
                            if (_wcsicmp(key, L"month") == 0 || _wcsicmp(key, L"day") == 0 ||
                                _wcsicmp(key, L"hour") == 0 || _wcsicmp(key, L"minute") == 0 || 
                                _wcsicmp(key, L"second") == 0) {
                                swprintf_s(valueStr, 16, L"%02d", value);
                            } else {
                                swprintf_s(valueStr, 16, L"%d", value);
                            }
                        }
                        
                        // Append to result
                        size_t valueLen = wcslen(valueStr);
                        if (resultLen + valueLen < outSize - 1) {
                            wcscpy_s(&result[resultLen], outSize - resultLen, valueStr);
                            resultLen += valueLen;
                        }
                    }
                }
                i = j; // Skip to closing brace
            }
        } else if (customTemplate[i] == L'/') {
            // Convert forward slash to backslash for Windows paths
            result[resultLen++] = L'\\';
        } else {
            // Regular character
            result[resultLen++] = customTemplate[i];
        }
    }
    
    result[resultLen] = L'\0';
    wcscpy_s(outPath, outSize, result);
}

bool FilesAreIdentical(const wchar_t* file1, const wchar_t* file2) {
    unsigned char hash1[HASH_SIZE], hash2[HASH_SIZE];
    
    if (!ComputeFileHashFast(file1, hash1)) return false;
    if (!ComputeFileHashFast(file2, hash2)) return false;
    
    return HashesEqual(hash1, hash2);
}

bool ImportFile(const wchar_t* sourcePath, const wchar_t* targetBase, bool move, 
                bool organizeByDate, int dateFormatIndex, const wchar_t* customTemplate, const FILETIME* fileTime) {
    wchar_t targetPath[MAX_PATH_LEN];
    wcscpy_s(targetPath, MAX_PATH_LEN, targetBase);
    
    // If organizing by date, create subdirectory
    if (organizeByDate) {
        wchar_t dateSubdir[256];
        
        // Use custom template if provided and format is "Custom"
        if (customTemplate && wcslen(customTemplate) > 0 && dateFormatIndex == 7) {
            GetDateSubdirectoryFromTemplate(fileTime, customTemplate, dateSubdir, 256);
        } else {
            GetDateSubdirectory(fileTime, dateFormatIndex, dateSubdir, 256);
        }
        
        // Only create subdirectory if we got a valid result
        if (wcslen(dateSubdir) > 0) {
            wchar_t fullTargetDir[MAX_PATH_LEN];
            swprintf_s(fullTargetDir, MAX_PATH_LEN, L"%s\\%s", targetBase, dateSubdir);
            
            // Create directory if it doesn't exist
            SHCreateDirectoryExW(NULL, fullTargetDir, NULL);
            
            wcscpy_s(targetPath, MAX_PATH_LEN, fullTargetDir);
        }
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
                                     g_app.organizeByDate, g_app.dateFormatIndex, g_app.customTemplate, &file->fileTime);
            if (!success) {
                // Use synchronous update for error messages
                wchar_t errorMsg[512];
                swprintf_s(errorMsg, 512, L"Failed to import: %s", file->path);
                SetWindowTextW(g_app.hwndStatusText, errorMsg);
            }
        }
        
        processedFiles++;
        SendMessage(g_app.hwndProgressBar, PBM_SETPOS, processedFiles, 0);
        
        // Update status every 10 files (synchronous to avoid lifetime issues)
        if (processedFiles % 10 == 0 || processedFiles == totalFiles) {
            wchar_t statusText[256];
            swprintf_s(statusText, 256, L"Importing: %d/%d (Skipped %d duplicates)", 
                      processedFiles, totalFiles, skippedDuplicates);
            SetWindowTextW(g_app.hwndStatusText, statusText);
        }
    }
    
    // Final status (synchronous)
    wchar_t finalStatus[256];
    swprintf_s(finalStatus, 256, L"Import complete: %d files, %d duplicates skipped", 
              processedFiles, skippedDuplicates);
    SetWindowTextW(g_app.hwndStatusText, finalStatus);
    
    // Reset progress bar
    SendMessage(g_app.hwndProgressBar, PBM_SETPOS, 0, 0);
    
    return 0;
}
