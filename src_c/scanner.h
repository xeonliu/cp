#ifndef SCANNER_H
#define SCANNER_H

#include "common.h"

// Scan directory for image and video files
DWORD WINAPI ScanThread(LPVOID lpParam);

// Add file to the list
void AddFile(const wchar_t* filepath, const WIN32_FIND_DATAW* findData);

// Check if file extension is supported
bool IsSupportedFile(const wchar_t* filename);

// Scan directory (recursive or not)
void ScanDirectory(const wchar_t* path, bool recursive);

#endif // SCANNER_H
