#include "Utils.h"
#include <QFile>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDateTime>
#include <QFileInfo>

QString Utils::format_date(const QDateTime& dt, const QString& format) {
    if (format == "YYYY-MM-DD") return dt.toString("yyyy-MM-dd");
    if (format == "YYYY/MM/DD") return dt.toString("yyyy/MM/dd");
    if (format == "YYYY-MM") return dt.toString("yyyy-MM");
    if (format == "YYYY/MM") return dt.toString("yyyy/MM");
    if (format == "YYYYMMDD") return dt.toString("yyyyMMdd");
    if (format == "YYYY") return dt.toString("yyyy");
    return dt.toString(format);
}

QString Utils::format_custom_template(const QDateTime& dt, const QString& tmpl) {
    QString result = tmpl;
    QRegularExpression re(R"(\{(\w+)(?::([^}]+))?\})");
    QRegularExpressionMatchIterator i = re.globalMatch(tmpl);
    
    // We need to replace matches. Since replacing changes indices, we iterate and replace.
    // Simpler: iterate matches, build list of replacements, then apply.
    // Or just use QString::replace for known keys if the template is simple.
    // But regex is better for custom formats.
    
    // For simplicity, let's implement basic replacement like Python's logic
    // We can't easily use regex replace with callback in Qt5/6 without a loop.
    
    // A simple approach:
    QStringList parts;
    int lastPos = 0;
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        parts << result.mid(lastPos, match.capturedStart() - lastPos);
        
        QString key = match.captured(1);
        QString fmt = match.captured(2);
        
        int value = 0;
        if (key == "year") value = dt.date().year();
        else if (key == "month") value = dt.date().month();
        else if (key == "day") value = dt.date().day();
        else if (key == "hour") value = dt.time().hour();
        else if (key == "minute") value = dt.time().minute();
        else if (key == "second") value = dt.time().second();
        else {
            parts << match.captured(0); // unknown key
            lastPos = match.capturedEnd();
            continue;
        }
        
        QString valStr;
        if (!fmt.isEmpty()) {
            // Basic support for '02d' style via QString::asprintf or arg
            // Qt doesn't support python format specifiers fully.
            // We'll support '02d' by checking if it starts with '0' and has 'd'
            if (fmt == "02d") valStr = QString("%1").arg(value, 2, 10, QChar('0'));
            else valStr = QString::number(value); // Fallback
        } else {
            // Default padding for some fields
            if (key == "month" || key == "day" || key == "hour" || key == "minute" || key == "second")
                valStr = QString("%1").arg(value, 2, 10, QChar('0'));
            else
                valStr = QString::number(value);
        }
        parts << valStr;
        lastPos = match.capturedEnd();
    }
    parts << result.mid(lastPos);
    return parts.join("");
}

QByteArray Utils::hash_file(const QString& file_path, qint64 block_size) {
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!file.atEnd()) {
        hash.addData(file.read(block_size));
    }
    return hash.result().toHex();
}

QDateTime Utils::get_file_date(const QString& file_path, const QString& time_source) {
    // For EXIF, we'd need Exiv2 or LibRaw or just rely on file time for now if LibRaw is complex for metadata only.
    // The Python code used PIL for EXIF.
    // LibRaw can read metadata.
    // For simplicity in this migration, let's use file modification time first.
    // TODO: Implement EXIF reading if needed using LibRaw.
    
    QFileInfo info(file_path);
    return info.lastModified();
}

bool Utils::files_are_identical(const QString& file1, const QString& file2) {
    QByteArray hash1 = hash_file(file1);
    QByteArray hash2 = hash_file(file2);
    return !hash1.isEmpty() && !hash2.isEmpty() && hash1 == hash2;
}

QString Utils::get_path_from_file(const QString& file_path, const QString& time_source, const QString& date_format, const QString& custom_template) {
    QDateTime dt = get_file_date(file_path, time_source);
    if (!custom_template.isEmpty()) {
        return format_custom_template(dt, custom_template);
    } else {
        return format_date(dt, date_format);
    }
}
