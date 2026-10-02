#!/usr/bin/env python3
"""Fail closed if tracked files look like they contain live secrets.

Looks for:
  - GUID-shaped VWorld keys near key/vworld usage
  - password= / password_portable= / password_dpapi= with a credential-like value
    (a DPAPI blob only opens for one Windows user, but it still is that user's password)
  - Authorization: Bearer <token>
  - account/key files that must never be tracked (config/*-local.ini, *-account.ini,
    secrets.ini, ka-hgis-vworld.ini), whatever their content

Allowlisted test fakes: 11111111-2222-..., AAAAAAAA-BBBB-..., and a few other
fixture GUIDs used in workflow tests, plus the secretvalueN tokens the log-masking
tests feed in on purpose.
"""
from __future__ import annotations

import re
import subprocess
import sys

ALLOW_GUIDS = {
    "11111111-2222-3333-4444-555555555555",
    "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE",
    "00000000-1111-2222-3333-444455556666",
    "11112222-3333-4444-5555-666677778888",
    "0A1B2C3D-4E5F-6071-8293-A4B5C6D7E8F9",  # tests/test_log_session.cpp masking fixture
}

GUID_RE = re.compile(
    r"(?i)\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\b"
)
# Config-style assignment only (no space before '='). Value must look like a secret.
PASSWORD_RE = re.compile(
    r"(?i)(?<![\w])password(?:_portable|_dpapi)?=([A-Za-z0-9_./+=\-]{4,})"
)
# Files that only ever hold accounts or keys. Tracking one is a leak even when empty now.
SECRET_FILE_RE = re.compile(
    r"(?i)(^|/)(secrets\.ini|ka-hgis-vworld\.ini|[^/]*-account\.ini|config/[^/]*-local\.ini)$"
)
# Fake values the log-masking tests write on purpose (tests/test_log_session.cpp).
FAKE_SECRET_RE = re.compile(r"(?i)^secretvalue\d+$")
BEARER_RE = re.compile(r"(?i)Authorization\s*:\s*Bearer\s+(\S+)")

SKIP_SUFFIX = {
    ".png", ".jpg", ".jpeg", ".gif", ".webp", ".ico", ".bmp",
    ".pdf", ".zip", ".7z", ".gz", ".exe", ".dll", ".pdb",
    ".gpkg", ".shp", ".dbf", ".shx", ".sbn", ".sbx", ".tif", ".tiff",
    ".mp4", ".wav", ".o", ".obj", ".lib", ".a",
}

hits: list[str] = []
files = subprocess.check_output(["git", "ls-files", "-z"], text=False).split(b"\0")
for raw in files:
    if not raw:
        continue
    path = raw.decode("utf-8", "surrogateescape")
    lower = path.lower()
    if SECRET_FILE_RE.search(lower):
        hits.append(f"{path}: account/key file is tracked (add it to .gitignore and untrack it)")
        continue
    if any(lower.endswith(s) for s in SKIP_SUFFIX):
        continue
    try:
        data = open(path, "rb").read()
    except OSError:
        continue
    if b"\0" in data[:8192]:
        continue
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        try:
            text = data.decode("cp949")
        except UnicodeDecodeError:
            continue
    for i, line in enumerate(text.splitlines(), 1):
        ctx = line.lower()
        for m in GUID_RE.finditer(line):
            g = m.group(0)
            if g.upper() in {x.upper() for x in ALLOW_GUIDS}:
                continue
            if (
                ("vworld" in ctx)
                or ("api.vworld" in ctx)
                or ("key=" in ctx)
                or ("key%3d" in ctx)
                or re.search(r"(?i)key\s*[:=]", line)
            ):
                hits.append(f"{path}:{i}: GUID-like key {g}")
        for m in PASSWORD_RE.finditer(line):
            val = m.group(1)
            # JS/C++ locals: const password=form / password=document.getElementById
            if re.match(r"(?i)(form|document|new|null|undefined|true|false)\b", val):
                continue
            if re.search(r"(?i)\b(const|let|var|auto)\s+\*?password\s*=", line):
                continue
            if FAKE_SECRET_RE.match(val):
                continue
            # Test code that asserts the token is absent.
            if "contains" in ctx and "password" in ctx:
                continue
            hits.append(f"{path}:{i}: password assignment ({val[:4]}...)")
        for m in BEARER_RE.finditer(line):
            token = m.group(1).strip().strip("\"'")
            if token.lower() in {"<", "token", "<token>", "xxx", "...", "$token"} or token.startswith("<"):
                continue
            hits.append(f"{path}:{i}: Authorization Bearer")

if hits:
    print("Secret scan FAILED:")
    for h in hits[:50]:
        print(" ", h)
    if len(hits) > 50:
        print(f"  ... and {len(hits) - 50} more")
    sys.exit(1)
print("Secret scan OK")
