#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Execute the actual patched PowerDevil resume/idle dispatch on a private fixture.

Only dependencies outside these two methods are replaced. This runs no system
bus, hardware writes, display service or suspend operation.
"""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def method(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


source = Path(sys.argv[1]).read_text()
harness = Path(sys.argv[2]).read_text()
body = '\n'.join(method(source, name) for name in
                 ('void Core::onResumeFromSuspend()', 'void Core::onResumingFromIdle()'))
with tempfile.TemporaryDirectory(prefix='powerdevil-resume-check-') as directory:
    root = Path(directory)
    (root / 'resume-methods.inc').write_text(body)
    (root / 'check.cpp').write_text(harness)
    flags = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    subprocess.run(['g++', '-std=c++20', '-fPIC', '-Wall', '-Wextra',
                    str(root / 'check.cpp'), '-o', str(root / 'check'), *flags], check=True)
    subprocess.run([str(root / 'check')], check=True, timeout=10)
