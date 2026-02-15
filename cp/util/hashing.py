import hashlib


def hash_file(file_path: str, block_size: int = 65536) -> str | None:
    hasher = hashlib.md5()
    try:
        with open(file_path, "rb") as f:
            buf = f.read(block_size)
            while len(buf) > 0:
                hasher.update(buf)
                buf = f.read(block_size)
        return hasher.hexdigest()
    except Exception:
        return None


def files_are_identical(file1: str, file2: str) -> bool:
    hash1 = hash_file(file1)
    hash2 = hash_file(file2)
    return bool(hash1 and hash2 and hash1 == hash2)

