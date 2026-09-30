"""Package the ready-to-use clients and their source without build caches."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


root = Path(__file__).resolve().parent
archive = root.parent / "Swiftwing-App-V1.9.zip"

with ZipFile(archive, mode="w", compression=ZIP_DEFLATED, compresslevel=9) as bundle:
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if path.suffix == ".nupkg":
            continue
        if relative.parent == Path("Windows") and path.suffix == ".xml":
            continue
        if "__pycache__" in relative.parts:
            continue
        bundle.write(path, Path("Swiftwing-App") / relative)

print(archive)
