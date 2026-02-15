#include "ImportWorker.h"
#include "Utils.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>

ImportWorker::ImportWorker(QObject *parent) : QObject(parent), is_running(false) {}

ImportWorker::~ImportWorker() {
    stop();
}

void ImportWorker::stop() {
    is_running = false;
}

void ImportWorker::import_files(const QStringList& file_list, const QString& target_path, const QString& mode, bool organize_by_date, const QString& date_format, const QString& time_source, const QString& custom_template) {
    is_running = true;
    int total = file_list.size();
    
    for (int i = 0; i < total; ++i) {
        if (!is_running) break;
        
        QString file_path = file_list[i];
        
        QString dest_dir = target_path;
        if (organize_by_date) {
            QString sub_dir = Utils::get_path_from_file(file_path, time_source, date_format, custom_template);
            dest_dir = QDir(target_path).filePath(sub_dir);
        }
        
        QDir().mkpath(dest_dir);
        
        QFileInfo fi(file_path);
        QString filename = fi.fileName();
        QString dest_file = QDir(dest_dir).filePath(filename);
        
        if (QFile::exists(dest_file)) {
            // Check for duplicate content
            if (Utils::files_are_identical(file_path, dest_file)) {
                emit progress(i + 1, total);
                continue; // Skip identical
            } else {
                // Rename to avoid overwrite
                QString base = fi.baseName();
                QString ext = fi.completeSuffix();
                dest_file = QDir(dest_dir).filePath(base + "_copy." + ext);
                // Simple collision resolution (could loop for _copy1, _copy2...)
            }
        }
        
        bool success = false;
        if (mode == "move") {
            success = QFile::rename(file_path, dest_file);
        } else {
            success = QFile::copy(file_path, dest_file);
        }
        
        if (!success) {
            qDebug() << "Failed to import:" << file_path;
        }
        
        emit progress(i + 1, total);
    }
    
    emit finished(true);
    is_running = false;
}
