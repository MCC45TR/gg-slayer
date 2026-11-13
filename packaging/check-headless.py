#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""The ctl broker and session bridge must work without a graphical frontend."""
import subprocess
import sys
for binary in sys.argv[1:]:
    dependencies = subprocess.check_output(['readelf', '-d', binary], text=True)
    for library in ('Qt6Gui', 'Qt6Widgets', 'Qt6Quick', 'Qt6Qml', 'KF6'):
        assert library not in dependencies, (binary, library)
print('ctl executables have no GUI, QML, Kirigami or KDE framework linkage.')
