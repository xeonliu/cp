#ifndef IMPORTWORKER_H
#define IMPORTWORKER_H

#include <QObject>
#include <QStringList>
#include <atomic>

class ImportWorker : public QObject {
    Q_OBJECT
public:
    explicit ImportWorker(QObject *parent = nullptr);
    ~ImportWorker();

public slots:
    void import_files(const QStringList& file_list, const QString& target_path, const QString& mode, bool organize_by_date, const QString& date_format, const QString& time_source, const QString& custom_template);
    void stop();

signals:
    void progress(int current, int total);
    void finished(bool success);

private:
    std::atomic<bool> is_running;
};

#endif // IMPORTWORKER_H
