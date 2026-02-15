#include "ThumbnailWorker.h"
#include <QFileInfo>
#include <QPainter>
#include <libraw/libraw.h>
#include <QDebug>

ThumbnailWorker::ThumbnailWorker(QObject *parent) : QObject(parent), is_running(false) {}

ThumbnailWorker::~ThumbnailWorker() {
    stop();
}

void ThumbnailWorker::stop() {
    is_running = false;
}

void ThumbnailWorker::process_files(const QStringList& file_paths) {
    is_running = true;
    for (const QString& path : file_paths) {
        if (!is_running) break;
        
        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();
        QImage img;
        
        if (QStringList{"arw", "cr2", "nef", "dng", "orf", "rw2"}.contains(ext)) {
            img = process_raw_file(path);
        } else if (QStringList{"mp4", "mov", "avi"}.contains(ext)) {
            img = process_video_file(path);
        } else {
            img = process_image_file(path);
        }
        
        if (!img.isNull()) {
            QPixmap pixmap = QPixmap::fromImage(img);
            // Scale to save memory, similar to Python code (200x200)
            pixmap = pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            emit finished(path, QIcon(pixmap));
        }
    }
    is_running = false;
}

QImage ThumbnailWorker::process_raw_file(const QString& path) {
    LibRaw RawProcessor;
    QImage img;
    
    // Open the file
    if (RawProcessor.open_file(path.toLocal8Bit().constData()) != LIBRAW_SUCCESS) {
        return img; // Empty image
    }
    
    // Try to unpack thumbnail
    if (RawProcessor.unpack_thumb() == LIBRAW_SUCCESS) {
        libraw_processed_image_t *thumb = RawProcessor.dcraw_make_mem_thumb();
        if (thumb) {
            if (thumb->type == LibRaw_image_formats::LIBRAW_IMAGE_JPEG) {
                img.loadFromData(thumb->data, thumb->data_size, "JPEG");
            } else if (thumb->type == LibRaw_image_formats::LIBRAW_IMAGE_BITMAP) {
                // Bitmap handling is complex, skip for now or implement if needed
                // Usually embedded thumbs are JPEG
                // Fallback to processing raw data if needed
            }
            LibRaw::dcraw_clear_mem(thumb);
        }
    }
    
    // If no thumbnail, process raw data (slow)
    if (img.isNull()) {
        // Only if we really need to. The Python code did this fallback.
        // But for C++, let's try to be fast. If thumb fails, maybe skip or show error?
        // Let's try simple processing
        RawProcessor.imgdata.params.use_camera_wb = 1;
        RawProcessor.imgdata.params.half_size = 1; // Fast
        if (RawProcessor.unpack() == LIBRAW_SUCCESS) {
             RawProcessor.dcraw_process();
             libraw_processed_image_t *image = RawProcessor.dcraw_make_mem_image();
             if (image) {
                 // Convert RGB data to QImage
                 // image->data is RGBRGB...
                 // QImage Format_RGB888
                 if (image->colors == 3) {
                     img = QImage(image->data, image->width, image->height, image->width * 3, QImage::Format_RGB888).copy();
                 }
                 LibRaw::dcraw_clear_mem(image);
             }
        }
    }
    
    RawProcessor.recycle();
    return img;
}

QImage ThumbnailWorker::process_video_file(const QString& path) {
    QImage img(150, 100, QImage::Format_RGB32);
    img.fill(QColor("#2a4a60"));
    QPainter painter(&img);
    painter.setPen(Qt::white);
    painter.drawText(img.rect(), Qt::AlignCenter, "VIDEO\n" + QFileInfo(path).suffix());
    return img;
}

QImage ThumbnailWorker::process_image_file(const QString& path) {
    QImage img;
    img.load(path);
    return img;
}
