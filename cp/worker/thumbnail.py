import os

import rawpy
from PyQt6.QtCore import QObject, Qt, pyqtSignal
from PyQt6.QtGui import QIcon, QImage, QPixmap, QColor, QPainter


class ThumbnailWorker(QObject):
    """
    在后台线程加载图片，避免阻塞 UI
    """

    finished = pyqtSignal(str, QIcon)  # 信号：路径, 图标

    def __init__(self) -> None:
        super().__init__()
        self.is_running = False

    def process_files(self, file_paths: list[str]) -> None:
        self.is_running = True
        for path in file_paths:
            if not self.is_running:
                break

            ext = os.path.splitext(path)[1].lower()
            pixmap = QPixmap()

            if ext in [".arw", ".cr2", ".nef", ".dng", ".orf", ".rw2"]:
                pixmap = self._load_raw_thumbnail(path, ext)
            elif ext in [".mp4", ".mov", ".avi"]:
                pixmap = self._make_video_placeholder(ext)
            else:
                pixmap.load(path)
                if not pixmap.isNull():
                    pixmap = pixmap.scaled(
                        200,
                        200,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    )

            if not pixmap.isNull():
                icon = QIcon(pixmap)
                self.finished.emit(path, icon)

        self.is_running = False

    def stop(self) -> None:
        self.is_running = False

    def _load_raw_thumbnail(self, path: str, ext: str) -> QPixmap:
        pixmap = QPixmap()
        try:
            with rawpy.imread(path) as raw:
                try:
                    thumb = raw.extract_thumb()
                except rawpy.LibRawNoThumbnailError:
                    thumb = None

                if thumb:
                    if thumb.format == rawpy.ThumbFormat.JPEG:
                        qimg = QImage.fromData(thumb.data)
                    elif thumb.format == rawpy.ThumbFormat.BITMAP:
                        rgb = raw.postprocess(
                            use_camera_wb=True,
                            bright=1.0,
                            user_sat=None,
                            no_auto_bright=True,
                            half_size=True,
                        )
                        h, w, ch = rgb.shape
                        qimg = QImage(rgb.data, w, h, ch * w, QImage.Format.Format_RGB888)
                    else:
                        qimg = QImage()
                else:
                    rgb = raw.postprocess(
                        use_camera_wb=True,
                        bright=1.0,
                        user_sat=None,
                        no_auto_bright=True,
                        half_size=True,
                    )
                    h, w, ch = rgb.shape
                    qimg = QImage(rgb.data, w, h, ch * w, QImage.Format.Format_RGB888)

                if not qimg.isNull():
                    pixmap = QPixmap.fromImage(qimg)
                    pixmap = pixmap.scaled(
                        200,
                        200,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    )
        except Exception as e:
            print(f"Error reading RAW {path}: {e}")
            pixmap = QPixmap(150, 100)
            pixmap.fill(QColor("#502020"))
            painter = QPainter(pixmap)
            painter.setPen(Qt.GlobalColor.white)
            painter.drawText(pixmap.rect(), Qt.AlignmentFlag.AlignCenter, f"RAW Error\n{ext}")
            painter.end()
        return pixmap

    def _make_video_placeholder(self, ext: str) -> QPixmap:
        pixmap = QPixmap(150, 100)
        pixmap.fill(QColor("#2a4a60"))
        painter = QPainter(pixmap)
        painter.setPen(Qt.GlobalColor.white)
        painter.drawText(pixmap.rect(), Qt.AlignmentFlag.AlignCenter, f"VIDEO\n{ext}")
        painter.end()
        return pixmap

