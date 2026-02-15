#include "CustomWidgets.h"
#include <QScrollBar>
#include <QApplication>

// --- AutoHeightListWidget ---

AutoHeightListWidget::AutoHeightListWidget(QWidget *parent) : QListWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionMode(QAbstractItemView::NoSelection);
    setIconSize(QSize(160, 120));
    setGridSize(QSize(180, 160));
    setViewMode(QListWidget::IconMode);
    setResizeMode(QListWidget::Adjust);
    setWordWrap(true);
    setSpacing(5);
}

void AutoHeightListWidget::resizeEvent(QResizeEvent *event) {
    QListWidget::resizeEvent(event);
    adjust_height();
}

void AutoHeightListWidget::adjust_height() {
    int count = this->count();
    if (count == 0) {
        setFixedHeight(0);
        return;
    }
    
    int w = viewport()->width();
    int grid_w = gridSize().width();
    if (w <= 0 || grid_w <= 0) return;
    
    int cols = std::max(1, w / grid_w);
    int rows = (count + cols - 1) / cols;
    
    int h = rows * gridSize().height() + 10;
    setFixedHeight(h);
}

// --- DateSection ---

DateSection::DateSection(const QString& date_str, QWidget *parent) 
    : QWidget(parent), date_str(date_str), is_expanded(true) 
{
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Header
    header = new QFrame(this);
    header->setStyleSheet("background-color: #2a2a2a; border-radius: 4px;");
    header->setFixedHeight(30);
    
    QHBoxLayout *header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(5, 0, 5, 0);
    
    toggle_btn = new QToolButton(header);
    toggle_btn->setText("▼");
    toggle_btn->setStyleSheet("border: none; color: #aaa; font-weight: bold;");
    connect(toggle_btn, &QToolButton::clicked, this, &DateSection::toggle_content);
    header_layout->addWidget(toggle_btn);
    
    checkbox = new QCheckBox(date_str, header);
    checkbox->setStyleSheet("font-weight: bold; color: #ddd;");
    checkbox->setChecked(true);
    connect(checkbox, &QCheckBox::checkStateChanged, this, &DateSection::on_header_checkbox_changed);
    header_layout->addWidget(checkbox);
    
    count_label = new QLabel("(0)", header);
    count_label->setStyleSheet("color: #888;");
    header_layout->addWidget(count_label);
    
    header_layout->addStretch();
    layout->addWidget(header);
    
    // ListWidget
    list_widget = new AutoHeightListWidget(this);
    connect(list_widget, &QListWidget::itemChanged, this, &DateSection::on_item_check_changed);
    connect(list_widget, &QListWidget::itemClicked, this, &DateSection::on_item_clicked);
    layout->addWidget(list_widget);
}

void DateSection::toggle_content() {
    is_expanded = !is_expanded;
    list_widget->setVisible(is_expanded);
    toggle_btn->setText(is_expanded ? "▼" : "▶");
}

void DateSection::add_item(QListWidgetItem *item) {
    list_widget->addItem(item);
    update_count();
    list_widget->adjust_height();
}

void DateSection::update_count() {
    count_label->setText(QString("(%1)").arg(list_widget->count()));
}

void DateSection::on_header_checkbox_changed(Qt::CheckState state) {
    bool is_checked = (state == Qt::Checked);
    list_widget->blockSignals(true);
    for (int i = 0; i < list_widget->count(); ++i) {
        QListWidgetItem *item = list_widget->item(i);
        item->setCheckState(is_checked ? Qt::Checked : Qt::Unchecked);
    }
    list_widget->blockSignals(false);
    emit selection_changed();
}

void DateSection::on_item_check_changed(QListWidgetItem *item) {
    Q_UNUSED(item);
    int total = list_widget->count();
    int selected = 0;
    for (int i = 0; i < total; ++i) {
        if (list_widget->item(i)->checkState() == Qt::Checked) {
            selected++;
        }
    }
    
    checkbox->blockSignals(true);
    if (selected == 0) checkbox->setCheckState(Qt::Unchecked);
    else if (selected == total) checkbox->setCheckState(Qt::Checked);
    else checkbox->setCheckState(Qt::PartiallyChecked);
    checkbox->blockSignals(false);
    
    emit selection_changed();
}

void DateSection::on_item_clicked(QListWidgetItem *item) {
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit thumbnail_clicked(path);
    }
}
