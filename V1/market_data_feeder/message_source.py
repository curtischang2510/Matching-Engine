from pathlib import Path


class FileMessageSource:
    def __init__(self, path: Path):
        self.path = path

    def messages(self):
        with open(self.path, "r") as f:
            for line in f:
                yield line.strip()