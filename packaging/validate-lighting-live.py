#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Bounded real LED/UPower checks; restores the previous colors/effect/brightness."""
import os
from pathlib import Path
import subprocess
import time
import sys
import pwd

if os.geteuid() != 0:
    raise SystemExit('Administrator authentication is required for real sysfs writes.')
controls = next(Path('/sys/bus/hid/devices').glob('*048D:600B.*/controls'))
leds = sorted(Path('/sys/class/leds').glob('rgb:kbd_backlight*'))
assert len(leds) == 126
saved = [(p, p.joinpath('multi_intensity').read_text()) for p in leds]
brightness = Path('/sys/class/leds/rgb:kbd_backlight/brightness')
original_brightness = int(brightness.read_text())
original_effect = controls.joinpath('hardware_effect').read_text().split()

def call(method, *args):
    return subprocess.check_output(['busctl', '--system', 'call', 'org.freedesktop.UPower',
        '/org/freedesktop/UPower/KbdBacklight', 'org.freedesktop.UPower.KbdBacklight', method, *args], text=True).strip()

def stats():
    return dict((k,int(v)) for k,v in (s.split('=') for s in controls.joinpath('transfer_stats').read_text().split()))

def color(value):
    for p in leds:
        p.joinpath('multi_intensity').write_text(value + '\n')
    time.sleep(.06)
    assert all(p.joinpath('multi_intensity').read_text().strip() == value for p in leds)

try:
    start = stats()
    before = time.monotonic()
    color('11 73 149')
    end = stats()
    assert end['frames'] - start['frames'] == 1, (start,end)
    assert end['error'] == 0, end
    if controls.joinpath('white_point').exists():
        point=list(map(int,controls.joinpath('white_point').read_text().split()))
        model_text=controls.joinpath('color_profile').read_text().strip() if controls.joinpath('color_profile').exists() else 'off'
        if model_text!='off':
            sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
            from fit_color_profile import evaluate
            blob=bytes.fromhex(model_text);n,t=blob[5],blob[6]
            vertices=[list(blob[8+i*6:11+i*6]) for i in range(n)]
            outputs=[list(blob[11+i*6:14+i*6]) for i in range(n)]
            cells=[list(blob[8+n*6+i*4:12+n*6+i*4]) for i in range(t)]
            expected=evaluate(vertices,outputs,cells,[11,73,149])
        else:expected=[(v*w+127)//255 for v,w in zip([11,73,149],point)]
        assert list(map(int,controls.joinpath('frame_color').read_text().split()))==expected
        print('White-point correction reaches USB frame; logical RGB is preserved: PASS',flush=True)
    print(f'126 individual multicolor writes -> one frame, no transport error: PASS ({(time.monotonic()-before)*1000:.1f} ms including 60 ms settle)',flush=True)
    start = stats()
    before = time.monotonic()
    call('SetBrightness', 'i', '25')
    elapsed = (time.monotonic()-before)*1000
    assert int(brightness.read_text()) == 25
    assert call('GetBrightness') == 'i 25'
    end = stats()
    assert end['frames'] == start['frames'] and end['brightness'] > start['brightness'], (start,end)
    assert end['error'] == 0
    print(f'UPower brightness -> native brightness command, no color frame: PASS ({elapsed:.1f} ms)',flush=True)
    subprocess.check_call(['/usr/bin/slayerctl','keyboard','hardware','wave','--brightness','70'], stdout=subprocess.DEVNULL)
    assert call('GetBrightness') == 'i 35'
    call('SetBrightness','i','10')
    assert controls.joinpath('hardware_effect').read_text().split()[0] == '3'
    assert int(brightness.read_text()) == 10
    print('Effect brightness cache agrees with UPower; brightness preserves wave: PASS',flush=True)
    if '--visual' in sys.argv:
        print('Visual sequence begins in three seconds: RED, GREEN, BLUE, then white low/high/off/high.',flush=True)
        time.sleep(3)
        call('SetBrightness','i','30')
        for rgb in ['255 0 0','0 255 0','0 0 255']:
            color(rgb);time.sleep(2)
        color('255 255 255')
        user_id = int(os.environ['PKEXEC_UID'])
        assert user_id > 0
        username = pwd.getpwuid(user_id).pw_name
        for level in [5,40,0,40]:
            subprocess.check_call(['/usr/sbin/runuser','-u',username,'--','/usr/bin/env',
                'DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/'+str(user_id)+'/bus',
                '/usr/bin/qdbus-qt6','org.kde.Solid.PowerManagement',
                '/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl',
                'org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl.setKeyboardBrightness',str(level)])
            time.sleep(1.5)
            assert int(brightness.read_text()) == level
finally:
    before_restore = stats()
    controls.joinpath('buffer_input').write_text('1\n')
    try:
        for p, value in saved:
            p.joinpath('multi_intensity').write_text(value)
        brightness.write_text(str(original_brightness) + '\n')
    finally:
        controls.joinpath('buffer_input').write_text('0\n')
    if original_effect[0] != '51':
        controls.joinpath('hardware_effect').write_text(' '.join(original_effect) + '\n')
    call('SetBrightness','i',str(original_brightness))
    after_restore = stats()
    assert after_restore['frames'] - before_restore['frames'] == 1, (before_restore,after_restore)
    assert after_restore['brightness'] - before_restore['brightness'] <= 1, (before_restore,after_restore)
    print('Buffered restore drains LED work and commits exactly one frame: PASS',flush=True)
    print('Original colors, effect and brightness restored.',flush=True)
