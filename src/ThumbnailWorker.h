#ifndef THUMBNAILWORKER_H
#define THUMBNAILWORKER_H

#include <QObject>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QMutex>
#include <atomic>
#include <QStringList>

class ThumbnailWorker : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailWorker(QObject *parent = nullptr);
    ~ThumbnailWorker();

public slots:
    void process_files(const QStringList& file_paths);
    void stop();

signals:
    void finished(QString path, QIcon icon);

private:
    std::atomic<bool> is_running;
    QImage process_raw_file(const QString& path);
    QImage process_video_file(const QString& path);
    QImage process_image_file(const QString& path);
};

#endif // THUMBNAILWORKER_H
