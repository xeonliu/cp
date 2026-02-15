#ifndef SCANNERWORKER_H
#define SCANNERWORKER_H

#include <QObject>
#include <QStringList>
#include <atomic>

class ScannerWorker : public QObject {
    Q_OBJECT
public:
    explicit ScannerWorker(QObject *parent = nullptr);
    ~ScannerWorker();

public slots:
    void scan(const QString& root_path, bool recursive);
    void stop();

signals:
    void files_found(QStringList files);
    void finished();

private:
    std::atomic<bool> is_running;
    QStringList valid_extensions;
};

#endif // SCANNERWORKER_H
