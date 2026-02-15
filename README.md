# Lightroom Import Clone

This project provides photo/video import and organization tools, similar to Adobe Lightroom's import functionality.

## Available Implementations

### 1. Qt6/C++ Version (Cross-Platform)
A full-featured cross-platform version with GUI preview and thumbnail support.

**Features:**
- Cross-platform (Windows, macOS, Linux)
- Preview images with thumbnails
- RAW file support via LibRaw
- Modern Qt6-based UI

See the main build instructions below for this version.

### 2. WinAPI C Version (Windows-Only, High Performance) ⚡
A lightweight, high-performance Windows-native implementation using pure WinAPI and C.

**Features:**
- Windows-only, optimized for maximum performance
- Pure C implementation with no external dependencies
- Memory-mapped file I/O for fast hash computation
- Native Windows threading
- ~100 KB executable size (vs 10-20 MB for Qt version)
- All core features except preview

📖 **See [README_C.md](README_C.md) for details and build instructions**

Build the C version:
```cmd
cd src_c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use NMAKE:
```cmd
nmake /f Makefile
```

---

## Qt6/C++ Version - Build Instructions

- Scan folders for photos and videos (recursive option).
- Generate thumbnails (using LibRaw for RAW files).
- Organize files by date (based on file modification time or EXIF).
- Import files (Copy/Move/Add) with duplicate detection (MD5 hash).
- Preview images.
- Cross-platform support (Windows, macOS, Linux).

### Prerequisites

- CMake 3.16+
- Qt 6.5+ (Core, Gui, Widgets)
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- LibRaw (optional, will be fetched automatically if not found)

### Build Steps

1. **Clone the repository:**
   ```bash
   git clone <repository_url>
   cd CP
   ```

2. **Configure:**
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   ```
   *Note: If Qt is not in your PATH, specify `-DCMAKE_PREFIX_PATH=/path/to/Qt`.*

3. **Build:**
   ```bash
   cmake --build build --config Release
   ```

4. **Run:**
   - On macOS: `open build/LightroomImportClone.app` or `./build/LightroomImportClone`
   - On Linux: `./build/LightroomImportClone`
   - On Windows: `build\Release\LightroomImportClone.exe`

## Dependencies

- **Qt6**: Used for GUI and core functionality.
- **LibRaw**: Used for processing RAW image files. It is automatically fetched via CMake if not found on the system.

## License

MIT
