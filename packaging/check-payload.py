#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Validate the staged install without starting any hardware control."""
from pathlib import Path
import os
import stat
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

root = Path(sys.argv[1])
required = (
    '/usr/libexec/slayerctl-service', '/usr/libexec/slayerctl-desktop',
    '/usr/share/dbus-1/system.d/org.gamegaraj.Slayer1.conf',
    '/usr/share/dbus-1/system-services/org.gamegaraj.Slayer1.service',
    '/usr/share/polkit-1/rules.d/49-slayerctl-admin.rules',
    '/usr/bin/slayerctl', '/usr/libexec/slayerctl-helper',
    '/usr/bin/slayer-control', '/usr/bin/slayer-control-qt', '/usr/bin/slayer-face-auth', '/usr/bin/slayer-control-gtk',
    '/usr/share/slayerctl/tools/white_balance.py',
    '/usr/lib64/qt6/plugins/plasma/kcms/systemsettings/kcm_slayerctl.so',
    '/usr/share/applications/slayer-control.desktop',
    '/usr/share/applications/kcm_slayerctl.desktop',
    '/usr/lib/udev/rules.d/85-slayerctl-power.rules',
    '/usr/lib/systemd/system-preset/90-slayerctl.preset',
    '/etc/slayerctl/slayerctl.conf',
)
for name in required:
    assert (root / name.lstrip('/')).is_file(), name
for file in root.rglob('*'):
    if not file.is_file():
        continue
    relative = '/' + str(file.relative_to(root))
    mode = file.stat().st_mode
    assert not mode & (stat.S_ISUID | stat.S_ISGID | stat.S_IWOTH), relative
    assert not relative.startswith(('/usr/local/', '/home/', '/boot/')), relative
    assert not relative.startswith('/etc/pam.d/'), relative
helper = root / 'usr/libexec/slayerctl-helper'
assert os.access(helper, os.X_OK)
assert helper.read_bytes() == (root / 'usr/bin/slayerctl').read_bytes()
policy = ET.parse(root / 'usr/share/polkit-1/actions/org.gamegaraj.slayerctl.policy')
assert policy.find('.//annotate').text == '/usr/libexec/slayerctl-helper'
for unit in (root / 'usr/lib/systemd/system').glob('slayerctl*.service'):
    assert '/usr/local/' not in unit.read_text()
for name in ('oyun', 'gece', 'dalga'):
    assert (root / f'etc/slayerctl/rgb-profiles/{name}.conf').is_file()
with tempfile.TemporaryDirectory(prefix='slayer-install-check-') as directory:
    fixture = Path(directory)
    config = fixture / 'etc/slayerctl/slayerctl.conf'
    config.parent.mkdir(parents=True)
    config.write_text('# User hardware preferences must survive installation\n')
    previous = config.read_bytes()
    result = subprocess.run(['cmake', '--install', sys.argv[2]],
                            env=dict(os.environ, DESTDIR=directory),
                            text=True, capture_output=True, check=True)
    assert config.read_bytes() == previous, 'Existing configuration overwritten'
    assert (fixture / 'etc/slayerctl/rgb-profiles/oyun.conf').is_file()
print('RPM payload paths, permissions, helper identity and configuration: OK')
