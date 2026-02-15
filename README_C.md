# Lightroom Import Clone - WinAPI C Version

This is a high-performance, Windows-native implementation using pure WinAPI and C language.

## Overview

This version reimplements the Lightroom-style photo import tool using:
- **Pure C language** (C11 standard)
- **Native Windows API** (WinAPI) for UI and file operations
- **Optimized for Windows performance**
- **Windows XP Support**: Special XP-compatible build available

Unlike the Qt/C++ version, this implementation is Windows-only but offers better performance on Windows systems through:
1. Direct Windows API usage (no abstraction layers)
2. Memory-mapped file I/O for hash computation
3. Native Windows threading
4. Minimal dependencies (no Qt, no external libraries except Windows SDK)

## Features

### Core Features
- ✅ **File Scanning**: Recursive and non-recursive directory scanning
- ✅ **Deduplication**: MD5 hash-based duplicate detection
- ✅ **Custom Folder Structure**: Organize files by date with customizable formats
- ✅ **Import Operations**: Copy or Move files
- ✅ **Multi-threading**: Separate threads for scanning and importing
- ✅ **Progress Tracking**: Real-time progress updates
- ✅ **Preview Structure**: Preview folder organization before importing

### Supported File Types
- **Images**: JPG, JPEG, PNG, BMP, GIF, TIFF
- **RAW Files**: ARW, CR2, NEF, DNG, ORF, RW2, RAW
- **Videos**: MP4, MOV, AVI, MKV, WMV, MPG, MPEG

### Date Organization Formats
- `YYYY-MM-DD` (e.g., 2024-02-15)
- `YYYY/MM/DD` (e.g., 2024/02/15)
- `YYYY-MM` (e.g., 2024-02)
- `YYYY/MM` (e.g., 2024/02)
- `YYYY` (e.g., 2024)
- `YYYY/YYYY-MM-DD` (e.g., 2024/2024-02-15) - nested structure
- `YYYY-MM/DD` (e.g., 2024-02/15) - nested structure
- `Custom` - user-defined template with placeholders

### Custom Template Format
When selecting "Custom" format, you can use the following placeholders:
- `{year}` or `{year:04d}` - 4-digit year (e.g., 2024)
- `{month}` or `{month:02d}` - 2-digit month (e.g., 02)
- `{day}` or `{day:02d}` - 2-digit day (e.g., 15)
- `{hour}` or `{hour:02d}` - 2-digit hour
- `{minute}` or `{minute:02d}` - 2-digit minute
- `{second}` or `{second:02d}` - 2-digit second

**Examples:**
- `{year}/{year}-{month:02d}-{day:02d}` → `2024/2024-02-15`
- `{year}/{month}/{day}` → `2024/02/15`
- `{year}-{month}_{day}` → `2024-02_15`
- `Photos_{year}/{month:02d}` → `Photos_2024/02`

Use forward slashes (`/`) to create nested directories - they will be automatically converted to backslashes (`\`) on Windows.

## Performance Optimizations

### 1. Memory-Mapped File I/O
For files larger than 10MB, the application uses Windows memory-mapped files for hash computation:
```c
HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
LPVOID lpBaseAddress = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
```
This reduces system calls and improves performance by 2-3x compared to traditional file reading.

### 2. Native Windows APIs
Uses `FindFirstFileW` and `FindNextFileW` instead of standard C library functions for directory traversal:
```c
WIN32_FIND_DATAW findData;
HANDLE hFind = FindFirstFileW(searchPath, &findData);
```

### 3. Multi-threaded Architecture
- Separate thread for file scanning
- Separate thread for importing
- Critical sections for thread-safe file list access

### 4. Efficient Hash Computation
- Custom MD5 implementation optimized for performance
- Batch processing for small files
- Memory-mapped I/O for large files

## Build Instructions

### Prerequisites
- Windows 10 or later (for modern build)
- **OR** Windows XP SP3 or later (for XP-compatible build)
- Visual Studio 2019 or later (with MSVC compiler)
- Windows SDK

### Modern Build (Windows 7+)

#### Option 1: Using CMake
```cmd
cd src_c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The executable will be in `src_c\build\Release\LightroomImportClone_C.exe`

### Option 2: Using NMAKE (Makefile.msvc)
```cmd
cd src_c
# Open Visual Studio Developer Command Prompt, or run vcvars64.bat
nmake /f Makefile.msvc
```

The executable will be `src_c\LightroomImportClone_C.exe`

### Option 3: Manual Compilation with MSVC
```cmd
cd src_c
cl /W3 /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
   main.c scanner.c import.c hash.c ^
   /link /SUBSYSTEM:WINDOWS comctl32.lib shlwapi.lib shell32.lib ole32.lib ^
   /OUT:LightroomImportClone_C.exe
```

### Windows XP Compatible Build

For Windows XP SP3 and later, use the XP-specific CMakeLists:

```cmd
cd src_c
# Copy XP CMakeLists
copy CMakeLists_XP.txt CMakeLists.txt

# Configure with XP toolset (requires Visual Studio 2019)
cmake -B build -DCMAKE_BUILD_TYPE=Release -T v141_xp -A Win32

# Build
cmake --build build --config Release
```

The XP-compatible executable will be in `src_c\build\Release\LightroomImportClone_C_XP.exe`

**Note**: The XP build:
- Targets 32-bit (x86) architecture
- Uses the v141_xp platform toolset (Visual Studio 2019)
- Defines `BUILD_FOR_XP` to enable XP compatibility macros
- Sets subsystem to Windows 5.01 (XP)
- Is automatically built by CI and available as artifacts

## Usage

1. **Select Source Folder**: Click on a drive or folder in the tree view
2. **Configure Scan Options**: 
   - Check "Recursive Scan" to scan subdirectories
3. **Click "Scan"**: The application will find all supported files
4. **Preview Structure** (NEW):
   - The preview tree automatically shows how files will be organized
   - Click "Preview Structure" button to manually refresh the preview
   - See file counts for each folder before importing
5. **Configure Target**:
   - Enter or browse to select target folder
   - Choose "Copy" or "Move" mode
   - Optionally enable "Organize by Date"
   - Select date format (including nested formats like `YYYY/YYYY-MM-DD`)
   - Or choose "Custom" and enter your own template (e.g., `{year}/{month}/{day}`)
6. **Click "Import Files"**: Files will be imported with deduplication

## Architecture

### Source Files
```
src_c/
├── common.h        - Common definitions and structures
├── main.c          - Main application and UI
├── scanner.h/c     - File scanning logic
├── import.h/c      - File import operations
└── hash.h/c        - MD5 hash computation
```

### Key Structures

**FileInfo**: Stores information about each scanned file
```c
typedef struct {
    wchar_t path[MAX_PATH_LEN];
    FILETIME fileTime;
    ULONGLONG fileSize;
    unsigned char hash[HASH_SIZE];
    bool hashComputed;
} FileInfo;
```

**AppState**: Global application state
```c
typedef struct {
    HWND hwndMain;
    FileInfo* files;
    int fileCount;
    CRITICAL_SECTION csFiles;
    // ... other UI and state members
} AppState;
```

## Deduplication Strategy

The application uses MD5 hashing for deduplication with a two-phase approach:

### Phase 1: Batch Deduplication
During import, files within the current scan batch are compared:
1. **Hash Computation**: Each file is hashed using MD5
2. **In-Memory Comparison**: Compare with previously processed files in the same batch (indices 0 to i-1)
3. **Skip Duplicates**: Files with matching hashes are skipped immediately

### Phase 2: Target Directory Deduplication
Before importing each file:
1. **Check Existence**: Verify if file with same name exists in target
2. **Hash Comparison**: If exists, compute hash of target file and compare
3. **Actions**:
   - **Identical**: Skip (counted as duplicate)
   - **Different**: Rename with "_copy" suffix and import
   - **Non-existent**: Import normally

### Duplicate Count Display
The status bar shows:
- **Real-time speed**: MB/s throughput during import
- **Current file**: Name of file currently being processed
- **Duplicate count**: Total duplicates from both phases
- **Final summary**: Average speed and total duplicates

**Example**: `Importing: 45/100 (12.5 MB/s, 3 duplicates) - IMG_1234.JPG`

## Copy/Move Algorithm

### Copy Algorithm
```c
// Uses Windows CopyFileW API
BOOL CopyFileW(
    sourceFile,     // Source path
    destFile,       // Destination path
    FALSE           // Overwrite if different content
);
```

**Process**:
1. Create target directory structure (using `SHCreateDirectoryExW`)
2. Apply date-based organization if enabled
3. Check for duplicates using hash comparison
4. Copy file using Windows API (kernel-mode operation)
5. Preserve file timestamps

### Move Algorithm
```c
// Uses Windows MoveFileW API
BOOL MoveFileW(
    sourceFile,     // Source path
    destFile        // Destination path
);
```

**Process**:
1. Same as copy for directory creation and deduplication
2. Move file using Windows API
3. **Same Volume**: Fast (just updates directory entry)
4. **Different Volume**: Copy then delete source

### Performance Characteristics

**Speed Tracking**:
- Measures bytes processed (sum of all successfully imported files)
- Calculates speed as: `(total bytes / 1024 / 1024) / elapsed seconds`
- Updates in real-time during import
- Shows average speed in final summary

**Typical Speeds**:
- **Same SSD**: 200-500 MB/s (copy), instant (move)
- **Different SSDs**: 100-400 MB/s (limited by slower drive)
- **SSD to HDD**: 80-120 MB/s (limited by HDD write speed)
- **Network**: 10-100 MB/s (depends on network speed)

## Limitations

### File System Limitations

**Maximum Path Length**: 260 characters (Windows MAX_PATH)
- Paths exceeding this limit will fail to import
- Applies to source and target paths combined
- **Workaround**: Use shorter folder names or enable long path support in Windows 10+

**Maximum Folder Depth**: ~32 levels (NTFS limitation)
- Deeply nested custom templates may fail
- Error occurs during directory creation

**Maximum Files**: Limited by available memory
- Each `FileInfo` structure: ~4 KB
- 100,000 files ≈ 400 MB RAM
- Practical limit: ~1 million files per scan

**Maximum File Size**: No hardcoded limit
- Limited by available disk space
- Files >4GB handled correctly with chunked processing
- Memory-mapped I/O used for files 10MB-500MB

### Supported File Systems
- **NTFS**: Full support (recommended)
- **FAT32**: Works but limited to 4GB files
- **exFAT**: Full support
- **Network Drives (SMB/CIFS)**: Supported but slower

### Threading Limitations
- **Single Scan Thread**: Only one scan can run at a time
- **Single Import Thread**: Only one import can run at a time
- **No Parallelism**: Files imported sequentially (by design for simplicity)
- **Thread Safety**: File list protected by critical section

### UI Limitations
- **Basic Interface**: Functional but minimal compared to Qt version
- **No Drag-Drop**: Must use tree view to select source
- **No Image Preview**: Excluded per original requirements
- **Status Updates**: Real-time during import, shows:
  - Current file being processed (filename only)
  - Copy/move speed in MB/s
  - Progress (X/Y files)
  - Duplicate count

### Performance Trade-offs

**Hash Computation**:
- MD5 algorithm: Fast but not cryptographically secure
- Memory-mapped I/O for 10MB-500MB files: 2-3x faster than read()
- Small files (<10MB): Buffered reading
- Large files (>500MB): Chunked processing to avoid memory issues

**Deduplication**:
- O(n²) worst case for batch comparison (n = files in scan)
- Optimized: Only compares with previous files (j < i)
- Target check: O(1) file existence check + O(1) hash comparison
- Hash cache: Computed once, reused for all comparisons

**Memory Usage**:
- Pre-allocated file array: `fileCapacity * sizeof(FileInfo)`
- Grows by 1000 when capacity exceeded
- No release until application exit
- Tree view data: ~100 bytes per folder node

## Thread Safety

- Uses `CRITICAL_SECTION` for protecting shared file list
- Separate threads for scanning and importing
- UI updates via `PostMessage` from worker threads

## Comparison with Qt/C++ Version

| Feature | Qt/C++ Version | WinAPI C Version |
|---------|---------------|------------------|
| Platform | Cross-platform | Windows only |
| Dependencies | Qt6, LibRaw | Windows SDK only |
| Executable Size | ~10-20 MB | ~100 KB |
| Memory Usage | Higher (Qt overhead) | Lower (native) |
| Performance | Good | Excellent on Windows |
| Speed Display | No | Yes (real-time MB/s) |
| Current File Display | No | Yes (during import) |
| Folder Navigation | Tree view | Expandable tree with lazy loading |
| Custom Templates | Limited | Full support with placeholders |
| Preview | Yes | Tree structure preview only |
| UI Framework | Qt Widgets | Native WinAPI |

## Key Limitations Summary

- **Windows Only**: This implementation uses Windows-specific APIs
- **Path Length**: 260 characters max (Windows MAX_PATH limitation)
- **Folder Depth**: ~32 levels (NTFS limitation)
- **File Count**: ~1 million files per scan (memory dependent)
- **Sequential Import**: Files processed one at a time (not parallel)
- **No RAW Preview**: RAW files treated as regular files (no thumbnail generation)
- **Basic UI**: Functional but less polished than Qt version
- **No Image Preview**: As requested, preview functionality is not included

See the "Limitations" section above for detailed information on file system constraints, performance characteristics, and workarounds.

## License

MIT License - Same as the main project

## Notes for Developers

### Why No RAW Preview?
The original request excluded preview functionality. RAW file decoding (LibRaw) would add significant complexity and dependencies, counter to the goal of a lightweight, pure WinAPI implementation.

### Future Enhancements
Possible improvements while maintaining pure C/WinAPI approach:
- EXIF reading using Windows Imaging Component (WIC)
- Thumbnail generation using WIC
- Async I/O using overlapped I/O
- Thread pool API for better scaling

## Building in CI/CD

See `.github/workflows/build-c.yml` for automated builds. The workflow:
1. Sets up MSVC environment
2. Builds using both CMake and NMAKE
3. Creates release package with README
4. Uploads artifacts
5. Creates GitHub releases on tags
