"""Pack the portable folder for a release: python -I make_release_zip.py <folder> <zip> <root name>"""
import os
import sys
import zipfile

folder, target, root_name = sys.argv[1:4]
count = 0
with zipfile.ZipFile(target, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for current, _, names in os.walk(folder):
        for name in sorted(names):
            path = os.path.join(current, name)
            relative = os.path.relpath(path, folder).replace(os.sep, "/")
            archive.write(path, root_name + "/" + relative)
            count += 1

with zipfile.ZipFile(target) as archive:
    assert archive.testzip() is None
    names = archive.namelist()
    for required in ("starcapp.exe", "LICENSE.txt", "NOTICE.txt", "licenses/qtkeychain.txt"):
        assert root_name + "/" + required in names, required
    forbidden = [
        n
        for n in names
        if n.lower().endswith((".starc", ".fountain", ".pri")) or "client_secret" in n.lower()
    ]
    assert not forbidden, forbidden
print("files", count, "zip MB", round(os.path.getsize(target) / 1024 / 1024, 1))
