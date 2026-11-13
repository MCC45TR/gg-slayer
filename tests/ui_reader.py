#!/usr/bin/python3
"""Representative, static UI data. Never opens any hardware or runs helpers."""
from pathlib import Path
import os
import sys

if sys.argv[1:] != ['ui-data']:
    raise SystemExit('The UI fixture supports read-only ui-data only.')
data = (Path(__file__).parent / 'fixtures/ui-data.tsv').read_text()
if os.environ.get('SLAYER_UI_FIXTURE_MODE') == 'blocked':
    data = data.replace('value\thardware_writes\tallowed', 'value\thardware_writes\tblocked')
    data += 'value\thardware_write_reason\tFixture: BIOS uyumsuz.\n'
    lines = []
    for line in data.splitlines():
        fields = line.split('\t')
        if len(fields) == 5 and fields[0] == 'cap' and fields[1] not in ('info', 'sensors', 'usb') and fields[2] == 'ready':
            fields[2] = 'blocked'
            fields[4] = 'Fixture: BIOS uyumsuz.'
        lines.append('\t'.join(fields))
    data = '\n'.join(lines)
data += '\nvalue\tkeyboard_layout\tTurkish\nvalue\tkeyboard_layout_source\tfixture\n'
print(data, end='\n')
