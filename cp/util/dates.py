import os
from datetime import datetime
from typing import Optional


def format_date(dt: datetime, date_format: str) -> str:
    format_mapping = {
        "YYYY-MM-DD": "%Y-%m-%d",
        "YYYY/MM/DD": "%Y/%m/%d",
        "YYYY-MM": "%Y-%m",
        "YYYY/MM": "%Y/%m",
        "YYYYMMDD": "%Y%m%d",
        "YYYY": "%Y",
    }
    format_str = format_mapping.get(date_format, date_format)
    return dt.strftime(format_str)


def format_custom_template(dt: datetime, template: str) -> str:
    import re

    result = template
    pattern = r"\{(\w+)(?::([^}]+))?\}"

    def replace_var(match: "re.Match[str]") -> str:
        key = match.group(1)
        fmt = match.group(2)

        value_map = {
            "year": dt.year,
            "month": dt.month,
            "day": dt.day,
            "hour": dt.hour,
            "minute": dt.minute,
            "second": dt.second,
        }

        if key not in value_map:
            return match.group(0)

        value = value_map[key]

        if fmt:
            try:
                return f"{value:{fmt}}"
            except Exception:
                return str(value)
        if key in ["month", "day", "hour", "minute", "second"]:
            return f"{value:02d}"
        return str(value)

    return re.sub(pattern, replace_var, result)


def get_file_date(
    file_path: str, date_format: Optional[str], custom_template: Optional[str]
) -> Optional[str]:
    try:
        file_mtime = os.path.getmtime(file_path)
        dt = datetime.fromtimestamp(file_mtime)

        if custom_template:
            return format_custom_template(dt, custom_template)
        if date_format:
            return format_date(dt, date_format)
        return dt.strftime("%Y-%m-%d")
    except Exception:
        return None


def get_exif_date(
    file_path: str, date_format: Optional[str], custom_template: Optional[str]
) -> Optional[str]:
    try:
        from PIL import Image
        from PIL.ExifTags import TAGS

        image = Image.open(file_path)
        exif_data = image._getexif()

        if exif_data:
            for tag_id, value in exif_data.items():
                tag_name = TAGS.get(tag_id, tag_id)
                if tag_name in ["DateTimeOriginal", "DateTime"]:
                    dt = datetime.strptime(value, "%Y:%m:%d %H:%M:%S")
                    if custom_template:
                        return format_custom_template(dt, custom_template)
                    if date_format:
                        return format_date(dt, date_format)
                    return dt.strftime("%Y-%m-%d")
    except Exception:
        pass

    return get_file_date(file_path, date_format, custom_template)

