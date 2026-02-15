import os

from PyQt6.QtCore import QObject, pyqtSignal


class ScannerWorker(QObject):
    """
    后台扫描文件，支持递归
    """

    files_found = pyqtSignal(list)  # 发现一批文件
    finished = pyqtSignal()  # 扫描完成

    def __init__(self) -> None:
        super().__init__()
        self.is_running = False
        self.valid_extensions = {
            ".jpg",
            ".jpeg",
            ".png",
            ".arw",
            ".cr2",
            ".nef",
            ".dng",
            ".rw2",
            ".mp4",
            ".mov",
        }

    def scan(self, root_path: str, recursive: bool) -> None:
        self.is_running = True
        batch: list[str] = []
        batch_size = 50

        if recursive:
            for root, dirs, files in os.walk(root_path):
                if not self.is_running:
                    break

                dirs[:] = [d for d in dirs if not d.startswith(".")]

                for file in files:
                    if not self.is_running:
                        break
                    if file.startswith("."):
                        continue

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
                    if not self.is_running:
                        break
                    full_path = os.path.join(root_path, entry)
                    if os.path.isfile(full_path) and not entry.startswith("."):
                        ext = os.path.splitext(entry)[1].lower()
                        if ext in self.valid_extensions:
                            batch.append(full_path)
            except OSError:
                pass

        if batch and self.is_running:
            self.files_found.emit(batch)

        self.finished.emit()
        self.is_running = False

    def stop(self) -> None:
        self.is_running = False

