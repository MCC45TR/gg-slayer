#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Sign only this release's RPMs using an isolated, root-owned local key."""
from pathlib import Path
import os
import re
import stat
import subprocess

if os.geteuid() != 0:
    raise SystemExit('Run with sudo; the signing key remains root-only.')
root = Path(__file__).resolve().parent.parent
version = (root / 'VERSION').read_text().strip()
if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
    raise SystemExit('Invalid VERSION')
top = root / 'packaging/rpm'
candidates = sorted((top / 'RPMS').glob(f'*/slayerctl*-{version}-*.rpm'))
candidates += sorted((top / 'RPMS').glob(f'*/akmod-slayerctl-r9t-{version}-*.rpm'))
candidates += sorted((top / 'RPMS').glob(f'*/kmod-slayerctl-r9t-{version}-*.rpm'))
ite_version = '0.0.20261113.4'
candidates += sorted((top / 'RPMS').glob(f'*/akmod-ite8291r3-{ite_version}-*.rpm'))
candidates += sorted((top / 'RPMS').glob(f'*/ite8291r3-kmod-common-{ite_version}-*.rpm'))
expected = {'slayerctl', 'slayerctl-gui', 'slayerctl-kde', 'slayerctl-gnome', 'slayerctl-face'}
allowed = expected | {'akmod-slayerctl-r9t', 'slayerctl-r9t-kmod-common', 'kmod-slayerctl-r9t', 'akmod-ite8291r3', 'ite8291r3-kmod-common'}
packages = []
names = []
for package in candidates:
    name = subprocess.check_output(['rpm', '-qp', '--qf', '%{NAME}', str(package)], text=True)
    if name not in allowed or name in names:
        raise SystemExit('Unexpected or duplicate release package: ' + name)
    packages.append(package)
    names.append(name)
if not expected.issubset(names):
    raise SystemExit('Missing application packages for this version')
sources = sorted((top / 'SRPMS').glob(f'slayerctl-{version}-*.src.rpm'))
if len(sources) != 1:
    raise SystemExit('Expected one matching application source RPM')
packages += sorted((top / 'SRPMS').glob(f'ite8291r3-kmod-{ite_version}-*.src.rpm'))
packages += sources
packages += sorted((top / 'SRPMS').glob(f'slayerctl-r9t-kmod-{version}-*.src.rpm'))

keystore = Path('/var/lib/slayerctl-rpm-signing')
os.umask(0o077)
keystore.mkdir(mode=0o700, exist_ok=True)
info = keystore.lstat()
if not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o077:
    raise SystemExit('Signing directory must be a root-owned private directory')
gpg = ['gpg', '--homedir', str(keystore), '--batch']
identity = 'Slayer Control local RPM signing'
try:
    keys = subprocess.check_output(gpg + ['--with-colons', '--list-secret-keys'], text=True)
    if not any(line.startswith('sec:') for line in keys.splitlines()):
        subprocess.run(gpg + ['--pinentry-mode', 'loopback', '--passphrase', '',
                              '--quick-generate-key', identity, 'ed25519', 'sign', '2y'], check=True)
        keys = subprocess.check_output(gpg + ['--with-colons', '--list-secret-keys'], text=True)
    if sum(line.startswith('sec:') for line in keys.splitlines()) != 1:
        raise SystemExit('Expected one dedicated signing key')
    if not any(line.startswith('uid:') and identity in line for line in keys.splitlines()):
        raise SystemExit('Unexpected signing identity')
    fingerprint = next(line.split(':')[9] for line in keys.splitlines() if line.startswith('fpr:'))
    if not re.fullmatch(r'[A-F0-9]{40,64}', fingerprint):
        raise SystemExit('Invalid key fingerprint')
    public_key = top / 'slayerctl-local-signing.asc'
    public_key.write_bytes(subprocess.check_output(gpg + ['--armor', '--export', fingerprint]))
    public_key.chmod(0o644)
    subprocess.run(['rpmkeys', '--import', str(public_key)], check=True)
    for package in packages:
        subprocess.run(['rpmsign', '--define', f'_gpg_path {keystore}',
                        '--define', f'_openpgp_sign_id {fingerprint}',
                        '--define', '_gpg_sign_cmd_extra_args --batch --pinentry-mode loopback',
                        '--addsign', str(package)], check=True)
        subprocess.run(['rpmkeys', '--checksig', str(package)], check=True)
    (top / 'signing-fingerprint.txt').write_text(fingerprint + '\n')
    (top / 'signing-fingerprint.txt').chmod(0o644)
    print('Local RPM signing fingerprint:', fingerprint)
finally:
    subprocess.run(['gpgconf', '--homedir', str(keystore), '--kill', 'gpg-agent'], check=False)
