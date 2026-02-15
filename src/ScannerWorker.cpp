#include "ScannerWorker.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QThread>

ScannerWorker::ScannerWorker(QObject *parent) : QObject(parent), is_running(false) {
    valid_extensions << "*.jpg" << "*.jpeg" << "*.png" << "*.arw" << "*.cr2" << "*.nef" << "*.dng" << "*.rw2" << "*.mp4" << "*.mov";
}

ScannerWorker::~ScannerWorker() {
    stop();
}

void ScannerWorker::stop() {
    is_running = false;
}

void ScannerWorker::scan(const QString& root_path, bool recursive) {
    is_running = true;
    
    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    if (recursive) {
        flags = QDirIterator::Subdirectories;
    }
    
    QDirIterator it(root_path, valid_extensions, QDir::Files, flags);
    
    QStringList batch;
    const int batch_size = 50;
    
    while (it.hasNext()) {
        if (!is_running) break;
        
        QString file_path = it.next();
        // Skip hidden files/dirs if QDirIterator doesn't (it shouldn't if configured right, but let's be safe)
        if (QFileInfo(file_path).fileName().startsWith(".")) continue;
        
        batch.append(file_path);
        
        if (batch.size() >= batch_size) {
            emit files_found(batch);
            batch.clear();
            // Process events to keep UI responsive if running in main thread (but we are in worker thread)
            // In worker thread, we don't need processEvents usually, but we should yield if needed.
            // But since we emit signal, the slot in main thread will handle it.
        }
    }
    
    if (!batch.isEmpty() && is_running) {
        emit files_found(batch);
    }
    
    emit finished();
    is_running = false;
}
