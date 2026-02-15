# Lightroom Import Clone (C++ / Qt6)

This project is a migration of the Python-based Lightroom Import Clone to C++17 using Qt6.

## Features

- Scan folders for photos and videos (recursive option).
- Generate thumbnails (using LibRaw for RAW files).
- Organize files by date (based on file modification time or EXIF).
- Import files (Copy/Move/Add) with duplicate detection (MD5 hash).
- Preview images.
- Cross-platform support (Windows, macOS, Linux).

## Build Instructions

### Prerequisites

- CMake 3.16+
- Qt 6.5+ (Core, Gui, Widgets)
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- LibRaw (optional, will be fetched automatically if not found)

### Steps

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
