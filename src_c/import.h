#ifndef IMPORT_H
#define IMPORT_H

#include "common.h"

// Import thread function
DWORD WINAPI ImportThread(LPVOID lpParam);

// Get date-based subdirectory from file time
void GetDateSubdirectory(const FILETIME* ft, int formatIndex, wchar_t* outPath, size_t outSize);

// Copy or move file with deduplication
bool ImportFile(const wchar_t* sourcePath, const wchar_t* targetBase, bool move, bool organizeByDate, int dateFormatIndex, const FILETIME* fileTime);

// Check if two files are identical by hash
bool FilesAreIdentical(const wchar_t* file1, const wchar_t* file2);

#endif // IMPORT_H
