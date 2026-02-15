# Preview Tree Structure Feature - Visual Guide

## UI Layout

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│  Lightroom Import Clone - WinAPI C                                                      │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                          │
│  ┌─ Source Folder ───┐  ┌─ Found Files ──┐  ┌─ Preview Structure ─┐                   │
│  │                    │  │                 │  │                      │                   │
│  │  📁 C:\            │  │  IMG_001.jpg    │  │  📁 C:\Photos (50)   │  Target Folder:  │
│  │  📁 D:\            │  │  IMG_002.jpg    │  │  ├─ 📁 2024\2024-... │  ┌─────────────┐ │
│  │  📁 E:\            │  │  IMG_003.jpg    │  │  │  └─ (12 files)   │  │ C:\Photos   │ │
│  │                    │  │  IMG_004.jpg    │  │  ├─ 📁 2024\2024-... │  └─────────────┘ │
│  │                    │  │  IMG_005.jpg    │  │  │  └─ (18 files)   │  [Browse...]     │
│  │                    │  │  ...            │  │  ├─ 📁 2024\2024-... │                   │
│  │                    │  │                 │  │  │  └─ (15 files)   │  Import Mode:    │
│  │                    │  │                 │  │  └─ 📁 2024\2024-... │  ⦿ Copy          │
│  │                    │  │                 │  │     └─ (5 files)    │  ○ Move          │
│  │                    │  │                 │  │                      │                   │
│  └────────────────────┘  └─────────────────┘  └──────────────────────┘  ☑ Organize by   │
│                                                                              Date        │
│  ☑ Recursive Scan        [Scan]                                                         │
│                                                                          Date Format:    │
│                                                                          ┌────────────┐  │
│                                                                          │YYYY/YYYY-..│  │
│                                                                          └────────────┘  │
│                                                                                          │
│                                                                          Custom Template:│
│                                                                          ┌────────────┐  │
│                                                                          │{year}/{...}│  │
│                                                                          └────────────┘  │
│                                                                                          │
│                                                                  [Preview Structure]     │
│                                                                  [Import Files]          │
│                                                                                          │
│  Progress: ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░                          │
│  Status: Preview generated: 50 files                                                    │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

## Feature Workflow

### 1. Initial State (Before Scan)
```
Source Folder: [Select a folder]
Found Files: [Empty]
Preview Structure: [Empty]
```

### 2. After Clicking "Scan"
```
Source Folder: C:\DCIM\
Found Files: 
  IMG_001.jpg
  IMG_002.jpg
  IMG_003.jpg
  [... 47 more files]

Preview Structure: [Automatically generated]
  📁 C:\Photos\Import (50 files)
  ├─ 📁 2024\2024-02-15 (12 files)
  ├─ 📁 2024\2024-02-14 (18 files)
  ├─ 📁 2024\2024-02-13 (15 files)
  └─ 📁 2024\2024-02-12 (5 files)

Status: Found 50 files
```

### 3. Changing Date Format
When user selects different format (e.g., "YYYY-MM"):
```
Preview Structure: [Auto-updates]
  📁 C:\Photos\Import (50 files)
  ├─ 📁 2024-02 (50 files)
```

### 4. Using Custom Template
When user enters "{year}/{month}/{day}":
```
Preview Structure: [Auto-updates]
  📁 C:\Photos\Import (50 files)
  ├─ 📁 2024\02\15 (12 files)
  ├─ 📁 2024\02\14 (18 files)
  ├─ 📁 2024\02\13 (15 files)
  └─ 📁 2024\02\12 (5 files)
```

### 5. Disabling "Organize by Date"
When user unchecks "Organize by Date":
```
Preview Structure: [Auto-updates]
  📁 C:\Photos\Import (50 files)
  └─ 📁 (root) (50 files)
```

## Key Benefits

### ✅ Instant Feedback
- Preview updates immediately as you change settings
- No need to perform actual import to see the structure

### ✅ Error Prevention
- Verify folder structure before committing
- Catch mistakes in template syntax
- See if organization matches expectations

### ✅ Clear Visualization
- Tree view makes hierarchy obvious
- File counts show distribution
- Easy to understand at a glance

### ✅ Zero Performance Impact
- Preview is calculated in memory
- No file operations performed
- Instant generation even with thousands of files

## Common Use Cases

### Case 1: Importing Photos from Camera
```
Source: F:\DCIM\100CANON\
Files: 250 photos from vacation

Preview shows:
  📁 D:\Photos\Vacation2024 (250 files)
  ├─ 📁 2024-02-15 (85 files)
  ├─ 📁 2024-02-16 (120 files)
  └─ 📁 2024-02-17 (45 files)

Result: Organized by shooting date
```

### Case 2: Archiving Old Photos
```
Source: C:\OldPhotos\
Files: 1500 photos from various years

Template: {year}/{month}

Preview shows:
  📁 E:\Archive (1500 files)
  ├─ 📁 2020\01 (45 files)
  ├─ 📁 2020\02 (67 files)
  ├─ 📁 2020\03 (89 files)
  ...
  └─ 📁 2024\02 (34 files)

Result: Organized by year and month
```

### Case 3: Simple Backup (No Organization)
```
Source: D:\CurrentProject\
Files: 500 files

Organize by Date: [Unchecked]

Preview shows:
  📁 F:\Backup (500 files)
  └─ 📁 (root) (500 files)

Result: All files in one folder
```

## Tips for Using Preview

1. **Always Check Preview Before Importing**
   - Verify the structure matches your expectations
   - Confirm file counts are correct
   - Check for unexpected folder names

2. **Experiment with Templates**
   - Try different templates to see results
   - Preview updates instantly
   - No risk since files aren't actually moved

3. **Use Preview to Validate Settings**
   - Ensure "Organize by Date" is enabled/disabled as intended
   - Verify date format produces desired structure
   - Check custom template syntax

4. **Monitor File Distribution**
   - Preview shows how many files go into each folder
   - Helps identify if dates are being read correctly
   - Useful for spotting files with incorrect timestamps

## Technical Notes

- Preview uses same logic as actual import
- Thread-safe operation (won't interfere with import)
- Memory efficient (linked list for unique folders)
- Updates automatically after scan
- Manual refresh available via button
