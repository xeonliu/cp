import sys
import os
import shutil
import psutil
import rawpy
import imageio
import numpy as np
from datetime import datetime
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QTreeView, QListWidget, 
                             QListWidgetItem, QSplitter, QLabel, QPushButton, 
                             QGroupBox, QScrollArea, QCheckBox, QProgressBar, 
                             QFrame, QButtonGroup, QAbstractItemView, QFileDialog,
                             QMessageBox, QScrollBar, QSpinBox, QTreeWidget, QTreeWidgetItem,
                             QComboBox, QLineEdit, QRadioButton, QToolButton, QSizePolicy)
from PyQt6.QtCore import Qt, QSize, QDir, QThread, pyqtSignal, QObject, QMutex, QWaitCondition, QTimer
from PyQt6.QtGui import QIcon, QPixmap, QColor, QAction, QFileSystemModel, QPalette, QImage

# --- 样式表 (Dark Mode) ---
STYLESHEET = """
QMainWindow { background-color: #1e1e1e; color: #d3d3d3; font-family: "Segoe UI", Arial; }
QWidget { background-color: #1e1e1e; color: #d3d3d3; font-size: 13px; }

/* 左侧文件树 */
QTreeView { background-color: #252525; border: none; outline: none; }
QTreeView::item { padding: 4px; }
QTreeView::item:selected { background-color: #4f4f4f; color: white; }
QTreeView::item:hover { background-color: #333; }

/* 中间网格 */
QListWidget { background-color: #1b1b1b; border: none; outline: none; }
QListWidget::item { background-color: #222; border-radius: 4px; margin: 5px; }
QListWidget::item:selected { background-color: #333; border: 1px solid #888; }
QListWidget::item:hover { background-color: #2a2a2a; }

/* 按钮 */
QPushButton { background-color: #333; border: 1px solid #555; border-radius: 3px; padding: 5px 15px; color: #ccc; }
QPushButton:hover { background-color: #444; }
QPushButton:checked { background-color: #555; color: white; border-color: #888; }
QPushButton#ImportBtn { background-color: #4CAF50; color: white; font-weight: bold; border: none; }
QPushButton#ImportBtn:hover { background-color: #45a049; }

/* 分割线与面板 */
QSplitter::handle { background-color: #111; }
QGroupBox { border: 1px solid #444; margin-top: 20px; font-weight: bold; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #aaa; }
QScrollArea { border: none; }
"""

# --- 缩略图加载工作线程 ---
class ThumbnailWorker(QObject):
    """
    在后台线程加载图片，避免阻塞 UI
    """
    finished = pyqtSignal(str, QIcon) # 信号：路径, 图标
    
    def __init__(self):
        super().__init__()
        self.is_running = False

    def process_files(self, file_paths):
        self.is_running = True
        for path in file_paths:
            if not self.is_running: break
            
            ext = os.path.splitext(path)[1].lower()
            pixmap = QPixmap()
            
            # 1. RAW 文件处理 (使用 rawpy)
            if ext in ['.arw', '.cr2', '.nef', '.dng', '.orf', '.rw2']:
                try:
                    # 尝试读取内嵌缩略图以加快速度
                    with rawpy.imread(path) as raw:
                        try:
                            thumb = raw.extract_thumb()
                        except rawpy.LibRawNoThumbnailError:
                            thumb = None
                        
                        if thumb:
                            if thumb.format == rawpy.ThumbFormat.JPEG:
                                # JPEG 格式的缩略图直接加载
                                qimg = QImage.fromData(thumb.data)
                            elif thumb.format == rawpy.ThumbFormat.BITMAP:
                                # RGB 格式的缩略图需要转换
                                # 注意：这里可能需要根据实际情况调整 stride 和 format
                                # 为了简单起见，如果无法直接加载 JPEG，我们回退到 postprocess
                                # 但 postprocess 比较慢，所以先尝试简单的
                                h, w = thumb.data.shape
                                # 通常 thumb.data 是一个 numpy array，这里可能需要更多处理
                                # 暂时跳过复杂的 bitmap 处理，直接用 postprocess 生成缩略图
                                rgb = raw.postprocess(use_camera_wb=True, bright=1.0, user_sat=None, no_auto_bright=True, half_size=True)
                                h, w, ch = rgb.shape
                                qimg = QImage(rgb.data, w, h, ch * w, QImage.Format.Format_RGB888)
                            else:
                                qimg = QImage()
                        else:
                            # 如果没有缩略图，使用 postprocess (较慢但质量好)
                            # half_size=True 可以显著加快速度
                            rgb = raw.postprocess(use_camera_wb=True, bright=1.0, user_sat=None, no_auto_bright=True, half_size=True)
                            h, w, ch = rgb.shape
                            qimg = QImage(rgb.data, w, h, ch * w, QImage.Format.Format_RGB888)

                        if not qimg.isNull():
                            pixmap = QPixmap.fromImage(qimg)
                            # 缩放以适应网格大小，减少内存占用
                            pixmap = pixmap.scaled(200, 200, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
                except Exception as e:
                    print(f"Error reading RAW {path}: {e}")
                    # 出错时显示特定图标
                    pixmap = QPixmap(150, 100)
                    pixmap.fill(QColor("#502020")) # 深红色代表错误
                    from PyQt6.QtGui import QPainter
                    painter = QPainter(pixmap)
                    painter.setPen(Qt.GlobalColor.white)
                    painter.drawText(pixmap.rect(), Qt.AlignmentFlag.AlignCenter, f"RAW Error\n{ext}")
                    painter.end()

            # 2. 模拟 视频文件
            elif ext in ['.mp4', '.mov', '.avi']:
                pixmap = QPixmap(150, 100)
                pixmap.fill(QColor("#2a4a60")) # 蓝色代表视频
                from PyQt6.QtGui import QPainter
                painter = QPainter(pixmap)
                painter.setPen(Qt.GlobalColor.white)
                painter.drawText(pixmap.rect(), Qt.AlignmentFlag.AlignCenter, f"VIDEO\n{ext}")
                painter.end()
            
            # 3. 普通图片
            else:
                # 缩放加载以节省内存
                pixmap.load(path)
                if not pixmap.isNull():
                    pixmap = pixmap.scaled(200, 200, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
            
            if not pixmap.isNull():
                icon = QIcon(pixmap)
                self.finished.emit(path, icon)
        
        self.is_running = False

    def stop(self):
        self.is_running = False

# --- 文件扫描工作线程 ---
class ScannerWorker(QObject):
    """
    后台扫描文件，支持递归
    """
    files_found = pyqtSignal(list) # 发现一批文件
    finished = pyqtSignal()        # 扫描完成
    
    def __init__(self):
        super().__init__()
        self.is_running = False
        self.valid_extensions = {'.jpg', '.jpeg', '.png', '.arw', '.cr2', '.nef', '.dng', '.rw2', '.mp4', '.mov'}

    def scan(self, root_path, recursive):
        self.is_running = True
        batch = []
        batch_size = 50
        
        if recursive:
            for root, dirs, files in os.walk(root_path):
                if not self.is_running: break
                
                # 忽略隐藏目录
                dirs[:] = [d for d in dirs if not d.startswith('.')]
                
                for file in files:
                    if not self.is_running: break
                    if file.startswith('.'): continue
                    
                    ext = os.path.splitext(file)[1].lower()
                    if ext in self.valid_extensions:
                        full_path = os.path.join(root, file)
                        batch.append(full_path)
                        
                        if len(batch) >= batch_size:
                            self.files_found.emit(batch)
                            batch = []
        else:
            try:
                for entry in os.listdir(root_path):
                    if not self.is_running: break
                    full_path = os.path.join(root_path, entry)
                    if os.path.isfile(full_path) and not entry.startswith('.'):
                        ext = os.path.splitext(entry)[1].lower()
                        if ext in self.valid_extensions:
                            batch.append(full_path)
            except OSError:
                pass

        if batch and self.is_running:
            self.files_found.emit(batch)
            
        self.finished.emit()
        self.is_running = False

    def stop(self):
        self.is_running = False

# --- 文件导入工作线程 ---
class ImportWorker(QObject):
    """
    在后台线程执行文件复制/移动，避免阻塞 UI
    """
    progress = pyqtSignal(int, int)  # 当前进度, 总数
    finished = pyqtSignal(bool)      # 是否成功
    
    def __init__(self):
        super().__init__()
        self.is_running = True
    
    def stop(self):
        self.is_running = False

    def hash_file(self, file_path, block_size=65536):
        """计算文件的MD5哈希值"""
        import hashlib
        hasher = hashlib.md5()
        try:
            with open(file_path, 'rb') as f:
                buf = f.read(block_size)
                while len(buf) > 0:
                    hasher.update(buf)
                    buf = f.read(block_size)
            return hasher.hexdigest()
        except:
            return None
    
    def import_files(self, file_list, target_path, mode, organize_by_date, date_format=None, time_source=None, custom_template=None):
        """
        mode: 'copy', 'move', 'add', 'dng'
        organize_by_date: 是否按日期组织
        date_format: 日期格式字符串，如 'YYYY-MM-DD', 'YYYY/MM/DD' 等
        time_source: 'file_mtime' (文件修改时间) 或 'exif' (拍摄时间)
        custom_template: 自定义模板，如 '{year}/{month}' 或 '{year}-{month:02d}'
        """
        self.is_running = True
        total = len(file_list)
        
        try:
            for idx, file_path in enumerate(file_list):
                if not self.is_running:
                    break
                
                # 确定目标目录
                dest_dir = target_path
                if organize_by_date:
                    # 获取时间信息
                    if time_source == 'exif':
                        date_str = self.get_exif_date(file_path, date_format, custom_template)
                    else:  # 默认使用文件修改时间
                        date_str = self.get_file_date(file_path, date_format, custom_template)
                    
                    if date_str:
                        dest_dir = os.path.join(target_path, date_str)
                        os.makedirs(dest_dir, exist_ok=True)
                
                filename = os.path.basename(file_path)
                dest_file = os.path.join(dest_dir, filename)
                
                # 检测文件重复（仅当目标文件已存在时）
                if os.path.exists(dest_file):
                    # 比对哈希值
                    src_hash = self.hash_file(file_path)
                    dst_hash = self.hash_file(dest_file)
                    
                    if src_hash and dst_hash and src_hash == dst_hash:
                        # 文件完全相同，跳过导入
                        print(f"Skip duplicate file: {file_path}")
                        self.progress.emit(idx + 1, total)
                        continue
                    else:
                        # 文件不同，添加后缀避免覆盖
                        base, ext = os.path.splitext(filename)
                        dest_file = os.path.join(dest_dir, f"{base}_copy{ext}")
                
                if mode == 'move':
                    shutil.move(file_path, dest_file)
                else:  # copy, add, dng 统一处理为复制
                    shutil.copy2(file_path, dest_file)
                
                self.progress.emit(idx + 1, total)
            
            self.finished.emit(True)
        except Exception as e:
            print(f"Import error: {e}")
            self.finished.emit(False)
        finally:
            self.is_running = False
    
    def get_file_date(self, file_path, date_format, custom_template):
        """从文件修改时间获取日期字符串"""
        try:
            file_mtime = os.path.getmtime(file_path)
            dt = datetime.fromtimestamp(file_mtime)
            
            if custom_template:
                return self.format_custom_template(dt, custom_template)
            elif date_format:
                return self.format_date(dt, date_format)
            else:
                return dt.strftime('%Y-%m-%d')
        except:
            return None
    
    def get_exif_date(self, file_path, date_format, custom_template):
        """从 EXIF 数据获取拍摄时间（如果存在）"""
        try:
            from PIL import Image
            from PIL.ExifTags import TAGS
            
            image = Image.open(file_path)
            exif_data = image._getexif()
            
            if exif_data:
                for tag_id, value in exif_data.items():
                    tag_name = TAGS.get(tag_id, tag_id)
                    # 36867 是 DateTimeOriginal, 306 是 DateTime
                    if tag_name in ['DateTimeOriginal', 'DateTime']:
                        dt = datetime.strptime(value, '%Y:%m:%d %H:%M:%S')
                        if custom_template:
                            return self.format_custom_template(dt, custom_template)
                        elif date_format:
                            return self.format_date(dt, date_format)
                        else:
                            return dt.strftime('%Y-%m-%d')
        except:
            pass
        
        # 如果 EXIF 提取失败，回退到文件修改时间
        return self.get_file_date(file_path, date_format, custom_template)
    
    def format_date(self, dt, date_format):
        """根据格式字符串格式化日期"""
        # 支持的格式：YYYY-MM-DD, YYYY/MM/DD, YYYY-MM, YYYY/MM, YYYYMMDD 等
        format_mapping = {
            'YYYY-MM-DD': '%Y-%m-%d',
            'YYYY/MM/DD': '%Y/%m/%d',
            'YYYY-MM': '%Y-%m',
            'YYYY/MM': '%Y/%m',
            'YYYYMMDD': '%Y%m%d',
            'YYYY': '%Y',
        }
        
        format_str = format_mapping.get(date_format, date_format)
        return dt.strftime(format_str)
    
    def format_custom_template(self, dt, template):
        """根据自定义模板格式化日期路径"""
        # 支持 {year}, {month}, {day}, {hour}, {minute}, {second} 等
        # {month:02d} 表示补零的月份
        result = template
        
        # 提取所有 {key:format} 或 {key} 的形式
        import re
        pattern = r'\{(\w+)(?::([^}]+))?\}'
        
        def replace_var(match):
            key = match.group(1)
            fmt = match.group(2)
            
            value_map = {
                'year': dt.year,
                'month': dt.month,
                'day': dt.day,
                'hour': dt.hour,
                'minute': dt.minute,
                'second': dt.second,
            }
            
            if key not in value_map:
                return match.group(0)
            
            value = value_map[key]
            
            # 支持格式化
            if fmt:
                try:
                    return f"{value:{fmt}}"
                except:
                    return str(value)
            else:
                # 默认补零到 2 位
                if key in ['month', 'day', 'hour', 'minute', 'second']:
                    return f"{value:02d}"
                else:
                    return str(value)
        
        return re.sub(pattern, replace_var, result)

# --- 自适应高度的 ListWidget ---
class AutoHeightListWidget(QListWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff) # 关闭内部滚动
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.setIconSize(QSize(160, 120))
        self.setGridSize(QSize(180, 160)) # 统一网格大小
        self.setViewMode(QListWidget.ViewMode.IconMode)
        self.setResizeMode(QListWidget.ResizeMode.Adjust)
        self.setWordWrap(True)
        self.setSpacing(5)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.adjust_height()

    def adjust_height(self):
        # 计算行数
        count = self.count()
        if count == 0:
            self.setFixedHeight(0)
            return

        # 获取视口宽度
        width = self.viewport().width()
        grid_width = self.gridSize().width()
        if width <= 0 or grid_width <= 0:
            return

        # 每行能放几个
        cols = max(1, width // grid_width)
        rows = (count + cols - 1) // cols
        
        # 计算高度
        height = rows * self.gridSize().height() + 10 # 加上一点边距
        self.setFixedHeight(height)

# --- 日期分组组件 ---
class DateSection(QWidget):
    selection_changed = pyqtSignal() # 当内部选择发生变化时
    thumbnail_clicked = pyqtSignal(str)

    def __init__(self, date_str, parent=None):
        super().__init__(parent)
        self.date_str = date_str
        self.is_expanded = True
        
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.layout.setSpacing(0)
        
        # 头部
        self.header = QFrame()
        self.header.setStyleSheet("background-color: #2a2a2a; border-radius: 4px;")
        self.header.setFixedHeight(30)
        header_layout = QHBoxLayout(self.header)
        header_layout.setContentsMargins(5, 0, 5, 0)
        
        # 展开/折叠按钮
        self.toggle_btn = QToolButton()
        self.toggle_btn.setText("▼")
        self.toggle_btn.setStyleSheet("border: none; color: #aaa; font-weight: bold;")
        self.toggle_btn.clicked.connect(self.toggle_content)
        header_layout.addWidget(self.toggle_btn)
        
        # 勾选框
        self.checkbox = QCheckBox(date_str)
        self.checkbox.setStyleSheet("font-weight: bold; color: #ddd;")
        self.checkbox.setChecked(True)
        self.checkbox.stateChanged.connect(self.on_header_checkbox_changed)
        header_layout.addWidget(self.checkbox)
        
        # 数量标签
        self.count_label = QLabel("(0)")
        self.count_label.setStyleSheet("color: #888;")
        header_layout.addWidget(self.count_label)
        
        header_layout.addStretch()
        self.layout.addWidget(self.header)
        
        # 内容区域 (ListWidget)
        self.list_widget = AutoHeightListWidget()
        self.list_widget.itemChanged.connect(self.on_item_check_changed)
        self.list_widget.itemClicked.connect(self.on_item_clicked)
        self.layout.addWidget(self.list_widget)

    def toggle_content(self):
        self.is_expanded = not self.is_expanded
        self.list_widget.setVisible(self.is_expanded)
        self.toggle_btn.setText("▼" if self.is_expanded else "▶")

    def add_item(self, item):
        self.list_widget.addItem(item)
        self.update_count()
        self.list_widget.adjust_height()

    def update_count(self):
        self.count_label.setText(f"({self.list_widget.count()})")

    def on_header_checkbox_changed(self, state):
        # 头部勾选 -> 全选/全不选内部
        is_checked = (state == Qt.CheckState.Checked.value)
        self.list_widget.blockSignals(True) # 防止递归触发
        for i in range(self.list_widget.count()):
            item = self.list_widget.item(i)
            if is_checked:
                item.setCheckState(Qt.CheckState.Checked)
            else:
                item.setCheckState(Qt.CheckState.Unchecked)
        self.list_widget.blockSignals(False)
        self.selection_changed.emit()

    def on_item_check_changed(self, item):
        # 内部选择变化 -> 更新头部勾选状态
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

    def on_item_clicked(self, item):
        path = item.data(Qt.ItemDataRole.UserRole)
        if path:
            self.thumbnail_clicked.emit(path)

# --- 主窗口 ---
class LightroomImport(QMainWindow):
    request_load = pyqtSignal(list) # 信号：请求加载文件列表
    request_scan = pyqtSignal(str, bool) # 信号：请求扫描目录 (路径, 是否递归)
    request_import = pyqtSignal(list, str, str, bool, str, str, str)  # 文件列表, 目标路径, 模式, 是否按日期, 日期格式, 时间源, 自定义模板

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lightroom Import Clone (PyQt6)")
        self.resize(1400, 800)
        self.setStyleSheet(STYLESHEET)

        # 核心数据成员
        self.current_files = []
        self.target_directory = os.path.expanduser("~")  # 默认主文件夹
        self.selected_items = set()
        self.path_to_item = {} # 映射：路径 -> QListWidgetItem
        self.date_sections = {} # 映射：日期字符串 -> DateSection Widget
        
        # 配置成员
        self.date_format = 'YYYY-MM-DD'
        self.time_source = 'file_mtime'
        self.custom_template = ''
        self.updating_tree = False  # 防止递归更新
        self.duplicate_files = set()  # 存储重复文件的路径
        self.last_import_count = 0
        self.last_skipped_duplicates = 0
        
        # 初始化 UI
        self.setup_ui()
        
        # 初始化线程
        self.init_thread()

        # 设备检测定时器
        self.device_timer = QTimer(self)
        self.device_timer.timeout.connect(self.refresh_devices)
        self.device_timer.start(3000) # 每3秒检测一次
        self.refresh_devices() # 立即执行一次

    def init_thread(self):
        # 扫描线程
        self.scan_thread = QThread()
        self.scanner = ScannerWorker()
        self.scanner.moveToThread(self.scan_thread)
        
        self.request_scan.connect(self.scanner.scan)
        self.scanner.files_found.connect(self.on_files_found)
        self.scanner.finished.connect(self.on_scan_finished)
        
        self.scan_thread.start()

        # 缩略图线程
        self.thread = QThread()
        self.worker = ThumbnailWorker()
        self.worker.moveToThread(self.thread)
        
        # 连接信号
        self.request_load.connect(self.worker.process_files)
        self.worker.finished.connect(self.update_thumbnail)
        
        self.thread.start()
        
        # 导入线程
        self.import_thread = QThread()
        self.import_worker = ImportWorker()
        self.import_worker.moveToThread(self.import_thread)
        
        self.request_import.connect(self.import_worker.import_files)
        self.import_worker.progress.connect(self.on_import_progress)
        self.import_worker.finished.connect(self.on_import_finished)
        
        self.import_thread.start()

    def closeEvent(self, event):
        self.scanner.stop()
        self.worker.stop()
        self.import_worker.stop()
        self.scan_thread.quit()
        self.thread.quit()
        self.import_thread.quit()
        self.scan_thread.wait()
        self.thread.wait()
        self.import_thread.wait()
        event.accept()

    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # 1. 顶部栏 (Top Bar)
        main_layout.addWidget(self.create_top_bar())

        # 2. 中间三栏布局 (Splitter)
        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setHandleWidth(2)
        
        splitter.addWidget(self.create_left_panel())   # 源
        splitter.addWidget(self.create_center_panel()) # 网格
        splitter.addWidget(self.create_right_panel())  # 设置
        
        # 设置中间区域优先拉伸
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 4)
        splitter.setStretchFactor(2, 1)

        main_layout.addWidget(splitter)

        # 3. 底部栏 (Bottom Bar)
        main_layout.addWidget(self.create_bottom_bar())

    # --- UI 组件构建 ---

    def create_top_bar(self):
        container = QWidget()
        container.setFixedHeight(45)
        container.setStyleSheet("background-color: #252525; border-bottom: 1px solid #111;")
        layout = QHBoxLayout(container)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        self.mode_group = QButtonGroup(self)
        
        modes = ["复制 (Copy)", "移动 (Move)", "添加 (Add)", "复制为 DNG"]
        for i, text in enumerate(modes):
            btn = QPushButton(text)
            btn.setCheckable(True)
            btn.setStyleSheet("""
                QPushButton { border: none; font-weight: bold; color: #999; font-size: 14px; margin: 0 10px; }
                QPushButton:checked { color: white; text-decoration: underline; }
            """)
            if i == 0: btn.setChecked(True)
            self.mode_group.addButton(btn)
            layout.addWidget(btn)
            
        return container

    def create_left_panel(self):
        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(0,0,0,0)
        layout.setSpacing(5)
        
        # --- 设备部分 ---
        dev_title = QLabel(" 设备 (Devices)")
        dev_title.setFixedHeight(30)
        dev_title.setStyleSheet("font-weight: bold; background-color: #333; color: #eee; padding-left: 5px;")
        layout.addWidget(dev_title)

        self.device_list = QListWidget()
        self.device_list.setMaximumHeight(150)
        self.device_list.setStyleSheet("background-color: #252525; border: none;")
        self.device_list.itemClicked.connect(self.on_device_clicked)
        layout.addWidget(self.device_list)

        # --- 文件夹部分 ---
        folder_title_layout = QHBoxLayout()
        folder_title = QLabel(" 文件夹 (Folders)")
        folder_title.setStyleSheet("font-weight: bold; color: #eee;")
        
        self.recursive_check = QCheckBox("包含子文件夹")
        self.recursive_check.setStyleSheet("color: #aaa; font-size: 11px;")
        self.recursive_check.setToolTip("递归扫描选中的文件夹（慎用：大文件夹可能会很慢）")
        self.recursive_check.toggled.connect(self.on_recursive_toggled)
        
        folder_title_layout.addWidget(folder_title)
        folder_title_layout.addStretch()
        folder_title_layout.addWidget(self.recursive_check)
        
        title_frame = QFrame()
        title_frame.setFixedHeight(30)
        title_frame.setStyleSheet("background-color: #333;")
        title_frame.setLayout(folder_title_layout)
        folder_title_layout.setContentsMargins(5, 0, 5, 0)
        
        layout.addWidget(title_frame)
        
        # 文件系统模型
        self.fs_model = QFileSystemModel()
        self.fs_model.setRootPath(QDir.rootPath())
        self.fs_model.setFilter(QDir.Filter.NoDotAndDotDot | QDir.Filter.AllDirs | QDir.Filter.Drives)
        
        self.tree_view = QTreeView()
        self.tree_view.setModel(self.fs_model)
        self.tree_view.hideColumn(1) # Size
        self.tree_view.hideColumn(2) # Type
        self.tree_view.hideColumn(3) # Date
        self.tree_view.setHeaderHidden(True)
        
        # 信号连接
        self.tree_view.clicked.connect(self.on_folder_clicked)

        layout.addWidget(self.tree_view)
        return container

    def create_center_panel(self):
        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(0,0,0,0)
        
        # 使用 ScrollArea 包裹内容
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setStyleSheet("QScrollArea { border: none; background-color: #1b1b1b; }")
        
        self.scroll_content = QWidget()
        self.scroll_content.setStyleSheet("background-color: #1b1b1b;")
        self.scroll_layout = QVBoxLayout(self.scroll_content)
        self.scroll_layout.setContentsMargins(10, 10, 10, 10)
        self.scroll_layout.setSpacing(10)
        self.scroll_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        
        self.scroll_area.setWidget(self.scroll_content)
        
        layout.addWidget(self.scroll_area)
        return container

    def create_right_panel(self):
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        
        # 预览面板
        preview_group = QGroupBox("预览")
        preview_layout = QVBoxLayout()
        
        self.preview_label = QLabel()
        self.preview_label.setFixedSize(200, 160)
        self.preview_label.setStyleSheet("background-color: #333; border: 1px solid #555;")
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        self.file_info_label = QLabel("未选择文件")
        self.file_info_label.setWordWrap(True)
        self.file_info_label.setStyleSheet("color: #aaa; font-size: 11px;")
        
        preview_layout.addWidget(self.preview_label)
        preview_layout.addWidget(self.file_info_label)
        preview_group.setLayout(preview_layout)
        layout.addWidget(preview_group)
        
        # 导入选项
        options_group = QGroupBox("导入设置")
        options_layout = QVBoxLayout()
        
        self.mode_label = QLabel("模式: 复制")
        self.mode_label.setStyleSheet("color: #ccc;")
        options_layout.addWidget(self.mode_label)
        
        self.organize_checkbox = QCheckBox("按日期组织文件")
        self.organize_checkbox.setChecked(True)
        self.organize_checkbox.stateChanged.connect(self.refresh_target_tree)
        options_layout.addWidget(self.organize_checkbox)
        
        # --- 日期格式选择 ---
        date_format_layout = QHBoxLayout()
        date_format_layout.addWidget(QLabel("日期格式:"))
        self.date_format_combo = QComboBox()
        self.date_format_combo.addItems([
            'YYYY-MM-DD',
            'YYYY/MM/DD',
            'YYYY-MM',
            'YYYY/MM',
            'YYYYMMDD',
            'YYYY'
        ])
        self.date_format_combo.currentTextChanged.connect(self.on_date_format_changed)
        date_format_layout.addWidget(self.date_format_combo)
        options_layout.addLayout(date_format_layout)
        
        # --- 时间源选择 ---
        time_source_label = QLabel("时间源:")
        options_layout.addWidget(time_source_label)
        
        time_source_layout = QHBoxLayout()
        self.time_source_group = QButtonGroup()
        
        radio_file = QRadioButton("文件修改时间")
        radio_file.setChecked(True)
        radio_file.clicked.connect(lambda: self.on_time_source_changed('file_mtime'))
        self.time_source_group.addButton(radio_file)
        time_source_layout.addWidget(radio_file)
        
        radio_exif = QRadioButton("拍摄时间 (EXIF)")
        radio_exif.clicked.connect(lambda: self.on_time_source_changed('exif'))
        self.time_source_group.addButton(radio_exif)
        time_source_layout.addWidget(radio_exif)
        
        options_layout.addLayout(time_source_layout)
        
        # --- 自定义模板 ---
        template_label = QLabel("自定义模板 (可选):")
        template_label.setStyleSheet("color: #aaa; font-size: 11px;")
        options_layout.addWidget(template_label)
        
        self.template_input = QLineEdit()
        self.template_input.setPlaceholderText("例: {year}/{month:02d}/{day:02d} 或 {year}-{month}-{day}")
        self.template_input.setStyleSheet("background-color: #222; border: 1px solid #444; padding: 4px;")
        self.template_input.textChanged.connect(self.on_template_changed)
        options_layout.addWidget(self.template_input)
        
        template_help = QLabel("变量: {year}, {month}, {day}, {hour}, {minute}, {second}")
        template_help.setStyleSheet("color: #666; font-size: 10px;")
        options_layout.addWidget(template_help)
        
        options_group.setLayout(options_layout)
        layout.addWidget(options_group)
        
        # 目标位置 - 包含路径和树视图
        target_group = QGroupBox("目标位置")
        target_layout = QVBoxLayout()
        
        # 路径显示和浏览按钮
        path_frame = QWidget()
        path_layout = QHBoxLayout(path_frame)
        path_layout.setContentsMargins(0, 0, 0, 0)
        
        self.target_label = QLabel("主文件夹")
        self.target_label.setWordWrap(True)
        self.target_label.setStyleSheet("color: #888; font-size: 10px; word-break: break-all;")
        self.target_label.setMinimumHeight(30)
        
        self.browse_btn = QPushButton("浏览...")
        self.browse_btn.setMaximumWidth(80)
        self.browse_btn.clicked.connect(self.on_browse_target)
        
        path_layout.addWidget(self.target_label, 1)
        path_layout.addWidget(self.browse_btn)
        
        # 目录树
        self.target_tree = QTreeWidget()
        self.target_tree.setHeaderLabel("文件夹结构")
        self.target_tree.setMaximumHeight(250)
        self.target_tree.setStyleSheet("""
            QTreeWidget { background-color: #222; border: 1px solid #444; }
            QTreeWidget::item { padding: 2px; }
            QTreeWidget::item:selected { background-color: #4f4f4f; }
        """)
        
        # 连接勾选变化信号
        self.target_tree.itemChanged.connect(self.on_tree_item_changed)
        
        target_layout.addWidget(QLabel("路径:"))
        target_layout.addWidget(path_frame)
        target_layout.addWidget(QLabel("目录结构:"))
        target_layout.addWidget(self.target_tree)
        target_group.setLayout(target_layout)
        layout.addWidget(target_group)
        
        # 导入统计
        stats_group = QGroupBox("统计")
        stats_layout = QVBoxLayout()
        
        self.stats_label = QLabel("已选择: 0 文件")
        self.stats_label.setStyleSheet("color: #aaa;")
        stats_layout.addWidget(self.stats_label)
        
        stats_group.setLayout(stats_layout)
        layout.addWidget(stats_group)
        
        scroll.setWidget(content)
        return scroll

    def create_bottom_bar(self):
        container = QWidget()
        container.setFixedHeight(50)
        container.setStyleSheet("background-color: #252525; border-top: 1px solid #333;")
        layout = QHBoxLayout(container)
        
        self.progress_bar = QProgressBar()
        self.progress_bar.setFixedWidth(250)
        self.progress_bar.setVisible(False)
        
        self.status_label = QLabel("就绪")
        self.status_label.setStyleSheet("color: #aaa;")
        
        self.import_btn = QPushButton("导入")
        self.import_btn.setObjectName("ImportBtn")
        self.import_btn.setFixedSize(100, 32)
        self.import_btn.clicked.connect(self.on_import_clicked)
        
        layout.addWidget(self.status_label)
        layout.addWidget(self.progress_bar)
        layout.addStretch()
        layout.addWidget(self.import_btn)
        return container

    # --- 逻辑处理 ---
    
    def refresh_devices(self):
        """刷新可移动设备列表"""
        current_devices = set()
        partitions = psutil.disk_partitions(all=True)
        
        self.device_list.clear()
        
        for p in partitions:
            # 在 macOS 上，/Volumes 下的通常是可移动设备或挂载点
            # 在 Windows 上，可以检查 opts 是否包含 'removable' 或 'cdrom'
            is_removable = False
            icon_name = "SP_DriveHDIcon"
            
            if sys.platform == 'darwin':
                if p.mountpoint.startswith('/Volumes/'):
                    is_removable = True
                elif p.mountpoint == '/':
                    is_removable = True # 也可以显示系统盘
                    icon_name = "SP_DriveHDIcon"
            elif sys.platform == 'win32':
                if 'removable' in p.opts or 'cdrom' in p.opts:
                    is_removable = True
                else:
                    is_removable = True # 显示所有盘符
                    icon_name = "SP_DriveHDIcon"
            else:
                is_removable = True # Linux 等其他系统
            
            if is_removable:
                item = QListWidgetItem(f"{os.path.basename(p.mountpoint) or p.mountpoint}")
                item.setData(Qt.ItemDataRole.UserRole, p.mountpoint)
                item.setIcon(self.style().standardIcon(getattr(self.style().StandardPixmap, icon_name)))
                self.device_list.addItem(item)

    def on_device_clicked(self, item):
        """点击设备列表项"""
        path = item.data(Qt.ItemDataRole.UserRole)
        
        # 更新文件夹视图的根目录
        index = self.fs_model.setRootPath(path)
        self.tree_view.setRootIndex(index)
        
        # 清除文件树的选择
        self.tree_view.clearSelection()
        self.start_scan(path)

    def on_folder_clicked(self, index):
        """点击文件夹树"""
        path = self.fs_model.filePath(index)
        # 清除设备列表的选择
        self.device_list.clearSelection()
        self.start_scan(path)
        
    def on_recursive_toggled(self, checked):
        """包含子文件夹开关切换"""
        # 如果当前有选中的文件夹或设备，重新扫描
        current_path = None
        if self.device_list.currentItem():
            current_path = self.device_list.currentItem().data(Qt.ItemDataRole.UserRole)
        else:
            indexes = self.tree_view.selectedIndexes()
            if indexes:
                current_path = self.fs_model.filePath(indexes[0])
        
        if current_path:
            self.start_scan(current_path)

    def start_scan(self, path):
        """开始扫描目录"""
        if not path or not os.path.exists(path):
            return

        self.status_label.setText(f"正在扫描: {path}...")
        
        # 停止旧任务
        self.scanner.stop()
        self.worker.stop()
        
        # 清空现有视图
        # 移除所有 DateSection
        while self.scroll_layout.count():
            item = self.scroll_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()
        
        self.date_sections.clear()
        self.path_to_item.clear()
        self.current_files = []
        self.selected_items.clear()
        self.duplicate_files.clear()
        self.update_preview(None)
        
        recursive = self.recursive_check.isChecked()
        self.request_scan.emit(path, recursive)

    def on_files_found(self, files):
        """扫描到一批文件"""
        new_files = []
        
        # 暂停 UI 更新以提高性能
        self.scroll_area.setUpdatesEnabled(False)
        
        for full_path in files:
            self.current_files.append(full_path)
            new_files.append(full_path)
            
            # 获取日期用于分组 (简单使用修改时间，优化性能)
            try:
                mtime = os.path.getmtime(full_path)
                date_str = datetime.fromtimestamp(mtime).strftime('%Y-%m-%d')
            except:
                date_str = "Unknown Date"
            
            # 获取或创建分组
            if date_str not in self.date_sections:
                section = DateSection(date_str)
                section.selection_changed.connect(self.on_grid_selection_changed)
                section.thumbnail_clicked.connect(self.update_preview)
                
                # 按日期顺序插入 (简单的倒序插入，新的日期在上面)
                # 如果需要严格排序，可能需要更复杂的逻辑，这里假设扫描顺序或直接追加
                # 为了简单，我们直接添加到 layout 底部，然后可能需要排序？
                # 这里暂时直接添加
                self.scroll_layout.addWidget(section)
                self.date_sections[date_str] = section
            
            section = self.date_sections[date_str]
            
            # 创建 Item
            entry = os.path.basename(full_path)
            item = QListWidgetItem(entry)
            item.setData(Qt.ItemDataRole.UserRole, full_path)
            
            # 默认图标
            default_pix = QPixmap(100, 100)
            default_pix.fill(QColor("#333"))
            item.setIcon(QIcon(default_pix))
            
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked)
            
            section.add_item(item)
            self.path_to_item[full_path] = item
            
        self.scroll_area.setUpdatesEnabled(True)
            
        # 请求加载这些新文件的缩略图
        if new_files:
            self.request_load.emit(new_files)
            
        self.status_label.setText(f"已发现 {len(self.current_files)} 个文件...")
        
        # 触发一次选择更新
        self.on_grid_selection_changed()

    def on_scan_finished(self):
        """扫描完成"""
        self.status_label.setText(f"扫描完成，共 {len(self.current_files)} 个文件")
        # 扫描结束后执行一次重复检测
        self.update_grid_filter()

    def update_grid_filter(self):
        """根据当前设置隐藏重复文件"""
        if not self.target_directory:
            return

        self.updating_tree = True # 借用该标志防止递归刷新
        self.duplicate_files.clear()
        
        # 遍历所有分组的所有 Item
        for section in self.date_sections.values():
            list_widget = section.list_widget
            for i in range(list_widget.count()):
                item = list_widget.item(i)
                file_path = item.data(Qt.ItemDataRole.UserRole)
                
                is_duplicate = False
                if self.organize_checkbox.isChecked():
                    # 计算目标路径
                    path_str = self.get_path_from_file(file_path, use_template=bool(self.custom_template))
                    target_full_path = os.path.join(self.target_directory, path_str)
                    dest_file = os.path.join(target_full_path, os.path.basename(file_path))
                    
                    if os.path.exists(dest_file) and self.files_are_identical(file_path, dest_file):
                        is_duplicate = True
                        self.duplicate_files.add(file_path)
                
                # 隐藏并取消勾选重复文件
                item.setHidden(is_duplicate)
                if is_duplicate:
                    item.setCheckState(Qt.CheckState.Unchecked)
        
        self.updating_tree = False
        self.on_grid_selection_changed() # 刷新统计和右侧树

    def update_thumbnail(self, file_path, icon):
        # 通过映射直接找到 Item
        if file_path in self.path_to_item:
            item = self.path_to_item[file_path]
            item.setIcon(icon)
    
    def on_grid_selection_changed(self):
        """更新选择信息和预览"""
        if self.updating_tree: # 防止在 update_grid_filter 时重复刷新
            return
            
        self.selected_items.clear()
        last_selected_path = None
        
        for section in self.date_sections.values():
            list_widget = section.list_widget
            for i in range(list_widget.count()):
                item = list_widget.item(i)
                if item and not item.isHidden() and item.checkState() == Qt.CheckState.Checked:
                    path = item.data(Qt.ItemDataRole.UserRole)
                    if path:
                        self.selected_items.add(path)
                        last_selected_path = path
        
        # 更新统计
        self.stats_label.setText(f"已选择: {len(self.selected_items)} 文件")
        
        # 刷新目标树
        self.refresh_target_tree()
        
        # 显示最后一个选中文件的预览
        self.update_preview(last_selected_path)
    
    def update_preview(self, file_path):
        """更新预览面板"""
        if not file_path:
            self.preview_label.setText("无预览")
            self.file_info_label.setText("未选择文件")
            return
        
        # 尝试加载并显示缩略图/原图
        pixmap = QPixmap()
        ext = os.path.splitext(file_path)[1].lower()
        
        if ext in ['.arw', '.cr2', '.nef', '.dng', '.orf', '.rw2']:
            # 对于预览面板，我们尝试快速读取内嵌 JPEG
            try:
                with rawpy.imread(file_path) as raw:
                    try:
                        thumb = raw.extract_thumb()
                    except rawpy.LibRawNoThumbnailError:
                        thumb = None
                    
                    if thumb and thumb.format == rawpy.ThumbFormat.JPEG:
                        qimg = QImage.fromData(thumb.data)
                        if not qimg.isNull():
                            pixmap = QPixmap.fromImage(qimg)
                    else:
                        # 如果没有内嵌预览，使用半尺寸解码 (预览不需要全尺寸)
                        rgb = raw.postprocess(use_camera_wb=True, bright=1.0, user_sat=None, no_auto_bright=True, half_size=True)
                        h, w, ch = rgb.shape
                        qimg = QImage(rgb.data, w, h, ch * w, QImage.Format.Format_RGB888)
                        if not qimg.isNull():
                            pixmap = QPixmap.fromImage(qimg)
            except Exception:
                pass # 如果 RAW 读取失败，将在下面处理为空的情况
        else:
            # 普通图片直接加载
            pixmap.load(file_path)

        if not pixmap.isNull():
            scaled = pixmap.scaled(200, 160, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
            self.preview_label.setPixmap(scaled)
        else:
            # 失败或视频文件的占位符
            if ext in ['.mp4', '.mov', '.avi']:
                pix = QPixmap(200, 160)
                pix.fill(QColor("#2a4a60"))
                from PyQt6.QtGui import QPainter
                painter = QPainter(pix)
                painter.setPen(Qt.GlobalColor.white)
                painter.drawText(pix.rect(), Qt.AlignmentFlag.AlignCenter, f"VIDEO\n{ext}")
                painter.end()
                self.preview_label.setPixmap(pix)
            else:
                self.preview_label.setText(f"无法预览\n{ext}")
        
        # 文件信息
        filename = os.path.basename(file_path)
        filesize = os.path.getsize(file_path) / 1024 / 1024  # MB
        mtime = os.path.getmtime(file_path)
        date_str = datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')
        
        info_text = f"文件: {filename}\n大小: {filesize:.2f} MB\n修改: {date_str}"
        self.file_info_label.setText(info_text)
    
    def on_browse_target(self):
        """选择目标目录"""
        dir_path = QFileDialog.getExistingDirectory(
            self,
            "选择导入目标文件夹",
            self.target_directory
        )
        
        if dir_path:
            self.target_directory = dir_path
            self.target_label.setText(dir_path)
            self.status_label.setText(f"目标: {os.path.basename(dir_path)}")
            self.update_grid_filter()
    
    def on_import_clicked(self):
        """开始导入"""
        if not self.selected_items:
            QMessageBox.warning(self, "提示", "请先选择要导入的文件")
            return

        files_to_import = list(self.selected_items - self.duplicate_files)
        duplicates_in_selection = self.selected_items & self.duplicate_files

        self.last_import_count = len(files_to_import)
        self.last_skipped_duplicates = len(duplicates_in_selection)

        if not files_to_import:
            QMessageBox.warning(self, "提示", "所有选择的文件都已存在（无重复导入）")
            return
        
        # 获取导入模式
        mode_text = self.mode_group.checkedButton().text()
        if "复制" in mode_text:
            mode = 'copy'
        elif "移动" in mode_text:
            mode = 'move'
        else:
            mode = 'copy'
        
        # 更新模式标签
        self.mode_label.setText(f"模式: {mode_text}")
        
        # 获取日期组织选项
        organize_by_date = self.organize_checkbox.isChecked()
        
        # 获取日期格式和时间源
        date_format = self.date_format
        time_source = self.time_source
        custom_template = self.custom_template
        
        if organize_by_date:
            if custom_template:
                preview_fmt = f"模板: {custom_template}"
            else:
                preview_fmt = f"格式: {date_format}"
            msg = f"即将导入 {len(files_to_import)} 个文件\n{preview_fmt}\n时间源: {'拍摄时间' if time_source == 'exif' else '文件修改时间'}\n目标: {self.target_directory}"
        else:
            msg = f"即将导入 {len(files_to_import)} 个文件到: {self.target_directory}"
        
        if self.last_skipped_duplicates:
            msg += f"\n\n跳过 {self.last_skipped_duplicates} 个重复文件（哈希相同）"
        
        reply = QMessageBox.question(self, "确认导入", msg, QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        # 启动导入
        self.progress_bar.setValue(0)
        self.progress_bar.setVisible(True)
        self.import_btn.setEnabled(False)
        self.status_label.setText("正在导入...")
        
        self.request_import.emit(files_to_import, self.target_directory, mode, organize_by_date, date_format, time_source, custom_template)
    
    def on_import_progress(self, current, total):
        """导入进度更新"""
        self.progress_bar.setMaximum(total)
        self.progress_bar.setValue(current)
        self.status_label.setText(f"导入中: {current}/{total}")
    
    def on_import_finished(self, success):
        """导入完成"""
        self.progress_bar.setVisible(False)
        self.import_btn.setEnabled(True)
        
        if success:
            self.status_label.setText("导入完成！")
            QMessageBox.information(self, "成功", f"已导入 {self.last_import_count} 个文件到\n{self.target_directory}")
            
            # 刷新文件树和目标树
            self.refresh_file_tree()
        else:
            self.status_label.setText("导入失败！")
            QMessageBox.critical(self, "错误", "导入过程中出错，请检查权限和空间")
        
        # 清空选择
        self.grid_list.clearSelection()
        self.selected_items.clear()
        self.stats_label.setText("已选择: 0 文件")
        self.refresh_target_tree()
        self.refresh_target_tree()
    
    def on_date_format_changed(self, format_text):
        """日期格式变更回调"""
        self.date_format = format_text
        self.update_grid_filter()
    
    def on_time_source_changed(self, source):
        """时间源变更回调"""
        self.time_source = source
        self.update_grid_filter()
    
    def on_template_changed(self, text):
        """自定义模板变更回调"""
        self.custom_template = text.strip()
        self.update_grid_filter()
    
    def on_tree_item_changed(self, item, column):
        """目录树项勾选状态改变回调"""
        if column != 0 or self.updating_tree:
            return
        
        if item.font(0).italic() and item.data(0, Qt.ItemDataRole.UserRole):
            path_str = item.data(0, Qt.ItemDataRole.UserRole)
            checked = item.checkState(0) == Qt.CheckState.Checked
            
            for file_path, grid_item in self.path_to_item.items():
                file_dir = self.get_path_from_file(file_path, use_template=bool(self.custom_template))
                if file_dir == path_str and not grid_item.isHidden():
                    grid_item.setSelected(checked)
            
            self.on_grid_selection_changed()
    
    def refresh_file_tree(self):
        """刷新文件树并展开目标目录"""
        # 刷新文件系统模型
        self.fs_model.refresh()
        
        # 展开目标目录
        target_index = self.fs_model.index(self.target_directory)
        if target_index.isValid():
            self.tree_view.expand(target_index)
            self.tree_view.setCurrentIndex(target_index)
        
        # 刷新目标目录树
        self.refresh_target_tree()
    
    def refresh_target_tree(self):
        """刷新目标位置的目录树视图（按 Lightroom 规范）"""
        if self.updating_tree:
            return
        self.updating_tree = True
        
        # 阻塞信号防止 clear() 触发 itemChanged 导致的问题
        self.target_tree.blockSignals(True)
        self.target_tree.clear()
        
        if not os.path.exists(self.target_directory):
            self.target_tree.blockSignals(False)
            self.updating_tree = False
            return
        
        # 创建根节点
        root = QTreeWidgetItem(self.target_tree)
        root.setText(0, os.path.basename(self.target_directory) or self.target_directory)
        root.setIcon(0, self.style().standardIcon(self.style().StandardPixmap.SP_DirIcon))
        
        organize_by_date = self.organize_checkbox.isChecked()
        
        if self.selected_items and organize_by_date:
            # 计算将要创建的日期文件夹及文件分配
            import_structure = self.calculate_import_structure()
            
            # 显示现有目录结构
            self.populate_tree(root, self.target_directory, existing_only=True)
            
            # 添加导入预览（虚拟路径）
            self.add_import_preview(root, import_structure)
        else:
            # 仅显示现有目录结构
            self.populate_tree(root, self.target_directory, existing_only=False)
        
        root.setExpanded(True)
        self.target_tree.blockSignals(False)
        self.updating_tree = False
    
    def calculate_import_structure(self):
        """计算导入后的目录结构和文件分配，并过滤重复文件"""
        import_structure = {}  # path_str -> [filenames]
        self.duplicate_files.clear()
        
        for file_path in self.selected_items:
            if os.path.exists(file_path):
                # 根据配置获取日期路径
                if self.custom_template:
                    path_str = self.get_path_from_file(file_path, use_template=True)
                else:
                    path_str = self.get_path_from_file(file_path, use_template=False)
                
                # 检测目标目录中是否有同名文件
                target_full_path = os.path.join(self.target_directory, path_str)
                dest_file = os.path.join(target_full_path, os.path.basename(file_path))
                
                is_duplicate = False
                if os.path.exists(dest_file):
                    # 有同名文件，比对哈希
                    if self.files_are_identical(file_path, dest_file):
                        is_duplicate = True
                        self.duplicate_files.add(file_path)
                
                # 仅将非重复文件添加到导入结构
                if not is_duplicate:
                    if path_str not in import_structure:
                        import_structure[path_str] = []
                    import_structure[path_str].append(os.path.basename(file_path))
        
        return import_structure
    
    def files_are_identical(self, file1, file2):
        """比对两个文件是否完全相同"""
        import hashlib
        
        def hash_file(fpath):
            hasher = hashlib.md5()
            try:
                with open(fpath, 'rb') as f:
                    buf = f.read(65536)
                    while len(buf) > 0:
                        hasher.update(buf)
                        buf = f.read(65536)
                return hasher.hexdigest()
            except:
                return None
        
        hash1 = hash_file(file1)
        hash2 = hash_file(file2)
        
        return hash1 and hash2 and hash1 == hash2
    
    def get_path_from_file(self, file_path, use_template=False):
        """从文件获取路径字符串"""
        try:
            if self.time_source == 'exif':
                dt = self.get_exif_datetime(file_path)
                if dt is None:
                    dt = datetime.fromtimestamp(os.path.getmtime(file_path))
            else:
                dt = datetime.fromtimestamp(os.path.getmtime(file_path))
            
            if use_template:
                return self.format_custom_template(dt, self.custom_template)
            else:
                return self.format_date(dt, self.date_format)
        except:
            return datetime.fromtimestamp(os.path.getmtime(file_path)).strftime('%Y-%m-%d')
    
    def get_exif_datetime(self, file_path):
        """尝试从 EXIF 获取拍摄时间"""
        try:
            from PIL import Image
            from PIL.ExifTags import TAGS
            
            image = Image.open(file_path)
            exif_data = image._getexif()
            
            if exif_data:
                for tag_id, value in exif_data.items():
                    tag_name = TAGS.get(tag_id, tag_id)
                    if tag_name in ['DateTimeOriginal', 'DateTime']:
                        return datetime.strptime(value, '%Y:%m:%d %H:%M:%S')
        except:
            pass
        
        return None
    
    def format_date(self, dt, date_format):
        """根据格式字符串格式化日期"""
        format_mapping = {
            'YYYY-MM-DD': '%Y-%m-%d',
            'YYYY/MM/DD': '%Y/%m/%d',
            'YYYY-MM': '%Y-%m',
            'YYYY/MM': '%Y/%m',
            'YYYYMMDD': '%Y%m%d',
            'YYYY': '%Y',
        }
        
        format_str = format_mapping.get(date_format, date_format)
        return dt.strftime(format_str)
    
    def format_custom_template(self, dt, template):
        """根据自定义模板格式化日期路径"""
        import re
        result = template
        
        pattern = r'\{(\w+)(?::([^}]+))?\}'
        
        def replace_var(match):
            key = match.group(1)
            fmt = match.group(2)
            
            value_map = {
                'year': dt.year,
                'month': dt.month,
                'day': dt.day,
                'hour': dt.hour,
                'minute': dt.minute,
                'second': dt.second,
            }
            
            if key not in value_map:
                return match.group(0)
            
            value = value_map[key]
            
            if fmt:
                try:
                    return f"{value:{fmt}}"
                except:
                    return str(value)
            else:
                if key in ['month', 'day', 'hour', 'minute', 'second']:
                    return f"{value:02d}"
                else:
                    return str(value)
        
        return re.sub(pattern, replace_var, result)
    
    def populate_tree(self, parent_item, directory, max_depth=2, current_depth=0, existing_only=False):
        """递归填充树项目，显示现有的目录和文件"""
        if current_depth >= max_depth:
            return
        
        try:
            entries = sorted(os.listdir(directory))
            
            # 先添加文件夹
            for entry in entries:
                full_path = os.path.join(directory, entry)
                if os.path.isdir(full_path) and not entry.startswith('.'):
                    item = QTreeWidgetItem(parent_item)
                    item.setText(0, f"{entry}")
                    item.setIcon(0, self.style().standardIcon(self.style().StandardPixmap.SP_DirIcon))
                    # 存储原始名称用于匹配
                    item.setData(0, Qt.ItemDataRole.UserRole + 2, entry)
                    
                    # 递归添加子文件夹
                    self.populate_tree(item, full_path, max_depth, current_depth + 1, existing_only=existing_only)
            
            # 再添加文件（仅当深度>0 且不是仅显示现有时）
            if current_depth > 0 and not existing_only:
                for entry in entries:
                    full_path = os.path.join(directory, entry)
                    if os.path.isfile(full_path) and not entry.startswith('.'):
                        item = QTreeWidgetItem(parent_item)
                        item.setText(0, f"{entry}")
                        item.setIcon(0, self.style().standardIcon(self.style().StandardPixmap.SP_FileIcon))
        except PermissionError:
            pass
    
    def add_import_preview(self, root_item, import_structure):
        """添加导入预览节点（虚拟路径）- 混合在现有目录树中"""
        if not import_structure:
            return
        
        # 为每个路径添加子项和文件，直接混合到现有树中
        for path_str in sorted(import_structure.keys()):
            files = import_structure[path_str]
            file_count = len(files)
            
            # 处理多级目录
            path_parts = path_str.split('/')
            parent_item = root_item
            
            for i, part in enumerate(path_parts):
                # 在parent中查找是否已有该项 (优先匹配 UserRole)
                found_item = None
                for j in range(parent_item.childCount()):
                    child = parent_item.child(j)
                    # 匹配 UserRole 存储的原始值或显示文本
                    if child.data(0, Qt.ItemDataRole.UserRole + 2) == part:
                        found_item = child
                        break
                    if part in child.text(0): # 兜底逻辑
                        found_item = child
                        break
                
                # 检查此级目录物理上是否已存在
                partial_path = os.path.join(self.target_directory, *path_parts[:i+1])
                part_exists = os.path.exists(partial_path)
                
                if found_item:
                    item = found_item
                else:
                    item = QTreeWidgetItem(parent_item)
                    item.setText(0, f"{part} （{file_count}）")
                    item.setIcon(0, self.style().standardIcon(self.style().StandardPixmap.SP_DirIcon))
                    item.setData(0, Qt.ItemDataRole.UserRole + 2, part)
                
                # 如果是最后一级或者是新路径，确保视觉呈现（虚拟 vs 物理）
                if not part_exists:
                    font = item.font(0)
                    font.setItalic(True)
                    item.setFont(0, font)
                    item.setForeground(0, QColor("#666666"))
                else:
                    font = item.font(0)
                    font.setItalic(False)
                    item.setFont(0, font)
                    item.setForeground(0, QColor("#88CCFF"))
                
                # 如果是导入的叶子文件夹，添加勾选框
                if i == len(path_parts) - 1:
                    item.setCheckState(0, Qt.CheckState.Checked)  # 默认选中
                    item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
                    # 存储相对路径用于信号处理
                    item.setData(0, Qt.ItemDataRole.UserRole, '/'.join(path_parts))
                
                parent_item = item
            
            # 在最后一级添加文件计数信息
            if len(path_parts) > 0:
                last_item = parent_item
                # 添加文件列表提示
                file_info = QTreeWidgetItem(last_item)
                file_info.setText(0, f"📊 {file_count} 待导入文件")
                file_info.setIcon(0, self.style().standardIcon(self.style().StandardPixmap.SP_FileIcon))
                file_info.setForeground(0, QColor("#999999"))
                font = file_info.font(0)
                font.setItalic(True)
                file_info.setFont(0, font)
                
                # 获取该目录是否已存在
                exists = os.path.exists(os.path.join(self.target_directory, path_str))
                
                # 添加前3个文件作为预览
                for filename in sorted(files)[:3]:
                    file_item = QTreeWidgetItem(last_item)
                    file_item.setText(0, f"📄 {filename}")
                    file_item.setIcon(0, self.style().standardIcon(self.style().StandardPixmap.SP_FileIcon))
                    
                    if not exists:
                        font = file_item.font(0)
                        font.setItalic(True)
                        file_item.setFont(0, font)
                        file_item.setForeground(0, QColor("#666666"))
                    else:
                        file_item.setForeground(0, QColor("#88CCFF"))
                
                # 如果超过3个，显示省略号
                if len(files) > 3:
                    more_item = QTreeWidgetItem(last_item)
                    more_item.setText(0, f"... 等 {len(files) - 3} 个文件")
                    more_item.setForeground(0, QColor("#999999"))

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = LightroomImport()
    window.show()
    sys.exit(app.exec())
