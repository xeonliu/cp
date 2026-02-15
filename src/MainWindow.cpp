#include "MainWindow.h"
#include "Utils.h"
#include <QApplication>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QStorageInfo>
#include <QDebug>
#include <QStyle>
#include <QRadioButton>
#include <QStandardPaths>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), 
    target_directory(QDir::homePath()),
    thumbnails_enabled(false),
    updating_tree(false),
    last_import_count(0),
    last_skipped_duplicates(0),
    date_format("YYYY-MM-DD"),
    time_source("file_mtime")
{
    setWindowTitle("Lightroom Import Clone (Qt6 C++)");
    resize(1400, 800);
    
    // Setup UI
    setup_ui();
    
    // Init Threads
    init_threads();
    
    // Timers
    device_timer = new QTimer(this);
    connect(device_timer, &QTimer::timeout, this, &MainWindow::refresh_devices);
    device_timer->start(3000);
    refresh_devices();
    
    thumbnail_check_timer = new QTimer(this);
    thumbnail_check_timer->setSingleShot(true);
    connect(thumbnail_check_timer, &QTimer::timeout, this, &MainWindow::update_visible_thumbnails);
    
    preview_refresh_timer = new QTimer(this);
    preview_refresh_timer->setSingleShot(true);
    connect(preview_refresh_timer, &QTimer::timeout, this, &MainWindow::refresh_target_tree);
}

MainWindow::~MainWindow() {
    scanner->stop();
    thumb_worker->stop();
    import_worker->stop();
    
    scan_thread.quit();
    scan_thread.wait();
    
    thumb_thread.quit();
    thumb_thread.wait();
    
    import_thread.quit();
    import_thread.wait();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QMainWindow::closeEvent(event);
}

void MainWindow::init_threads() {
    scanner = new ScannerWorker();
    scanner->moveToThread(&scan_thread);
    connect(&scan_thread, &QThread::finished, scanner, &QObject::deleteLater);
    connect(scanner, &ScannerWorker::files_found, this, &MainWindow::on_files_found);
    connect(scanner, &ScannerWorker::finished, this, &MainWindow::on_scan_finished);
    scan_thread.start();
    
    thumb_worker = new ThumbnailWorker();
    thumb_worker->moveToThread(&thumb_thread);
    connect(&thumb_thread, &QThread::finished, thumb_worker, &QObject::deleteLater);
    connect(thumb_worker, &ThumbnailWorker::finished, this, &MainWindow::on_thumbnail_ready);
    thumb_thread.start();
    
    import_worker = new ImportWorker();
    import_worker->moveToThread(&import_thread);
    connect(&import_thread, &QThread::finished, import_worker, &QObject::deleteLater);
    connect(import_worker, &ImportWorker::progress, this, &MainWindow::on_import_progress);
    connect(import_worker, &ImportWorker::finished, this, &MainWindow::on_import_finished);
    import_thread.start();
}

void MainWindow::setup_ui() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    
    main_layout->addWidget(create_top_bar());
    
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(2);
    splitter->addWidget(create_left_panel());
    splitter->addWidget(create_center_panel());
    splitter->addWidget(create_right_panel());
    
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setStretchFactor(2, 1);
    
    main_layout->addWidget(splitter);
    main_layout->addWidget(create_bottom_bar());
    
    // Apply Stylesheet (Simplified version of Python one)
    setStyleSheet(R"(
        QMainWindow { background-color: #1e1e1e; color: #d3d3d3; font-family: "Segoe UI", Arial; }
        QWidget { background-color: #1e1e1e; color: #d3d3d3; font-size: 13px; }
        QTreeView { background-color: #252525; border: none; outline: none; }
        QTreeView::item { padding: 4px; }
        QTreeView::item:selected { background-color: #4f4f4f; color: white; }
        QTreeView::item:hover { background-color: #333; }
        QListWidget { background-color: #1b1b1b; border: none; outline: none; }
        QListWidget::item { background-color: #222; border-radius: 4px; margin: 5px; }
        QListWidget::item:selected { background-color: #333; border: 1px solid #888; }
        QListWidget::item:hover { background-color: #2a2a2a; }
        QPushButton { background-color: #333; border: 1px solid #555; border-radius: 3px; padding: 5px 15px; color: #ccc; }
        QPushButton:hover { background-color: #444; }
        QPushButton:checked { background-color: #555; color: white; border-color: #888; }
        QPushButton#ImportBtn { background-color: #4CAF50; color: white; font-weight: bold; border: none; }
        QPushButton#ImportBtn:hover { background-color: #45a049; }
        QSplitter::handle { background-color: #111; }
        QGroupBox { border: 1px solid #444; margin-top: 20px; font-weight: bold; padding-top: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #aaa; }
        QScrollArea { border: none; }
    )");
}

QWidget* MainWindow::create_top_bar() {
    QWidget *container = new QWidget(this);
    container->setFixedHeight(45);
    container->setStyleSheet("background-color: #252525; border-bottom: 1px solid #111;");
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setAlignment(Qt::AlignCenter);
    
    mode_group = new QButtonGroup(this);
    QStringList modes = {"复制 (Copy)", "移动 (Move)", "添加 (Add)", "复制为 DNG"};
    for (int i = 0; i < modes.size(); ++i) {
        QPushButton *btn = new QPushButton(modes[i], container);
        btn->setCheckable(true);
        btn->setStyleSheet(R"(
            QPushButton { border: none; font-weight: bold; color: #999; font-size: 14px; margin: 0 10px; }
            QPushButton:checked { color: white; text-decoration: underline; }
        )");
        if (i == 0) btn->setChecked(true);
        mode_group->addButton(btn, i);
        layout->addWidget(btn);
    }
    
    return container;
}

QWidget* MainWindow::create_left_panel() {
    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    
    QLabel *dev_title = new QLabel(" 设备 (Devices)", container);
    dev_title->setFixedHeight(30);
    dev_title->setStyleSheet("font-weight: bold; background-color: #333; color: #eee; padding-left: 5px;");
    layout->addWidget(dev_title);
    
    device_list = new QListWidget(container);
    device_list->setMaximumHeight(150);
    device_list->setStyleSheet("background-color: #252525; border: none;");
    connect(device_list, &QListWidget::itemClicked, this, &MainWindow::on_device_clicked);
    layout->addWidget(device_list);
    
    // Folder Title
    QWidget *title_frame = new QFrame(container);
    title_frame->setFixedHeight(30);
    title_frame->setStyleSheet("background-color: #333;");
    QHBoxLayout *folder_title_layout = new QHBoxLayout(title_frame);
    folder_title_layout->setContentsMargins(5, 0, 5, 0);
    
    QLabel *folder_title = new QLabel(" 文件夹 (Folders)", title_frame);
    folder_title->setStyleSheet("font-weight: bold; color: #eee;");
    folder_title_layout->addWidget(folder_title);
    folder_title_layout->addStretch();
    
    recursive_check = new QCheckBox("包含子文件夹", title_frame);
    recursive_check->setStyleSheet("color: #aaa; font-size: 11px;");
    connect(recursive_check, &QCheckBox::toggled, this, &MainWindow::on_recursive_toggled);
    folder_title_layout->addWidget(recursive_check);
    
    layout->addWidget(title_frame);
    
    fs_model = new QFileSystemModel(this);
    fs_model->setRootPath(QDir::rootPath());
    fs_model->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Drives);
    
    tree_view = new QTreeView(container);
    tree_view->setModel(fs_model);
    tree_view->hideColumn(1);
    tree_view->hideColumn(2);
    tree_view->hideColumn(3);
    tree_view->setHeaderHidden(true);
    connect(tree_view, &QTreeView::clicked, this, &MainWindow::on_folder_clicked);
    
    layout->addWidget(tree_view);
    
    return container;
}

QWidget* MainWindow::create_center_panel() {
    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    
    scroll_area = new QScrollArea(container);
    scroll_area->setWidgetResizable(true);
    scroll_area->setStyleSheet("QScrollArea { border: none; background-color: #1b1b1b; }");
    connect(scroll_area->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::schedule_thumbnail_check);
    
    scroll_content = new QWidget(scroll_area);
    scroll_content->setStyleSheet("background-color: #1b1b1b;");
    scroll_layout = new QVBoxLayout(scroll_content);
    scroll_layout->setContentsMargins(10, 10, 10, 10);
    scroll_layout->setSpacing(10);
    scroll_layout->setAlignment(Qt::AlignTop);
    
    scroll_area->setWidget(scroll_content);
    layout->addWidget(scroll_area);
    
    return container;
}

QWidget* MainWindow::create_right_panel() {
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget *content = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setAlignment(Qt::AlignTop);
    
    // Preview
    QGroupBox *preview_group = new QGroupBox("预览", content);
    QVBoxLayout *preview_layout = new QVBoxLayout(preview_group);
    
    preview_label = new QLabel(preview_group);
    preview_label->setFixedSize(200, 160);
    preview_label->setStyleSheet("background-color: #333; border: 1px solid #555;");
    preview_label->setAlignment(Qt::AlignCenter);
    
    file_info_label = new QLabel("未选择文件", preview_group);
    file_info_label->setWordWrap(true);
    file_info_label->setStyleSheet("color: #aaa; font-size: 11px;");
    
    preview_layout->addWidget(preview_label);
    preview_layout->addWidget(file_info_label);
    layout->addWidget(preview_group);
    
    // Options
    QGroupBox *options_group = new QGroupBox("导入设置", content);
    QVBoxLayout *options_layout = new QVBoxLayout(options_group);
    
    mode_label = new QLabel("模式: 复制", options_group);
    mode_label->setStyleSheet("color: #ccc;");
    options_layout->addWidget(mode_label);
    
    organize_checkbox = new QCheckBox("按日期组织文件", options_group);
    organize_checkbox->setChecked(true);
    connect(organize_checkbox, &QCheckBox::checkStateChanged, this, &MainWindow::update_grid_filter);
    options_layout->addWidget(organize_checkbox);
    
    thumbnail_checkbox = new QCheckBox("生成缩略图（可能较慢）", options_group);
    thumbnail_checkbox->setChecked(false);
    connect(thumbnail_checkbox, &QCheckBox::checkStateChanged, this, &MainWindow::on_thumbnail_toggle_changed);
    options_layout->addWidget(thumbnail_checkbox);
    
    QHBoxLayout *date_format_layout = new QHBoxLayout();
    date_format_layout->addWidget(new QLabel("日期格式:", options_group));
    date_format_combo = new QComboBox(options_group);
    date_format_combo->addItems({"YYYY-MM-DD", "YYYY/MM/DD", "YYYY-MM", "YYYY/MM", "YYYYMMDD", "YYYY"});
    connect(date_format_combo, &QComboBox::currentTextChanged, this, &MainWindow::on_date_format_changed);
    date_format_layout->addWidget(date_format_combo);
    options_layout->addLayout(date_format_layout);
    
    options_layout->addWidget(new QLabel("时间源:", options_group));
    QHBoxLayout *time_source_layout = new QHBoxLayout();
    time_source_group = new QButtonGroup(this);
    
    QRadioButton *radio_file = new QRadioButton("文件修改时间", options_group);
    radio_file->setChecked(true);
    time_source_group->addButton(radio_file, 0);
    time_source_layout->addWidget(radio_file);
    
    QRadioButton *radio_exif = new QRadioButton("拍摄时间 (EXIF)", options_group);
    time_source_group->addButton(radio_exif, 1);
    time_source_layout->addWidget(radio_exif);
    connect(time_source_group, &QButtonGroup::buttonClicked, this, &MainWindow::on_time_source_changed);
    
    options_layout->addLayout(time_source_layout);
    
    options_layout->addWidget(new QLabel("自定义模板 (可选):", options_group));
    template_input = new QLineEdit(options_group);
    template_input->setPlaceholderText("例: {year}/{month:02d}/{day:02d}");
    template_input->setStyleSheet("background-color: #222; border: 1px solid #444; padding: 4px;");
    connect(template_input, &QLineEdit::textChanged, this, &MainWindow::on_template_changed);
    options_layout->addWidget(template_input);
    
    layout->addWidget(options_group);
    
    // Target
    QGroupBox *target_group = new QGroupBox("目标位置", content);
    QVBoxLayout *target_layout = new QVBoxLayout(target_group);
    
    QWidget *path_frame = new QWidget(target_group);
    QHBoxLayout *path_layout = new QHBoxLayout(path_frame);
    path_layout->setContentsMargins(0, 0, 0, 0);
    
    target_label = new QLabel(target_directory, path_frame);
    target_label->setWordWrap(true);
    target_label->setStyleSheet("color: #888; font-size: 10px;");
    
    QPushButton *browse_btn = new QPushButton("浏览...", path_frame);
    browse_btn->setMaximumWidth(80);
    connect(browse_btn, &QPushButton::clicked, this, &MainWindow::on_browse_target);
    
    path_layout->addWidget(target_label, 1);
    path_layout->addWidget(browse_btn);
    
    target_tree = new QTreeWidget(target_group);
    target_tree->setHeaderLabel("文件夹结构");
    target_tree->setMaximumHeight(250);
    target_tree->setStyleSheet("QTreeWidget { background-color: #222; border: 1px solid #444; }");
    connect(target_tree, &QTreeWidget::itemChanged, this, &MainWindow::on_tree_item_changed);
    
    target_layout->addWidget(new QLabel("路径:", target_group));
    target_layout->addWidget(path_frame);
    target_layout->addWidget(new QLabel("目录结构:", target_group));
    target_layout->addWidget(target_tree);
    
    layout->addWidget(target_group);
    
    // Stats
    QGroupBox *stats_group = new QGroupBox("统计", content);
    QVBoxLayout *stats_layout = new QVBoxLayout(stats_group);
    stats_label = new QLabel("已选择: 0 文件", stats_group);
    stats_label->setStyleSheet("color: #aaa;");
    stats_layout->addWidget(stats_label);
    layout->addWidget(stats_group);
    
    scroll->setWidget(content);
    return scroll;
}

QWidget* MainWindow::create_bottom_bar() {
    QWidget *container = new QWidget(this);
    container->setFixedHeight(50);
    container->setStyleSheet("background-color: #252525; border-top: 1px solid #333;");
    QHBoxLayout *layout = new QHBoxLayout(container);
    
    status_label = new QLabel("就绪", container);
    status_label->setStyleSheet("color: #aaa;");
    
    progress_bar = new QProgressBar(container);
    progress_bar->setFixedWidth(250);
    progress_bar->setVisible(false);
    
    import_btn = new QPushButton("导入", container);
    import_btn->setObjectName("ImportBtn");
    import_btn->setFixedSize(100, 32);
    connect(import_btn, &QPushButton::clicked, this, &MainWindow::on_import_clicked);
    
    layout->addWidget(status_label);
    layout->addWidget(progress_bar);
    layout->addStretch();
    layout->addWidget(import_btn);
    
    return container;
}

void MainWindow::refresh_devices() {
    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    device_list->clear();
    
    for (const QStorageInfo &storage : volumes) {
        if (storage.isValid() && storage.isReady()) {
            QString name = storage.name();
            if (name.isEmpty()) name = storage.rootPath();
            
            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, storage.rootPath());
            item->setIcon(style()->standardIcon(QStyle::SP_DriveHDIcon));
            device_list->addItem(item);
        }
    }
}

void MainWindow::on_device_clicked(QListWidgetItem *item) {
    QString path = item->data(Qt::UserRole).toString();
    QModelIndex index = fs_model->setRootPath(path);
    tree_view->setRootIndex(index);
    tree_view->clearSelection();
    start_scan(path);
}

void MainWindow::on_folder_clicked(const QModelIndex &index) {
    QString path = fs_model->filePath(index);
    device_list->clearSelection();
    start_scan(path);
}

void MainWindow::start_scan(const QString &path) {
    if (path.isEmpty()) return;
    
    status_label->setText("正在扫描: " + path + "...");
    
    // Stop old scan/thumb
    scanner->stop();
    thumb_worker->stop();
    
    // Clear UI
    QLayoutItem *item;
    while ((item = scroll_layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    
    date_sections.clear();
    path_to_item.clear();
    current_files.clear();
    selected_items.clear();
    duplicate_files.clear();
    path_cache.clear();
    loaded_thumbnails.clear();
    loading_thumbnails.clear();
    
    update_preview("");
    
    QMetaObject::invokeMethod(scanner, "scan", Q_ARG(QString, path), Q_ARG(bool, recursive_check->isChecked()));
}

void MainWindow::on_files_found(const QStringList &files) {
    scroll_area->setUpdatesEnabled(false);
    
    for (const QString &path : files) {
        current_files.append(path);
        
        QFileInfo fi(path);
        QString date_str = fi.lastModified().toString("yyyy-MM-dd");
        
        if (!date_sections.contains(date_str)) {
            DateSection *section = new DateSection(date_str);
            connect(section, &DateSection::selection_changed, this, &MainWindow::on_grid_selection_changed);
            connect(section, &DateSection::thumbnail_clicked, this, &MainWindow::update_preview);
            scroll_layout->addWidget(section);
            date_sections[date_str] = section;
        }
        
        DateSection *section = date_sections[date_str];
        QListWidgetItem *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, path);
        item->setData(Qt::UserRole + 1, false); // thumb loaded flag
        
        QPixmap default_pix(100, 100);
        default_pix.fill(QColor("#333"));
        item->setIcon(QIcon(default_pix));
        
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        
        section->add_item(item);
        path_to_item[path] = item;
    }
    
    scroll_area->setUpdatesEnabled(true);
    schedule_thumbnail_check();
    status_label->setText(QString("已发现 %1 个文件...").arg(current_files.size()));
    on_grid_selection_changed();
}

void MainWindow::on_scan_finished() {
    status_label->setText(QString("扫描完成，共 %1 个文件").arg(current_files.size()));
    update_grid_filter();
}

void MainWindow::schedule_thumbnail_check() {
    if (!thumbnails_enabled) return;
    thumbnail_check_timer->start(100);
}

void MainWindow::update_visible_thumbnails() {
    if (!thumbnails_enabled) return;
    
    QStringList batch;
    QRegion visible_region = scroll_area->viewport()->visibleRegion();
    QRect visible_rect = visible_region.boundingRect(); // Simplified
    
    // Logic to find visible items in sections
    // This requires mapping coordinates.
    // For simplicity, we can just check if section is visible, then check items.
    
    for (auto it = date_sections.begin(); it != date_sections.end(); ++it) {
        DateSection *section = it.value();
        AutoHeightListWidget *lw = section->getListWidget();
        
        if (lw->isHidden()) continue;
        
        // Check if lw is in visible rect of scroll area
        QPoint lw_pos = lw->mapTo(scroll_area->viewport(), QPoint(0,0));
        QRect lw_rect(lw_pos, lw->size());
        
        if (!visible_rect.intersects(lw_rect)) continue;
        
        for (int i = 0; i < lw->count(); ++i) {
            QListWidgetItem *item = lw->item(i);
            if (item->isHidden()) continue;
            
            // Check item visibility (approx)
            QRect item_rect = lw->visualItemRect(item);
            QRect item_global_rect(lw_pos + item_rect.topLeft(), item_rect.size());
            
            if (visible_rect.intersects(item_global_rect)) {
                QString path = item->data(Qt::UserRole).toString();
                if (!loaded_thumbnails.contains(path) && !loading_thumbnails.contains(path)) {
                    batch.append(path);
                    loading_thumbnails.insert(path);
                    if (batch.size() >= 32) break;
                }
            }
        }
        if (batch.size() >= 32) break;
    }
    
    if (!batch.isEmpty()) {
        QMetaObject::invokeMethod(thumb_worker, "process_files", Q_ARG(QStringList, batch));
    }
}

void MainWindow::on_thumbnail_ready(const QString &path, const QIcon &icon) {
    if (path_to_item.contains(path)) {
        path_to_item[path]->setIcon(icon);
        path_to_item[path]->setData(Qt::UserRole + 1, true);
    }
    loaded_thumbnails.insert(path);
    loading_thumbnails.remove(path);
    // Continue loading if needed
    schedule_thumbnail_check();
}

void MainWindow::update_preview(const QString &path) {
    if (path.isEmpty()) {
        preview_label->setText("无预览");
        file_info_label->setText("未选择文件");
        return;
    }
    
    QPixmap pix;
    if (pix.load(path)) {
        preview_label->setPixmap(pix.scaled(200, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        preview_label->setText("无法预览");
    }
    
    QFileInfo fi(path);
    file_info_label->setText(QString("文件: %1\n大小: %2 MB\n修改: %3")
                             .arg(fi.fileName())
                             .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 2)
                             .arg(fi.lastModified().toString("yyyy-MM-dd HH:mm:ss")));
}

void MainWindow::on_grid_selection_changed() {
    if (updating_tree) return;
    
    selected_items.clear();
    QString last_path;
    
    for (auto it = date_sections.begin(); it != date_sections.end(); ++it) {
        AutoHeightListWidget *lw = it.value()->getListWidget();
        for (int i = 0; i < lw->count(); ++i) {
            QListWidgetItem *item = lw->item(i);
            if (!item->isHidden() && item->checkState() == Qt::Checked) {
                QString path = item->data(Qt::UserRole).toString();
                selected_items.insert(path);
                last_path = path;
            }
        }
    }
    
    stats_label->setText(QString("已选择: %1 文件").arg(selected_items.size()));
    schedule_refresh_target_tree();
    update_preview(last_path);
}

void MainWindow::update_grid_filter() {
    if (target_directory.isEmpty()) return;
    
    updating_tree = true;
    duplicate_files.clear();
    
    for (auto it = date_sections.begin(); it != date_sections.end(); ++it) {
        AutoHeightListWidget *lw = it.value()->getListWidget();
        for (int i = 0; i < lw->count(); ++i) {
            QListWidgetItem *item = lw->item(i);
            QString file_path = item->data(Qt::UserRole).toString();
            
            bool is_duplicate = false;
            if (organize_checkbox->isChecked()) {
                QString path_str = Utils::get_path_from_file(file_path, time_source, date_format, custom_template);
                QString dest_dir = QDir(target_directory).filePath(path_str);
                QString dest_file = QDir(dest_dir).filePath(QFileInfo(file_path).fileName());
                
                if (QFile::exists(dest_file) && Utils::files_are_identical(file_path, dest_file)) {
                    is_duplicate = true;
                    duplicate_files.insert(file_path);
                }
            }
            
            item->setHidden(is_duplicate);
            if (is_duplicate) item->setCheckState(Qt::Unchecked);
        }
    }
    
    updating_tree = false;
    on_grid_selection_changed();
}

void MainWindow::schedule_refresh_target_tree() {
    if (updating_tree) return;
    preview_refresh_timer->start(200);
}

void MainWindow::refresh_target_tree() {
    if (updating_tree) return;
    updating_tree = true;
    
    target_tree->blockSignals(true);
    target_tree->clear();
    
    if (QDir(target_directory).exists()) {
        QTreeWidgetItem *root = new QTreeWidgetItem(target_tree);
        root->setText(0, QDir(target_directory).dirName());
        root->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        
        if (organize_checkbox->isChecked() && !selected_items.isEmpty()) {
            populate_tree(root, target_directory, 2, 0, true);
            QMap<QString, QStringList> structure = calculate_import_structure();
            add_import_preview(root, structure);
        } else {
            populate_tree(root, target_directory, 2, 0, false);
        }
        root->setExpanded(true);
    }
    
    target_tree->blockSignals(false);
    updating_tree = false;
}

void MainWindow::populate_tree(QTreeWidgetItem *parent, const QString &dir, int max_depth, int current_depth, bool existing_only) {
    if (current_depth >= max_depth) return;
    
    QDir d(dir);
    QFileInfoList list = d.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files, QDir::Name);
    
    for (const QFileInfo &fi : list) {
        if (fi.isDir()) {
            QTreeWidgetItem *item = new QTreeWidgetItem(parent);
            item->setText(0, fi.fileName());
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            item->setData(0, Qt::UserRole + 2, fi.fileName());
            populate_tree(item, fi.filePath(), max_depth, current_depth + 1, existing_only);
        } else if (!existing_only && current_depth > 0) {
            QTreeWidgetItem *item = new QTreeWidgetItem(parent);
            item->setText(0, fi.fileName());
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        }
    }
}

QMap<QString, QStringList> MainWindow::calculate_import_structure() {
    QMap<QString, QStringList> structure;
    duplicate_files.clear();
    
    for (const QString &file_path : selected_items) {
        QString path_str = Utils::get_path_from_file(file_path, time_source, date_format, custom_template);
        QString dest_dir = QDir(target_directory).filePath(path_str);
        QString dest_file = QDir(dest_dir).filePath(QFileInfo(file_path).fileName());
        
        if (QFile::exists(dest_file) && Utils::files_are_identical(file_path, dest_file)) {
            duplicate_files.insert(file_path);
            continue;
        }
        
        structure[path_str].append(QFileInfo(file_path).fileName());
    }
    return structure;
}

void MainWindow::add_import_preview(QTreeWidgetItem *root, const QMap<QString, QStringList> &structure) {
    // Similar to Python logic, merge structure into tree
    // Simplified for brevity
    for (auto it = structure.begin(); it != structure.end(); ++it) {
        QString path_str = it.key();
        QStringList files = it.value();
        
        QStringList parts = path_str.split('/');
        QTreeWidgetItem *parent = root;
        
        for (int i = 0; i < parts.size(); ++i) {
            QString part = parts[i];
            QTreeWidgetItem *found = nullptr;
            for (int j = 0; j < parent->childCount(); ++j) {
                if (parent->child(j)->text(0) == part || parent->child(j)->data(0, Qt::UserRole + 2).toString() == part) {
                    found = parent->child(j);
                    break;
                }
            }
            
            bool exists = QDir(target_directory).cd(path_str.section('/', 0, i)) && QDir(target_directory + "/" + path_str.section('/', 0, i)).exists(part);
            
            if (!found) {
                found = new QTreeWidgetItem(parent);
                found->setText(0, part + QString(" (%1)").arg(files.size()));
                found->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                found->setData(0, Qt::UserRole + 2, part);
                if (!exists) {
                    found->setForeground(0, QColor("#88CCFF"));
                    QFont f = found->font(0);
                    f.setItalic(true);
                    found->setFont(0, f);
                }
            }
            parent = found;
        }
        
        // Add files preview to the last node
        for (int k = 0; k < std::min<qsizetype>(3, files.size()); ++k) {
            QTreeWidgetItem *file_item = new QTreeWidgetItem(parent);
            file_item->setText(0, "📄 " + files[k]);
            file_item->setForeground(0, QColor("#88CCFF"));
        }
        if (files.size() > 3) {
            QTreeWidgetItem *more = new QTreeWidgetItem(parent);
            more->setText(0, QString("... 等 %1 个文件").arg(files.size() - 3));
        }
    }
}

void MainWindow::on_browse_target() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择目标", target_directory);
    if (!dir.isEmpty()) {
        target_directory = dir;
        target_label->setText(dir);
        update_grid_filter();
    }
}

void MainWindow::on_import_clicked() {
    if (selected_items.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择文件");
        return;
    }
    
    QString mode_text = mode_group->checkedButton()->text();
    QString mode = "copy";
    if (mode_text.contains("移动")) mode = "move";
    
    bool organize = organize_checkbox->isChecked();
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认导入", 
        QString("即将导入 %1 个文件到 %2").arg(selected_items.size()).arg(target_directory),
        QMessageBox::Yes|QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        progress_bar->setVisible(true);
        progress_bar->setValue(0);
        import_btn->setEnabled(false);
        status_label->setText("导入中...");
        
        QStringList files = selected_items.values();
        QMetaObject::invokeMethod(import_worker, "import_files",
            Q_ARG(QStringList, files),
            Q_ARG(QString, target_directory),
            Q_ARG(QString, mode),
            Q_ARG(bool, organize),
            Q_ARG(QString, date_format),
            Q_ARG(QString, time_source),
            Q_ARG(QString, custom_template)
        );
    }
}

void MainWindow::on_import_progress(int current, int total) {
    progress_bar->setMaximum(total);
    progress_bar->setValue(current);
    status_label->setText(QString("导入中: %1/%2").arg(current).arg(total));
}

void MainWindow::on_import_finished(bool success) {
    progress_bar->setVisible(false);
    import_btn->setEnabled(true);
    status_label->setText(success ? "导入完成" : "导入失败");
    if (success) {
        QMessageBox::information(this, "成功", "导入完成");
        refresh_file_tree(); // Refresh source tree
        refresh_target_tree();
    }
}

void MainWindow::refresh_file_tree() {
    // fs_model refresh is automatic usually, but we can force it
    // fs_model->rootPath() is watched.
}

void MainWindow::on_recursive_toggled(bool checked) {
    // Re-scan current path
    QModelIndex index = tree_view->currentIndex();
    if (index.isValid()) {
        on_folder_clicked(index);
    }
}

void MainWindow::on_date_format_changed(const QString &text) {
    date_format = text;
    path_cache.clear();
    update_grid_filter();
}

void MainWindow::on_time_source_changed(QAbstractButton *button) {
    if (mode_group->id(button) == 1) time_source = "exif";
    else time_source = "file_mtime";
    path_cache.clear();
    update_grid_filter();
}

void MainWindow::on_template_changed(const QString &text) {
    custom_template = text;
    path_cache.clear();
    update_grid_filter();
}

void MainWindow::on_thumbnail_toggle_changed(int state) {
    thumbnails_enabled = (state == Qt::Checked);
    if (thumbnails_enabled) schedule_thumbnail_check();
}

void MainWindow::on_tree_item_changed(QTreeWidgetItem *item, int column) {
    // Handle tree item checking to sync with grid
    // Simplified: Just refresh grid selection if needed
}
