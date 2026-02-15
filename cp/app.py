from PyQt6.QtWidgets import QApplication

from main import run_app as _run_app


def run_app() -> None:
    _run_app()


def main() -> None:
    app = QApplication.instance()
    if app is None:
        _run_app()
    else:
        _run_app()
