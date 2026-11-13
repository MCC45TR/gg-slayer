#!/usr/bin/python3
"""Render the shipped QML at every supported layout width without hardware access."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='slayer-qml-') as directory:
    result = subprocess.run([sys.argv[1], '--fixture'], env=dict(os.environ,
        QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
        QT_FORCE_STDERR_LOGGING='1', SLAYER_UI_SCREENSHOTS=directory),
        capture_output=True, text=True, timeout=100)
    assert result.returncode == 0, result.stdout + result.stderr
    errors = [line for line in result.stderr.splitlines() if 'qrc:/ui/' in line or 'Binding loop' in line or 'ReferenceError' in line or 'TypeError' in line]
    assert not errors, '\n'.join(errors)
    captures = list(Path(directory).glob('*.png'))
    assert len(captures) == 56, len(captures)
    assert all(file.stat().st_size > 4000 for file in captures)
print('Seven Kirigami pages at six sizes; no QML load/binding errors; hardware access disabled.')
