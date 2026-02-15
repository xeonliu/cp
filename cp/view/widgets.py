from PyQt6.QtCore import Qt, QSize, pyqtSignal
from PyQt6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QSizePolicy,
    QToolButton,
    QVBoxLayout,
)


class AutoHeightListWidget(QListWidget):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.setIconSize(QSize(160, 120))
        self.setGridSize(QSize(180, 160))
        self.setViewMode(QListWidget.ViewMode.IconMode)
        self.setResizeMode(QListWidget.ResizeMode.Adjust)
        self.setWordWrap(True)
        self.setSpacing(5)

    def resizeEvent(self, event) -> None:  # type: ignore[override]
        super().resizeEvent(event)
        self.adjust_height()

    def adjust_height(self) -> None:
        count = self.count()
        if count == 0:
            self.setFixedHeight(0)
            return

        width = self.viewport().width()
        grid_width = self.gridSize().width()
        if width <= 0 or grid_width <= 0:
            return

        cols = max(1, width // grid_width)
        rows = (count + cols - 1) // cols

        height = rows * self.gridSize().height() + 10
        self.setFixedHeight(height)


class DateSection(QFrame):
    selection_changed = pyqtSignal()
    thumbnail_clicked = pyqtSignal(str)

    def __init__(self, date_str: str, parent=None) -> None:
        super().__init__(parent)
        self.date_str = date_str
        self.is_expanded = True

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        header = QFrame()
        header.setFixedHeight(30)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(5, 0, 5, 0)

        self.toggle_btn = QToolButton()
        self.toggle_btn.setText("▼")
        self.toggle_btn.setStyleSheet("border: none;")
        self.toggle_btn.clicked.connect(self.toggle_content)
        header_layout.addWidget(self.toggle_btn)

        self.checkbox = QCheckBox(date_str)
        self.checkbox.setChecked(True)
        self.checkbox.stateChanged.connect(self.on_header_checkbox_changed)
        header_layout.addWidget(self.checkbox)

        self.count_label = QLabel("(0)")
        header_layout.addWidget(self.count_label)

        header_layout.addStretch()
        layout.addWidget(header)

        self.list_widget = AutoHeightListWidget()
        self.list_widget.itemChanged.connect(self.on_item_check_changed)
        self.list_widget.itemClicked.connect(self.on_item_clicked)
        layout.addWidget(self.list_widget)

    def toggle_content(self) -> None:
        self.is_expanded = not self.is_expanded
        self.list_widget.setVisible(self.is_expanded)
        self.toggle_btn.setText("▼" if self.is_expanded else "▶")

    def add_item(self, item) -> None:
        self.list_widget.addItem(item)
        self.update_count()
        self.list_widget.adjust_height()

    def update_count(self) -> None:
        self.count_label.setText(f"({self.list_widget.count()})")

    def on_header_checkbox_changed(self, state: int) -> None:
        is_checked = state == Qt.CheckState.Checked.value
        self.list_widget.blockSignals(True)
        for i in range(self.list_widget.count()):
            item = self.list_widget.item(i)
            if is_checked:
                item.setCheckState(Qt.CheckState.Checked)
            else:
                item.setCheckState(Qt.CheckState.Unchecked)
        self.list_widget.blockSignals(False)
        self.selection_changed.emit()

    def on_item_check_changed(self, item) -> None:
        total = self.list_widget.count()
        selected = 0
        for i in range(total):
            it = self.list_widget.item(i)
            if it.checkState() == Qt.CheckState.Checked:
                selected += 1

        self.checkbox.blockSignals(True)
        if selected == 0:
            self.checkbox.setCheckState(Qt.CheckState.Unchecked)
        elif selected == total:
            self.checkbox.setCheckState(Qt.CheckState.Checked)
        else:
            self.checkbox.setCheckState(Qt.CheckState.PartiallyChecked)
        self.checkbox.blockSignals(False)

        self.selection_changed.emit()

    def on_item_clicked(self, item) -> None:
        path = item.data(Qt.ItemDataRole.UserRole)
        if path:
            self.thumbnail_clicked.emit(path)
