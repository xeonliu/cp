import os
import shutil
from typing import Iterable

from PyQt6.QtCore import QObject, pyqtSignal

from cp.util import hash_file
from cp.util.dates import get_exif_date, get_file_date


class ImportWorker(QObject):
    """
    在后台线程执行文件复制/移动，避免阻塞 UI
    """

    progress = pyqtSignal(int, int)  # 当前进度, 总数
    finished = pyqtSignal(bool)  # 是否成功

    def __init__(self) -> None:
        super().__init__()
        self.is_running = True

    def stop(self) -> None:
        self.is_running = False

    def import_files(
        self,
        file_list: Iterable[str],
        target_path: str,
        mode: str,
        organize_by_date: bool,
        date_format: str | None = None,
        time_source: str | None = None,
        custom_template: str | None = None,
    ) -> None:
        """
        mode: 'copy', 'move', 'add', 'dng'
        organize_by_date: 是否按日期组织
        date_format: 日期格式字符串，如 'YYYY-MM-DD', 'YYYY/MM/DD' 等
        time_source: 'file_mtime' (文件修改时间) 或 'exif' (拍摄时间)
        custom_template: 自定义模板，如 '{year}/{month}' 或 '{year}-{month:02d}'
        """
        self.is_running = True
        files = list(file_list)
        total = len(files)

        try:
            for idx, file_path in enumerate(files):
                if not self.is_running:
                    break

                dest_dir = target_path
                if organize_by_date:
                    if time_source == "exif":
                        date_str = get_exif_date(file_path, date_format, custom_template)
                    else:
                        date_str = get_file_date(file_path, date_format, custom_template)

                    if date_str:
                        dest_dir = os.path.join(target_path, date_str)
                        os.makedirs(dest_dir, exist_ok=True)

                filename = os.path.basename(file_path)
                dest_file = os.path.join(dest_dir, filename)

                if os.path.exists(dest_file):
                    src_hash = hash_file(file_path)
                    dst_hash = hash_file(dest_file)

                    if src_hash and dst_hash and src_hash == dst_hash:
                        self.progress.emit(idx + 1, total)
                        continue
                    base, ext = os.path.splitext(filename)
                    dest_file = os.path.join(dest_dir, f"{base}_copy{ext}")

                if mode == "move":
                    shutil.move(file_path, dest_file)
                elif mode == "copy":
                    shutil.copy2(file_path, dest_file)
                elif mode == "add":
                    pass
                elif mode == "dng":
                    shutil.copy2(file_path, dest_file)
                else:
                    shutil.copy2(file_path, dest_file)

                self.progress.emit(idx + 1, total)

            self.finished.emit(True)
        except Exception:
            self.finished.emit(False)
