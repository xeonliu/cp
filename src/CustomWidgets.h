#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QToolButton>
#include <QFrame>

class AutoHeightListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit AutoHeightListWidget(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

public:
    void adjust_height();
};

class DateSection : public QWidget {
    Q_OBJECT
public:
    explicit DateSection(const QString& date_str, QWidget *parent = nullptr);
    
    void add_item(QListWidgetItem *item);
    AutoHeightListWidget* getListWidget() const { return list_widget; }
    QString getDateStr() const { return date_str; }

signals:
    void selection_changed();
    void thumbnail_clicked(QString path);

private slots:
    void toggle_content();
    void on_header_checkbox_changed(int state);
    void on_item_check_changed(QListWidgetItem *item);
    void on_item_clicked(QListWidgetItem *item);

private:
    void update_count();

    QString date_str;
    bool is_expanded;
    
    QVBoxLayout *layout;
    QFrame *header;
    QToolButton *toggle_btn;
    QCheckBox *checkbox;
    QLabel *count_label;
    AutoHeightListWidget *list_widget;
};

#endif // CUSTOMWIDGETS_H
