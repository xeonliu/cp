#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include <QByteArray>
#include <QObject>

class Utils {
public:
    static QString format_date(const QDateTime& dt, const QString& format);
    static QString format_custom_template(const QDateTime& dt, const QString& tmpl);
    static QByteArray hash_file(const QString& file_path, qint64 block_size = 65536);
    static QDateTime get_file_date(const QString& file_path, const QString& time_source);
    static bool files_are_identical(const QString& file1, const QString& file2);
    static QString get_path_from_file(const QString& file_path, const QString& time_source, const QString& date_format, const QString& custom_template);
};

#endif // UTILS_H
