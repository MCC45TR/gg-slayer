#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Private camera fixture. No hardware access or privileged writes."""
import os
from pathlib import Path
import sys
import time

assert sys.argv[1:] == ['settings', 'camera', 'status']
root = Path(os.environ['SLAYER_CAMERA_FIXTURE'])
with (root / 'reads').open('a') as log:
    log.write('read\n')
value = (root / 'state').read_text().strip()
if value == 'hang':
    time.sleep(20)
elif value == 'invalid':
    print('camera: 2 (off)')
elif value == 'inconsistent':
    print('camera: 1 (off)')
elif value == 'large':
    print('x' * 8192)
elif value in ('0', '1'):
    print(f'camera: {value} ({"on" if value == "1" else "off"})')
else:
    raise SystemExit(4)
