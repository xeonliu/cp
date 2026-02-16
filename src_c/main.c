#include "common.h"
#include "scanner.h"
#include "import.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

// Global application state
AppState g_app = {0};

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// UI creation functions
void CreateUI(HWND hwnd);
void OnScanClick();
void OnImportClick();
void OnBrowseTargetClick();
void OnPreviewClick();
void UpdatePreviewTree();
void PopulateTreeChildren(HTREEITEM hParent);
void FreeTreeItemRecursive(HTREEITEM hItem);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    
    // Initialize application state
    g_app.fileCapacity = 1000;
    g_app.files = (FileInfo*)malloc(g_app.fileCapacity * sizeof(FileInfo));
    g_app.fileCount = 0;
    g_app.isRecursive = true;
    g_app.organizeByDate = false;
    g_app.isMoving = false;
    g_app.useExifDate = false;
    g_app.dateFormatIndex = 0;
    g_app.stopRequested = false;
    wcscpy_s(g_app.customTemplate, 256, L"{year}/{year}-{month:02d}-{day:02d}");
    InitializeCriticalSection(&g_app.csFiles);
    GetCurrentDirectoryW(MAX_PATH_LEN, g_app.targetPath);
    
    // Initialize COM for WIC (Windows Imaging Component)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    // Register window class
    const wchar_t CLASS_NAME[] = L"LightroomImportClone";
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassW(&wc);
    
    // Create main window
    g_app.hwndMain = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Lightroom Import Clone - WinAPI C",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 700,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    
    if (g_app.hwndMain == NULL) {
        return 0;
    }
    
    CreateUI(g_app.hwndMain);
    
    ShowWindow(g_app.hwndMain, nCmdShow);
    UpdateWindow(g_app.hwndMain);
    
    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    CoUninitialize();
    DeleteCriticalSection(&g_app.csFiles);
    free(g_app.files);
    
    return 0;
}

void CreateUI(HWND hwnd) {
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    int y = 10;
    
    // Source section
    CreateWindowW(L"STATIC", L"Source Folder:", WS_VISIBLE | WS_CHILD,
                 10, y, 120, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndSourceTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
                                           WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
                                           10, y + 30, 280, 200, hwnd, (HMENU)ID_SOURCE_TREE, NULL, NULL);
    SendMessage(g_app.hwndSourceTree, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Add drives to tree with expandable folders
    wchar_t drives[256];
    GetLogicalDriveStringsW(256, drives);
    for (wchar_t* drive = drives; *drive; drive += wcslen(drive) + 1) {
        TVINSERTSTRUCT tvis = {0};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
        tvis.item.pszText = drive;
        tvis.item.cChildren = 1; // Indicate it has children (will load on expand)
        
        // Allocate and store full path
        TreeItemData* itemData = (TreeItemData*)malloc(sizeof(TreeItemData));
        wcscpy_s(itemData->fullPath, MAX_PATH_LEN, drive);
        itemData->childrenLoaded = false;
        tvis.item.lParam = (LPARAM)itemData;
        
        TreeView_InsertItem(g_app.hwndSourceTree, &tvis);
    }
    
    y += 240;
    
    g_app.hwndRecursiveCheck = CreateWindowW(L"BUTTON", L"Recursive Scan",
                                             WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                             10, y, 150, 25, hwnd, (HMENU)ID_RECURSIVE_CHECK, NULL, NULL);
    SendMessage(g_app.hwndRecursiveCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_app.hwndRecursiveCheck, BM_SETCHECK, BST_CHECKED, 0);
    
    HWND hScanBtn = CreateWindowW(L"BUTTON", L"Scan",
                                  WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                  170, y, 120, 30, hwnd, (HMENU)ID_SCAN_BUTTON, NULL, NULL);
    SendMessage(hScanBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // File list
    y = 10;
    CreateWindowW(L"STATIC", L"Found Files:", WS_VISIBLE | WS_CHILD,
                 310, y, 120, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndFileList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
                                         WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT,
                                         310, y + 30, 180, 360, hwnd, (HMENU)ID_FILE_LIST, NULL, NULL);
    SendMessage(g_app.hwndFileList, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Enable checkboxes in ListView
    ListView_SetExtendedListViewStyle(g_app.hwndFileList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    
    // Add column
    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = L"File";
    lvc.cx = 150;
    ListView_InsertColumn(g_app.hwndFileList, 0, &lvc);
    
    // Select All / Deselect All buttons (below file list)
    y += 395;
    HWND hSelectAllBtn = CreateWindowW(L"BUTTON", L"All",
                                       WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                       310, y, 85, 25, hwnd, (HMENU)ID_SELECT_ALL_BUTTON, NULL, NULL);
    SendMessage(hSelectAllBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    HWND hDeselectAllBtn = CreateWindowW(L"BUTTON", L"None",
                                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         405, y, 85, 25, hwnd, (HMENU)ID_DESELECT_ALL_BUTTON, NULL, NULL);
    SendMessage(hDeselectAllBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Preview tree aligned with file list (middle column)
    int previewY = 10;
    CreateWindowW(L"STATIC", L"Preview Structure:", WS_VISIBLE | WS_CHILD,
                 500, previewY, 200, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndPreviewTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
                                            WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
                                            500, previewY + 30, 210, 360, hwnd, (HMENU)ID_PREVIEW_TREE, NULL, NULL);
    SendMessage(g_app.hwndPreviewTree, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Target section
    y = 10;
    CreateWindowW(L"STATIC", L"Target Folder:", WS_VISIBLE | WS_CHILD,
                 730, y, 120, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndTargetEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_app.targetPath,
                                           WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                           730, y + 30, 350, 25, hwnd, (HMENU)ID_TARGET_EDIT, NULL, NULL);
    SendMessage(g_app.hwndTargetEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    HWND hBrowseBtn = CreateWindowW(L"BUTTON", L"Browse...",
                                    WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                    1090, y + 30, 90, 25, hwnd, (HMENU)ID_TARGET_BROWSE, NULL, NULL);
    SendMessage(hBrowseBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += 70;
    
    // Import options
    CreateWindowW(L"STATIC", L"Import Mode:", WS_VISIBLE | WS_CHILD,
                 730, y, 120, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndCopyRadio = CreateWindowW(L"BUTTON", L"Copy",
                                        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                                        730, y + 25, 80, 25, hwnd, (HMENU)ID_COPY_RADIO, NULL, NULL);
    SendMessage(g_app.hwndCopyRadio, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_app.hwndCopyRadio, BM_SETCHECK, BST_CHECKED, 0);
    
    g_app.hwndMoveRadio = CreateWindowW(L"BUTTON", L"Move",
                                        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                        820, y + 25, 80, 25, hwnd, (HMENU)ID_MOVE_RADIO, NULL, NULL);
    SendMessage(g_app.hwndMoveRadio, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += 60;
    
    g_app.hwndOrganizeCheck = CreateWindowW(L"BUTTON", L"Organize by Date",
                                            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                            730, y, 180, 25, hwnd, (HMENU)ID_ORGANIZE_CHECK, NULL, NULL);
    SendMessage(g_app.hwndOrganizeCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    g_app.hwndUseExifCheck = CreateWindowW(L"BUTTON", L"Use EXIF date (Vista+)",
                                           WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                           930, y, 200, 25, hwnd, (HMENU)ID_USE_EXIF_CHECK, NULL, NULL);
    SendMessage(g_app.hwndUseExifCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_app.hwndUseExifCheck, BM_SETCHECK, g_app.useExifDate ? BST_CHECKED : BST_UNCHECKED, 0);
    
    y += 30;
    
    CreateWindowW(L"STATIC", L"Date Format:", WS_VISIBLE | WS_CHILD,
                 730, y, 120, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndDateFormatCombo = CreateWindowW(L"COMBOBOX", L"",
                                              WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                              730, y + 25, 200, 200, hwnd, (HMENU)ID_DATE_FORMAT_COMBO, NULL, NULL);
    SendMessage(g_app.hwndDateFormatCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY-MM-DD");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY/MM/DD");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY-MM");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY/MM");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY/YYYY-MM-DD");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"YYYY-MM/DD");
    SendMessage(g_app.hwndDateFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"Custom");
    SendMessage(g_app.hwndDateFormatCombo, CB_SETCURSEL, 0, 0);
    
    y += 60;
    
    // Custom template input
    CreateWindowW(L"STATIC", L"Custom Template:", WS_VISIBLE | WS_CHILD,
                 730, y, 150, 25, hwnd, NULL, NULL, NULL);
    
    g_app.hwndCustomTemplateEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"{year}/{year}-{month:02d}-{day:02d}",
                                                   WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                                   730, y + 25, 350, 25, hwnd, (HMENU)ID_CUSTOM_TEMPLATE_EDIT, NULL, NULL);
    SendMessage(g_app.hwndCustomTemplateEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += 60;
    
    // Preview and Import buttons
    HWND hPreviewBtn = CreateWindowW(L"BUTTON", L"Preview Structure",
                                     WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                     730, y, 150, 40, hwnd, (HMENU)ID_PREVIEW_BUTTON, NULL, NULL);
    SendMessage(hPreviewBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    HWND hImportBtn = CreateWindowW(L"BUTTON", L"Import Files",
                                    WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                    890, y, 150, 40, hwnd, (HMENU)ID_IMPORT_BUTTON, NULL, NULL);
    SendMessage(hImportBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += 50;
    
    // Progress bar
    g_app.hwndProgressBar = CreateWindowExW(0, PROGRESS_CLASS, NULL,
                                            WS_VISIBLE | WS_CHILD,
                                            730, y, 450, 25, hwnd, (HMENU)ID_PROGRESS_BAR, NULL, NULL);
    
    y += 35;
    
    // Status text
    g_app.hwndStatusText = CreateWindowW(L"STATIC", L"Ready",
                                         WS_VISIBLE | WS_CHILD,
                                         730, y, 450, 25, hwnd, (HMENU)ID_STATUS_TEXT, NULL, NULL);
    SendMessage(g_app.hwndStatusText, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void FreeTreeItemRecursive(HTREEITEM hItem) {
    if (!hItem) return;
    
    // Use iterative approach with manual stack to avoid stack overflow on deep trees
    // Allocate item stack on heap (initial capacity: 256 items)
    int stackCapacity = 256;
    int stackSize = 0;
    HTREEITEM* itemStack = (HTREEITEM*)malloc(stackCapacity * sizeof(HTREEITEM));
    if (!itemStack) {
        // Out of memory - cannot clean up properly
        // Log error (this is called during cleanup, so just skip)
        OutputDebugStringW(L"FreeTreeItemRecursive: malloc failed, memory leak possible\n");
        return;
    }
    
    // Push initial item
    itemStack[stackSize++] = hItem;
    
    // Process items iteratively (depth-first)
    while (stackSize > 0) {
        // Pop item from stack
        HTREEITEM currentItem = itemStack[--stackSize];
        
        // Push all children to stack (process children before freeing parent)
        HTREEITEM hChild = TreeView_GetChild(g_app.hwndSourceTree, currentItem);
        while (hChild) {
            // Expand stack if needed
            if (stackSize >= stackCapacity) {
                int newCapacity = stackCapacity * 2;
                HTREEITEM* newStack = (HTREEITEM*)realloc(itemStack, newCapacity * sizeof(HTREEITEM));
                if (newStack) {
                    itemStack = newStack;
                    stackCapacity = newCapacity;
                } else {
                    // Out of memory - free what we can
                    break;
                }
            }
            
            itemStack[stackSize++] = hChild;
            hChild = TreeView_GetNextSibling(g_app.hwndSourceTree, hChild);
        }
        
        // Free this item's data
        TVITEMW tvi;
        tvi.mask = TVIF_PARAM;
        tvi.hItem = currentItem;
        if (TreeView_GetItem(g_app.hwndSourceTree, &tvi) && tvi.lParam) {
            free((TreeItemData*)tvi.lParam);
        }
    }
    
    free(itemStack);
}

void PopulateTreeChildren(HTREEITEM hParent) {
    // Get parent item data
    TVITEMW tvi;
    tvi.mask = TVIF_PARAM;
    tvi.hItem = hParent;
    TreeView_GetItem(g_app.hwndSourceTree, &tvi);
    
    TreeItemData* parentData = (TreeItemData*)tvi.lParam;
    if (!parentData || parentData->childrenLoaded) {
        return; // Already loaded
    }
    
    // Mark as loaded
    parentData->childrenLoaded = true;
    
    // Build search path
    wchar_t searchPath[MAX_PATH_LEN];
    swprintf_s(searchPath, MAX_PATH_LEN, L"%s*", parentData->fullPath);
    
    // Find subdirectories
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        // Skip . and ..
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        // Only add directories
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Skip hidden and system directories
            if (findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
                continue;
            }
            
            // Build full path
            wchar_t fullPath[MAX_PATH_LEN];
            swprintf_s(fullPath, MAX_PATH_LEN, L"%s%s\\", parentData->fullPath, findData.cFileName);
            
            // Check if this directory has subdirectories
            wchar_t checkPath[MAX_PATH_LEN];
            swprintf_s(checkPath, MAX_PATH_LEN, L"%s*", fullPath);
            WIN32_FIND_DATAW checkData;
            HANDLE hCheck = FindFirstFileW(checkPath, &checkData);
            int hasChildren = 0;
            
            if (hCheck != INVALID_HANDLE_VALUE) {
                do {
                    if (wcscmp(checkData.cFileName, L".") != 0 && 
                        wcscmp(checkData.cFileName, L"..") != 0 &&
                        (checkData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        !(checkData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
                        hasChildren = 1;
                        break;
                    }
                } while (FindNextFileW(hCheck, &checkData));
                FindClose(hCheck);
            }
            
            // Add to tree
            TVINSERTSTRUCT tvis = {0};
            tvis.hParent = hParent;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
            tvis.item.pszText = findData.cFileName;
            tvis.item.cChildren = hasChildren; // Only show expand button if has children
            
            // Allocate and store full path
            TreeItemData* itemData = (TreeItemData*)malloc(sizeof(TreeItemData));
            wcscpy_s(itemData->fullPath, MAX_PATH_LEN, fullPath);
            itemData->childrenLoaded = false;
            tvis.item.lParam = (LPARAM)itemData;
            
            TreeView_InsertItem(g_app.hwndSourceTree, &tvis);
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

void OnScanClick() {
    // Get selected tree item
    HTREEITEM hSelected = TreeView_GetSelection(g_app.hwndSourceTree);
    if (hSelected == NULL) {
        MessageBoxW(g_app.hwndMain, L"Please select a folder to scan", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get item data containing full path
    TVITEMW tvi;
    tvi.mask = TVIF_PARAM;
    tvi.hItem = hSelected;
    TreeView_GetItem(g_app.hwndSourceTree, &tvi);
    
    TreeItemData* itemData = (TreeItemData*)tvi.lParam;
    if (!itemData) {
        MessageBoxW(g_app.hwndMain, L"Invalid folder selection", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Update recursive setting
    g_app.isRecursive = (SendMessage(g_app.hwndRecursiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    // Start scan thread with full path
    wchar_t* pathCopy = _wcsdup(itemData->fullPath);
    g_app.hScanThread = CreateThread(NULL, 0, ScanThread, pathCopy, 0, NULL);
}

void OnImportClick() {
    if (g_app.fileCount == 0) {
        MessageBoxW(g_app.hwndMain, L"No files to import. Please scan first.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get target path
    GetWindowTextW(g_app.hwndTargetEdit, g_app.targetPath, MAX_PATH_LEN);
    
    if (wcslen(g_app.targetPath) == 0) {
        MessageBoxW(g_app.hwndMain, L"Please specify a target folder", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Update settings
    g_app.isMoving = (SendMessage(g_app.hwndMoveRadio, BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_app.organizeByDate = (SendMessage(g_app.hwndOrganizeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_app.useExifDate = (SendMessage(g_app.hwndUseExifCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_app.dateFormatIndex = (int)SendMessage(g_app.hwndDateFormatCombo, CB_GETCURSEL, 0, 0);
    
    // Get custom template
    GetWindowTextW(g_app.hwndCustomTemplateEdit, g_app.customTemplate, 256);
    
    // Start import thread
    g_app.hImportThread = CreateThread(NULL, 0, ImportThread, NULL, 0, NULL);
}

void OnBrowseTargetClick() {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = g_app.hwndMain;
    bi.lpszTitle = L"Select Target Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            SetWindowTextW(g_app.hwndTargetEdit, path);
            wcscpy_s(g_app.targetPath, MAX_PATH_LEN, path);
        }
        CoTaskMemFree(pidl);
    }
}

void OnPreviewClick() {
    UpdatePreviewTree();
}

// Structure to hold folder information for tree building
typedef struct FolderNode {
    wchar_t path[MAX_PATH_LEN];
    int fileCount;
    struct FolderNode* next;
} FolderNode;

void UpdatePreviewTree() {
    // Clear existing tree
    TreeView_DeleteAllItems(g_app.hwndPreviewTree);
    
    if (g_app.fileCount == 0) {
        SetWindowTextW(g_app.hwndStatusText, L"No files to preview. Please scan first.");
        return;
    }
    
    // Get current settings
    bool organizeByDate = (SendMessage(g_app.hwndOrganizeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    int dateFormatIndex = (int)SendMessage(g_app.hwndDateFormatCombo, CB_GETCURSEL, 0, 0);
    wchar_t customTemplate[256];
    GetWindowTextW(g_app.hwndCustomTemplateEdit, customTemplate, 256);
    
    // Get target path
    wchar_t targetPath[MAX_PATH_LEN];
    GetWindowTextW(g_app.hwndTargetEdit, targetPath, MAX_PATH_LEN);
    
    // Build folder structure
    FolderNode* folders = NULL;
    int totalFiles = 0;
    
    EnterCriticalSection(&g_app.csFiles);
    
    for (int i = 0; i < g_app.fileCount; i++) {
        FileInfo* file = &g_app.files[i];
        
        if (!file->isSelected) {
            continue;
        }
        
        wchar_t folderPath[MAX_PATH_LEN];
        
        if (organizeByDate) {
            wchar_t dateSubdir[256];
            
            // Use custom template if selected
            if (customTemplate && wcslen(customTemplate) > 0 && dateFormatIndex == DATE_FORMAT_CUSTOM) {
                GetDateSubdirectoryFromTemplate(&file->fileTime, customTemplate, dateSubdir, 256);
            } else {
                GetDateSubdirectory(&file->fileTime, dateFormatIndex, dateSubdir, 256);
            }
            
            if (wcslen(dateSubdir) > 0) {
                swprintf_s(folderPath, MAX_PATH_LEN, L"%s", dateSubdir);
            } else {
                wcscpy_s(folderPath, MAX_PATH_LEN, L"(root)");
            }
        } else {
            wcscpy_s(folderPath, MAX_PATH_LEN, L"(root)");
        }
        
        // Find or create folder node
        FolderNode* current = folders;
        FolderNode* prev = NULL;
        bool found = false;
        
        while (current != NULL) {
            if (wcscmp(current->path, folderPath) == 0) {
                current->fileCount++;
                found = true;
                break;
            }
            prev = current;
            current = current->next;
        }
        
        if (!found) {
            FolderNode* newNode = (FolderNode*)malloc(sizeof(FolderNode));
            wcscpy_s(newNode->path, MAX_PATH_LEN, folderPath);
            newNode->fileCount = 1;
            newNode->next = NULL;
            
            if (prev == NULL) {
                folders = newNode;
            } else {
                prev->next = newNode;
            }
        }
        
        totalFiles++;
    }
    
    LeaveCriticalSection(&g_app.csFiles);
    
    // Build tree view
    TVINSERTSTRUCT tvis = {0};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    
    // Add root node showing target path
    wchar_t rootText[MAX_PATH_LEN + 50];
    swprintf_s(rootText, MAX_PATH_LEN + 50, L"%s (%d files)", targetPath, totalFiles);
    tvis.item.pszText = rootText;
    HTREEITEM hRoot = TreeView_InsertItem(g_app.hwndPreviewTree, &tvis);
    
    // Add folder nodes
    FolderNode* current = folders;
    while (current != NULL) {
        wchar_t nodeText[MAX_PATH_LEN + 50];
        swprintf_s(nodeText, MAX_PATH_LEN + 50, L"%s (%d files)", current->path, current->fileCount);
        
        tvis.hParent = hRoot;
        tvis.item.pszText = nodeText;
        TreeView_InsertItem(g_app.hwndPreviewTree, &tvis);
        
        FolderNode* next = current->next;
        free(current);
        current = next;
    }
    
    // Expand root node
    TreeView_Expand(g_app.hwndPreviewTree, hRoot, TVE_EXPAND);
    
    // Update status
    wchar_t statusText[256];
    swprintf_s(statusText, 256, L"Preview generated: %d files", totalFiles);
    SetWindowTextW(g_app.hwndStatusText, statusText);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_NOTIFY: {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            
            // Handle tree view item expanding
            if (pnmhdr->idFrom == ID_SOURCE_TREE && pnmhdr->code == TVN_ITEMEXPANDING) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                if (pnmtv->action == TVE_EXPAND) {
                    PopulateTreeChildren(pnmtv->itemNew.hItem);
                }
            }
            
            if (pnmhdr->idFrom == ID_FILE_LIST && pnmhdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)lParam;
                if ((pnmlv->uChanged & LVIF_STATE) && 
                    ((pnmlv->uOldState & LVIS_STATEIMAGEMASK) != (pnmlv->uNewState & LVIS_STATEIMAGEMASK))) {
                    int fileIndex = (int)pnmlv->lParam;
                    if (fileIndex >= 0 && fileIndex < g_app.fileCount) {
                        g_app.files[fileIndex].isSelected =
                            ListView_GetCheckState(g_app.hwndFileList, pnmlv->iItem) ? true : false;
                        if (!g_app.suppressPreviewOnCheck) {
                            UpdatePreviewTree();
                        }
                    }
                }
            }
            break;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_SCAN_BUTTON:
                    OnScanClick();
                    break;
                case ID_IMPORT_BUTTON:
                    OnImportClick();
                    break;
                case ID_TARGET_BROWSE:
                    OnBrowseTargetClick();
                    break;
                case ID_PREVIEW_BUTTON:
                    OnPreviewClick();
                    break;
                case ID_SELECT_ALL_BUTTON:
                    g_app.suppressPreviewOnCheck = true;
                    EnterCriticalSection(&g_app.csFiles);
                    for (int i = 0; i < g_app.fileCount; i++) {
                        g_app.files[i].isSelected = true;
                        ListView_SetCheckState(g_app.hwndFileList, i, TRUE);
                    }
                    LeaveCriticalSection(&g_app.csFiles);
                    g_app.suppressPreviewOnCheck = false;
                    UpdatePreviewTree();
                    break;
                case ID_DESELECT_ALL_BUTTON:
                    g_app.suppressPreviewOnCheck = true;
                    EnterCriticalSection(&g_app.csFiles);
                    for (int i = 0; i < g_app.fileCount; i++) {
                        g_app.files[i].isSelected = false;
                        ListView_SetCheckState(g_app.hwndFileList, i, FALSE);
                    }
                    LeaveCriticalSection(&g_app.csFiles);
                    g_app.suppressPreviewOnCheck = false;
                    UpdatePreviewTree();
                    break;
                case ID_ORGANIZE_CHECK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        UpdatePreviewTree();
                    }
                    break;
                case ID_USE_EXIF_CHECK:
                    if (HIWORD(wParam) == BN_CLICKED) {
                        g_app.useExifDate = (SendMessage(g_app.hwndUseExifCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    }
                    break;
                case ID_DATE_FORMAT_COMBO:
                    // Auto-update preview when date format changes
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        UpdatePreviewTree();
                    }
                    break;
            }
            break;
            
        case WM_DESTROY:
            g_app.stopRequested = true;
            if (g_app.hScanThread) {
                WaitForSingleObject(g_app.hScanThread, 5000);
                CloseHandle(g_app.hScanThread);
            }
            if (g_app.hImportThread) {
                WaitForSingleObject(g_app.hImportThread, 5000);
                CloseHandle(g_app.hImportThread);
            }
            
            // Free all tree item data recursively
            HTREEITEM hItem = TreeView_GetRoot(g_app.hwndSourceTree);
            while (hItem) {
                HTREEITEM hNext = TreeView_GetNextSibling(g_app.hwndSourceTree, hItem);
                FreeTreeItemRecursive(hItem);
                hItem = hNext;
            }
            
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
