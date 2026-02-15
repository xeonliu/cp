#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QListWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTreeWidget>
#include <QScrollArea>
#include <QTimer>
#include <QThread>
#include <QMap>
#include <QSet>

#include "ScannerWorker.h"
#include "ThumbnailWorker.h"
#include "ImportWorker.h"
#include "CustomWidgets.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // UI Events
    void on_device_clicked(QListWidgetItem *item);
    void on_folder_clicked(const QModelIndex &index);
    void on_recursive_toggled(bool checked);
    void on_browse_target();
    void on_import_clicked();
    void on_date_format_changed(const QString &text);
    void on_time_source_changed(QAbstractButton *button);
    void on_template_changed(const QString &text);
    void on_thumbnail_toggle_changed(int state);
    void on_tree_item_changed(QTreeWidgetItem *item, int column);
    
    // Worker Signals
    void on_files_found(const QStringList &files);
    void on_scan_finished();
    void on_thumbnail_ready(const QString &path, const QIcon &icon);
    void on_import_progress(int current, int total);
    void on_import_finished(bool success);
    
    // Internal Logic
    void refresh_devices();
    void update_grid_filter();
    void on_grid_selection_changed();
    void update_preview(const QString &path);
    void schedule_thumbnail_check();
    void update_visible_thumbnails();
    void schedule_refresh_target_tree();
    void refresh_target_tree();
    void refresh_file_tree();

private:
    void setup_ui();
    void init_threads();
    
    QWidget* create_top_bar();
    QWidget* create_left_panel();
    QWidget* create_center_panel();
    QWidget* create_right_panel();
    QWidget* create_bottom_bar();
    
    void start_scan(const QString &path);
    void populate_tree(QTreeWidgetItem *parent, const QString &dir, int max_depth, int current_depth, bool existing_only);
    void add_import_preview(QTreeWidgetItem *root, const QMap<QString, QStringList> &structure);
    QMap<QString, QStringList> calculate_import_structure();
    
    // UI Elements
    QButtonGroup *mode_group;
    QListWidget *device_list;
    QCheckBox *recursive_check;
    QTreeView *tree_view;
    QFileSystemModel *fs_model;
    
    QScrollArea *scroll_area;
    QWidget *scroll_content;
    QVBoxLayout *scroll_layout;
    
    QLabel *preview_label;
    QLabel *file_info_label;
    QLabel *mode_label;
    QCheckBox *organize_checkbox;
    QCheckBox *thumbnail_checkbox;
    QComboBox *date_format_combo;
    QButtonGroup *time_source_group;
    QLineEdit *template_input;
    QLabel *target_label;
    QTreeWidget *target_tree;
    QLabel *stats_label;
    
    QProgressBar *progress_bar;
    QLabel *status_label;
    QPushButton *import_btn;
    
    // Data
    QString target_directory;
    QStringList current_files;
    QSet<QString> selected_items;
    QSet<QString> duplicate_files;
    QMap<QString, QListWidgetItem*> path_to_item; // Map file path to list item
    QMap<QString, DateSection*> date_sections;
    
    // Settings
    QString date_format;
    QString time_source;
    QString custom_template;
    bool thumbnails_enabled;
    
    // State
    bool updating_tree;
    int last_import_count;
    int last_skipped_duplicates;
    
    QMap<QPair<QString, bool>, QString> path_cache;
    QSet<QString> loaded_thumbnails;
    QSet<QString> loading_thumbnails;
    
    // Threads
    QThread scan_thread;
    ScannerWorker *scanner;
    
    QThread thumb_thread;
    ThumbnailWorker *thumb_worker;
    
    QThread import_thread;
    ImportWorker *import_worker;
    
    // Timers
    QTimer *device_timer;
    QTimer *thumbnail_check_timer;
    QTimer *preview_refresh_timer;
};

#endif // MAINWINDOW_H
