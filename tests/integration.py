#!/usr/bin/env python3
"""All mutation tests target a temporary directory, never real sysfs/USB/EC."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

BIN = str(Path(sys.argv.pop(1)).resolve())

class Hardware(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='slayerctl-test-')
        self.root = Path(self.tmp.name)
        self.put('/sys/class/dmi/id/sys_vendor', 'GAME GARAJ')
        self.put('/sys/class/dmi/id/product_name', 'SLAYER R9T')
        self.put('/sys/class/dmi/id/product_sku', 'R9T V1')
        self.put('/sys/class/dmi/id/bios_version', 'N.1.46GAR16')
        self.put('/proc/sys/kernel/osrelease', 'fixture')
        for i in range(3):
            p = '/sys/class/leds/rgb:kbd_backlight' + (f'_{i}' if i else '')
            for n,v in {'max_brightness':'50','brightness':'25','multi_index':'red green blue','multi_intensity':'10 20 30','multi_max_intensity':'255 255 255'}.items():
                self.put(p+'/'+n,v)
        self.put('/sys/bus/hid/devices/0003:048D:600B.0001/controls/buffer_input','0')
        for n,v in {'type':'Battery','capacity':'70','charge_control_end_threshold':'100','charge_control_start_threshold':'50'}.items():
            self.put('/sys/class/power_supply/BAT0/'+n,v)
        self.put('/sys/firmware/acpi/platform_profile','balanced')
        self.put('/sys/firmware/acpi/platform_profile_choices','low-power balanced performance')
        for port,cls in [('1-1','03'),('1-2','08'),('1-3','09'),('1-4','ff'),('1-5','00'),('1-6','00')]:
            for n,v in {'idVendor':'1234','idProduct':'5678','bDeviceClass':cls,'product':'Fixture','power/control':'on','power/wakeup':'enabled'}.items():
                self.put('/sys/bus/usb/devices/'+port+'/'+n,v)
        self.put('/sys/bus/usb/devices/1-4:1.0/bInterfaceClass','ff')
        self.put('/sys/bus/usb/devices/1-5:1.0/bInterfaceClass','03')
        self.put('/sys/bus/usb/devices/1-6:1.0/bInterfaceClass','ff')
    def tearDown(self): self.tmp.cleanup()
    def put(self,p,v):
        p=self.root/p.lstrip('/');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(v+'\n')
    def get(self,p): return (self.root/p.lstrip('/')).read_text().strip()
    def snap(self):
        return {str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file() and not p.is_symlink()}
    def runcli(self,*args,ok=True):
        p=subprocess.run([BIN,'--root',str(self.root),*args],capture_output=True,text=True,timeout=8)
        self.assertEqual(p.returncode==0,ok,(args,p.returncode,p.stdout,p.stderr))
        self.assertNotIn('AddressSanitizer',p.stderr)
        self.assertNotIn('runtime error:',p.stderr)
        return p.stdout+p.stderr
    def test_telemetry_is_passive_and_preserves_missing_values(self):
        self.put('/proc/stat', 'cpu  100 0 40 200 10 0 0 0 0 0')
        self.put('/sys/class/hwmon/hwmon0/name', 'slayer_r9t')
        self.put('/sys/class/hwmon/hwmon0/fan1_label', 'CPU fan')
        self.put('/sys/class/hwmon/hwmon0/fan1_input', '0')
        self.put('/sys/class/hwmon/hwmon0/fan2_label', 'GPU fan')
        self.put('/sys/class/hwmon/hwmon0/fan2_input', '1800')
        old=self.snap()
        output=self.runcli('telemetry')
        self.assertIn('CPU fan\trpm\t0',output)
        self.assertIn('GPU fan\trpm\t1800',output)
        self.assertNotIn('gpu_usage',output)
        self.assertNotIn('key-rgb',output)
        self.assertNotIn('value\tprofile',output)
        self.assertEqual(old,self.snap())

    def test_fan_toggle_and_mode_are_narrow_and_validated(self):
        self.put('/sys/devices/platform/slayerctl/fan_mode','auto')
        self.put('/sys/devices/platform/slayerctl/fan_modes_available','auto boost')
        before=self.snap()
        self.assertEqual(self.runcli('fan','mode').strip(),'auto')
        self.assertEqual(before,self.snap())
        self.runcli('fan','toggle')
        self.assertEqual(self.get('/sys/devices/platform/slayerctl/fan_mode'),'boost')
        self.runcli('fan','toggle')
        self.assertEqual(self.get('/sys/devices/platform/slayerctl/fan_mode'),'auto')
        self.put('/sys/devices/platform/slayerctl/fan_mode','firmware-custom')
        before=self.snap();self.runcli('fan','toggle',ok=False);self.assertEqual(before,self.snap())
        self.put('/sys/devices/platform/slayerctl/fan_mode','auto')
        self.put('/sys/class/dmi/id/bios_version','unverified')
        before=self.snap();self.runcli('fan','toggle',ok=False);self.assertEqual(before,self.snap())

    def test_kde_ownership_prevents_boot_rgb_and_brightness_writes(self):
        self.runcli('config','set','lighting_owner','kde','brightness_owner','kde',
                    'keyboard_color','FF0000','keyboard_brightness','80')
        old=self.snap()
        self.runcli('apply-config')
        self.assertEqual(old,self.snap())
        self.runcli('config','set','lighting_owner','invalid',ok=False)
        self.assertEqual(old,self.snap())

    def test_identical_configuration_does_not_rewrite(self):
        self.runcli('config','set','fan_mode','auto')
        config=self.root/'etc/slayerctl/slayerctl.conf'
        stamp=config.stat().st_mtime_ns
        self.runcli('config','set','fan_mode','auto')
        self.assertEqual(stamp,config.stat().st_mtime_ns)

    def test_temporary_rgb_restores_firmware_effect(self):
        endpoint='/sys/bus/hid/devices/0003:048D:600B.0001/controls/hardware_effect'
        for args in [('keyboard','identify','1'),
                     ('keyboard','effect','wave','--duration','1','--fps','10'),
                     ('keyboard','effect','breathe','--duration','1','--fps','10')]:
            with self.subTest(args=args):
                self.put(endpoint,'3 5 25 1')
                old=self.snap()
                process=subprocess.Popen([BIN,'--root',str(self.root),*args],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                try:
                    deadline=time.monotonic()+1
                    color='/sys/class/leds/rgb:kbd_backlight/multi_intensity'
                    brightness='/sys/class/leds/rgb:kbd_backlight/brightness'
                    while time.monotonic()<deadline and process.poll() is None:
                        if self.get(color)!='10 20 30' or self.get(brightness)!='25':
                            break
                        time.sleep(.002)
                    self.assertIsNone(process.poll(),'Temporary operation ended before state simulation')
                    # Model the native driver switching to USER mode on a
                    # buffered frame (including restoration after breathe).
                    self.put(endpoint,'51 5 25 0')
                    out,err=process.communicate(timeout=8)
                    self.assertEqual(process.returncode,0,out+err)
                    self.assertEqual(self.snap(),old)
                finally:
                    if process.poll() is None:process.kill()
                    process.communicate(timeout=2)

    def test_buffer_store_failure_restores_firmware_effect(self):
        endpoint='/sys/bus/hid/devices/0003:048D:600B.0001/controls/hardware_effect'
        self.put(endpoint,'3 5 25 1')
        self.put('/.slayerctl-fail-write-after','1')
        effect=self.root/endpoint.lstrip('/')
        stamp=effect.stat().st_mtime_ns
        old=self.snap()
        self.runcli('keyboard','color','ffffff',ok=False)
        self.assertEqual(self.snap(),old)
        self.assertNotEqual(effect.stat().st_mtime_ns,stamp,'Firmware mode was not restored after the failed buffer store')

    def test_last_rgb_snapshot_is_hidden_and_preserves_named_profiles(self):
        self.runcli('keyboard','profile','create','oyun','010203','40')
        before=self.get('/etc/slayerctl/rgb-profiles/oyun.conf')
        self.runcli('keyboard','profile','snapshot','__slayer_current')
        self.runcli('keyboard','color','abcdef')
        self.runcli('keyboard','profile','snapshot','__slayer_current')
        self.assertNotIn('__slayer_current',self.runcli('keyboard','profile','list'))
        self.assertEqual(before,self.get('/etc/slayerctl/rgb-profiles/oyun.conf'))
        self.runcli('keyboard','profile','snapshot','other',ok=False)

    def setup_profile_sync(self, hardware='low-power', desktop='performance'):
        self.put('/sys/devices/platform/slayerctl/thermal_mode_choices','low-power balanced performance')
        self.put('/sys/devices/platform/slayerctl/thermal_mode',hardware)
        self.put('/sys/devices/platform/slayerctl/profile_events','acpi-query+wmi')
        self.put('/run/slayerctl/test-desktop-profile',desktop)

    def test_profile_sync_is_bidirectional_and_off_is_independent(self):
        self.setup_profile_sync()
        self.runcli('profile','sync','on')
        self.assertEqual(self.get('/sys/devices/platform/slayerctl/thermal_mode'),'low-power')
        self.assertEqual(self.get('/run/slayerctl/test-desktop-profile'),'power-saver')
        self.put('/run/slayerctl/test-desktop-profile','performance')
        self.runcli('profile','sync','from-desktop')
        self.assertEqual(self.get('/sys/devices/platform/slayerctl/thermal_mode'),'performance')
        self.put('/sys/devices/platform/slayerctl/thermal_mode','low-power')
        self.runcli('profile','sync','from-hardware')
        self.assertEqual(self.get('/run/slayerctl/test-desktop-profile'),'power-saver')
        self.runcli('profile','sync','off')
        self.put('/run/slayerctl/test-desktop-profile','balanced')
        before=self.snap()
        self.runcli('profile','sync','from-desktop')
        self.assertEqual(before,self.snap())
        self.assertEqual(self.get('/sys/devices/platform/slayerctl/thermal_mode'),'low-power')

    def test_profile_sync_rejects_unavailable_and_unknown_without_writes(self):
        self.setup_profile_sync()
        self.put('/sys/devices/platform/slayerctl/profile_events','unavailable')
        before=self.snap()
        self.runcli('profile','sync','on',ok=False)
        self.assertEqual(before,self.snap())
        self.put('/sys/devices/platform/slayerctl/profile_events','acpi-query+wmi')
        self.put('/sys/devices/platform/slayerctl/thermal_mode','custom')
        before=self.snap()
        self.runcli('profile','sync','on',ok=False)
        self.assertEqual(before,self.snap())

    def test_profile_sync_boot_uses_hardware_and_avoids_repeated_writes(self):
        self.setup_profile_sync()
        self.runcli('config','set','profile','balanced','desktop_profile','balanced','performance_sync','on')
        self.runcli('apply-config')
        self.assertEqual(self.get('/sys/devices/platform/slayerctl/thermal_mode'),'low-power')
        self.assertEqual(self.get('/run/slayerctl/test-desktop-profile'),'power-saver')
        before=self.snap()
        stamp=(self.root/'run/slayerctl/test-desktop-profile').stat().st_mtime_ns
        self.runcli('profile','sync','from-hardware')
        self.assertEqual(before,self.snap())
        self.assertEqual(stamp,(self.root/'run/slayerctl/test-desktop-profile').stat().st_mtime_ns)

    def test_passive(self):
        old=self.snap()
        for c in [('status',),('bios',),('doctor',),('caps',),('usb','list'),('battery','status'),('gpu','status'),('sensors',),('ui-data',),('config','show')]:
            self.runcli(*c)
        self.assertEqual(old,self.snap())
        self.assertIn((Path(__file__).parent.parent/'VERSION').read_text().strip(),self.runcli('--version'))
        self.assertIn('N.1.46GAR16',self.runcli('bios'))
    def test_keyboard(self):
        self.runcli('keyboard','brightness','50')
        self.runcli('keyboard','color','ff8000')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_2/multi_intensity'),'255 128 0')
        self.runcli('keyboard','key','1','blue')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_1/multi_intensity'),'0 0 255')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_2/multi_intensity'),'255 128 0')
        self.assertEqual(self.get('/sys/bus/hid/devices/0003:048D:600B.0001/controls/buffer_input'),'0')
    def test_rejects_no_writes(self):
        old=self.snap()
        for c in [('gpu','mode','dedicated'),('fan','curve','100'),('cpu','power','100'),('usb','charging','on'),('keyboard','brightness','101'),('keyboard','brightness','-1'),('keyboard','brightness','1;touch /tmp/pwn'),('keyboard','color','gg0000'),('keyboard','key','999','ffffff'),('battery','limit','80','80'),('battery','limit','49'),('profile','set','unknown'),('usb','wake','../../etc/passwd','on'),('firmware','write')]:
            self.runcli(*c,ok=False)
            self.assertEqual(old,self.snap(),c)
    def test_battery(self):
        self.runcli('battery','limit','80','60')
        p='/sys/class/power_supply/BAT0/'
        self.assertEqual(self.get(p+'charge_control_end_threshold'),'80')
        self.assertEqual(self.get(p+'charge_control_start_threshold'),'60')
        self.runcli('battery','limit','100','90')
        old=self.snap()
        self.runcli('battery','limit','80',ok=False)
        self.assertEqual(old,self.snap())
        self.runcli('battery','limit','80','60')
    def test_usb_protection(self):
        old=self.snap()
        for port in ['1-1','1-2','1-3','1-5']:
            self.runcli('usb','powersave',port,'on',ok=False)
            self.assertEqual(old,self.snap())
        self.runcli('usb','powersave','1-4','on')
        self.assertEqual(self.get('/sys/bus/usb/devices/1-4/power/control'),'auto')
        self.runcli('usb','wake','1-6','off')
        self.assertEqual(self.get('/sys/bus/usb/devices/1-6/power/wakeup'),'disabled')
        self.put('/sys/bus/usb/devices/1-6:1.0/bInterfaceClass','')
        old=self.snap();self.runcli('usb','powersave','1-6','on',ok=False);self.assertEqual(old,self.snap())
    def test_dry_run(self):
        old=self.snap()
        for c in [('keyboard','color','ffffff'),('keyboard','brightness','100'),('battery','limit','80','60'),('profile','set','performance'),('usb','wake','1-4','off'),('keyboard','effect','wave','--duration','1'),('config','set','keyboard_color','ffffff')]:
            self.runcli('--dry-run',*c)
        self.assertEqual(old,self.snap())
    def test_config(self):
        self.runcli('config','set','keyboard_color','123456')
        self.runcli('config','set','battery_end','80')
        self.runcli('config','set','battery_start','60')
        self.runcli('apply-config')
        self.assertIn('battery_end=80',self.runcli('config','show'))
        self.runcli('config','set','battery_end','60','battery_start','50')
        self.assertIn('battery_end=60',self.runcli('config','show'))
        old=self.snap();self.runcli('config','set','exec','evil',ok=False);self.assertEqual(old,self.snap())
        self.runcli('config','clear')
        self.assertNotIn('keyboard_color=',self.runcli('config','show'))
    def test_symlink_escape(self):
        with tempfile.TemporaryDirectory() as outside:
            p=self.root/'sys/class/leds/rgb:kbd_backlight/brightness';p.unlink()
            target=Path(outside)/'victim';target.write_text('safe')
            p.symlink_to(target)
            self.runcli('keyboard','brightness','50',ok=False)
            self.assertEqual(target.read_text(),'safe')
    def test_effect_restores(self):
        old=self.snap()
        self.runcli('keyboard','effect','wave','--duration','1','--fps','2')
        self.assertEqual(old,self.snap())
        self.runcli('keyboard','effect','breathe','--duration','1','--fps','2')
        self.assertEqual(old,self.snap())
    def test_effect_signal(self):
        old=self.snap()
        p=subprocess.Popen([BIN,'--root',str(self.root),'keyboard','effect','wave','--duration','5','--fps','10'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        time.sleep(.25);p.send_signal(signal.SIGTERM);out,err=p.communicate(timeout=3)
        self.assertEqual(p.returncode,0,(out,err));self.assertEqual(old,self.snap())
    def test_absent_interfaces(self):
        (self.root/'sys/class/power_supply/BAT0/charge_control_end_threshold').unlink()
        (self.root/'sys/firmware/acpi/platform_profile').unlink()
        old=self.snap()
        error = self.runcli('battery','limit','80',ok=False)
        self.assertIn('%80 şarj sınırı uygulanmadı', error)
        self.runcli('profile','set','balanced',ok=False)
        self.assertEqual(old,self.snap())
    def test_helper_rejects_fixture(self):
        helper=self.root/'slayerctl-helper';helper.symlink_to(BIN)
        p=subprocess.run([str(helper),'--root',str(self.root),'keyboard','color','ffffff'],capture_output=True,text=True)
        self.assertNotEqual(p.returncode,0)
        self.assertIn('test kökü kabul etmez',p.stderr)

    def hardware_fixture(self):
        base='/sys/class/hidraw/hidraw1'
        self.put(base+'/device/uevent','HID_ID=0003:0000048D:0000600B')
        for n,v in {'bInterfaceNumber':'01','idVendor':'048d','idProduct':'600b','bcdDevice':'0003'}.items():self.put(base+'/device/'+n,v)
        self.put(base+'/test_report','untouched')
    def test_hardware_packet(self):
        self.hardware_fixture()
        self.runcli('keyboard','hardware','wave','--speed','3','--brightness','50','--direction','left')
        self.assertEqual(self.get('/sys/class/hidraw/hidraw1/test_report'),'0 8 2 3 3 25 8 2 0')
        self.runcli('keyboard','hardware','off')
        self.assertEqual(self.get('/sys/class/hidraw/hidraw1/test_report'),'0 8 1 0 0 0 0 0 0')
        for effect in ['breathe','rainbow','reactive','ripple','marquee','raindrop','aurora','spark']:
            self.runcli('keyboard','hardware',effect)
            self.assertEqual(self.get('/sys/class/hidraw/hidraw1/test_report').split()[-1],'0')
    def test_hardware_effect_prefers_native_driver_cache(self):
        self.hardware_fixture()
        path='/sys/class/hidraw/hidraw1/device/controls/hardware_effect'
        self.put(path,'51 5 0 0')
        self.runcli('keyboard','hardware','wave','--speed','3','--brightness','50','--direction','left')
        self.assertEqual(self.get(path),'3 3 25 2')
        self.assertEqual(self.get('/sys/class/hidraw/hidraw1/test_report'),'untouched')
        old=self.snap()
        self.runcli('--dry-run','keyboard','hardware','rainbow')
        self.assertEqual(old,self.snap())
        self.runcli('keyboard','hardware','off')
        self.assertEqual(self.get(path),'0 5 25 0')
    def test_screen_color_profile_is_validated_saved_and_restored(self):
        self.hardware_fixture()
        base='/sys/class/hidraw/hidraw1/device/controls/'
        self.put(base+'color_profile','off');self.put(base+'white_point','255 255 255')
        payload=bytearray(b'SC3D'+bytes([1,8,6,0]))
        for i in range(8):
            rgb=[255 if i&(1<<c) else 0 for c in range(3)]
            payload.extend(rgb+rgb)
        for cell in [(0,1,3,7),(0,1,5,7),(0,2,3,7),(0,2,6,7),(0,4,5,7),(0,4,6,7)]:payload.extend(cell)
        encoded=payload.hex()
        self.runcli('keyboard','color-profile','set','panel',encoded)
        self.assertEqual(self.get(base+'color_profile'),encoded)
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight/multi_intensity'),'10 20 30')
        old=self.snap()
        self.runcli('keyboard','color-profile','set','panel',encoded[:-2],ok=False)
        self.assertEqual(old,self.snap())
        self.runcli('--dry-run','keyboard','color-profile','off');self.assertEqual(old,self.snap())
        self.runcli('config','set','keyboard_color_profile','panel','lighting_owner','kde','brightness_owner','kde')
        self.put(base+'color_profile','off');self.runcli('apply-config')
        self.assertEqual(self.get(base+'color_profile'),encoded)
        self.runcli('keyboard','color-profile','off');self.assertEqual(self.get(base+'color_profile'),'off')

    def test_white_point_preserves_rgb_and_restores_with_kde_owner(self):
        self.hardware_fixture()
        path='/sys/class/hidraw/hidraw1/device/controls/white_point'
        self.put(path,'255 255 255')
        self.runcli('keyboard','white-point','160','255','255')
        self.assertEqual(self.get(path),'160 255 255')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight/multi_intensity'),'10 20 30')
        old=self.snap()
        for args in [('0','255','255'),('256','255','255'),('160','255')]:
            self.runcli('keyboard','white-point',*args,ok=False)
            self.assertEqual(old,self.snap())
        self.runcli('--dry-run','keyboard','white-point','255','255','255','--white')
        self.assertEqual(old,self.snap())
        self.runcli('keyboard','white-point','160','255','255','--white')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight/multi_intensity'),'255 255 255')
        self.runcli('config','set','keyboard_white_point','160,255,255','lighting_owner','kde','brightness_owner','kde')
        self.put(path,'255 255 255')
        self.runcli('apply-config')
        self.assertEqual(self.get(path),'160 255 255')
        self.runcli('config','set','keyboard_white_point','0,255,255',ok=False)

    def test_hardware_fail_closed(self):
        self.hardware_fixture()
        for key,value in [('bcdDevice','0002'),('bInterfaceNumber','00'),('idProduct','600a')]:
            original=self.get('/sys/class/hidraw/hidraw1/device/'+key)
            self.put('/sys/class/hidraw/hidraw1/device/'+key,value)
            old=self.snap();self.runcli('keyboard','hardware','wave',ok=False);self.assertEqual(old,self.snap())
            self.put('/sys/class/hidraw/hidraw1/device/'+key,original)
        old=self.snap()
        for args in [('wave','--speed','11'),('wave','--brightness','101'),('wave','--direction','garbage'),('rainbow','--direction','left'),('wave','--save','1'),('raw','08'),('wave','--speed')]:
            self.runcli('keyboard','hardware',*args,ok=False);self.assertEqual(old,self.snap())
        self.runcli('--dry-run','keyboard','hardware','wave');self.assertEqual(old,self.snap())
    def test_layout(self):
        old=self.snap();self.runcli('keyboard','layout','list');self.assertEqual(old,self.snap())
        self.runcli('keyboard','layout','set','W','1')
        self.assertIn('W\tW\t1\tcalibrated',self.runcli('keyboard','layout','list'))
        self.runcli('keyboard','key','W','ff0000')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_1/multi_intensity'),'255 0 0')
        old=self.snap()
        for args in [('set','W','126'),('set','../../etc/passwd','0'),('set','A','1')]:
            self.runcli('keyboard','layout',*args,ok=False);self.assertEqual(old,self.snap())
    def test_rgb_profiles(self):
        self.runcli('keyboard','layout','set','W','1')
        self.runcli('keyboard','profile','create','oyun','0000ff','40')
        self.runcli('keyboard','profile','set-key','oyun','W','ff0000')
        self.assertIn('oyun',self.runcli('keyboard','profile','list'))
        self.runcli('keyboard','profile','apply','oyun')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_1/multi_intensity'),'255 0 0')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_2/multi_intensity'),'0 0 255')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight/brightness'),'20')
        self.runcli('config','set','rgb_profile','oyun')
        self.runcli('apply-config')
        old=self.snap()
        for args in [('create','../evil'),('create','oyun'),('set-key','oyun','NoSuchKey','ff0000'),('set-key','oyun','A','xx0000')]:
            self.runcli('keyboard','profile',*args,ok=False);self.assertEqual(old,self.snap())
        self.runcli('keyboard','profile','set-key','oyun','Q','ffffff')
        old=self.snap();self.runcli('keyboard','profile','apply','oyun',ok=False);self.assertEqual(old,self.snap())
    def test_hardware_profile(self):
        self.hardware_fixture()
        self.runcli('keyboard','profile','effect','dalga','wave','4','60','up')
        self.runcli('config','set','rgb_profile','dalga')
        self.runcli('apply-config')
        self.assertEqual(self.get('/sys/class/hidraw/hidraw1/test_report'),'0 8 2 3 4 30 8 3 0')
        old=self.snap();self.runcli('keyboard','profile','set-key','dalga','W','ff0000',ok=False);self.assertEqual(old,self.snap())
    def test_identify_restores(self):
        old=self.snap();self.runcli('keyboard','identify','1');self.assertEqual(old,self.snap())
        p=subprocess.Popen([BIN,'--root',str(self.root),'keyboard','identify','1'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        time.sleep(.25);p.send_signal(signal.SIGTERM);out,err=p.communicate(timeout=3)
        self.assertEqual(p.returncode,0,(out,err));self.assertEqual(old,self.snap())

    def test_profile_capture_and_edit(self):
        self.runcli('keyboard','profile','capture','snapshot')
        self.assertIn('led.1=0a141e',self.runcli('keyboard','profile','show','snapshot'))
        self.runcli('keyboard','color','ff0000')
        self.runcli('keyboard','profile','apply','snapshot')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight_1/multi_intensity'),'10 20 30')
        self.runcli('keyboard','profile','create','edit','101010','50')
        self.runcli('keyboard','profile','base','edit','202020','70')
        self.assertIn('brightness=70',self.runcli('keyboard','profile','show','edit'))
        self.runcli('keyboard','profile','set-group','edit','wasd','ff0000')
        text=self.runcli('keyboard','profile','show','edit')
        for key in ['W','A','S','D']:self.assertIn('key.'+key+'=ff0000',text)
        self.runcli('config','set','rgb_profile','snapshot')
        self.runcli('config','unset','rgb_profile')
        self.assertNotIn('rgb_profile=',self.runcli('config','show'))
    def test_broken_profile_no_writes(self):
        for text in ['type=hardware\neffect=raw','type=static\nkey.W=gg0000','type=static\nled.126=ffffff','type=static\nbrightness=101','type=static\nbackground=ffffff\nbackground=000000']:
            self.put('/etc/slayerctl/rgb-profiles/broken.conf',text)
            old=self.snap();self.runcli('keyboard','profile','apply','broken',ok=False);self.assertEqual(old,self.snap())

    def test_standard_charging_profile(self):
        p='/sys/class/power_supply/BAT0/charge_types'
        self.put(p,'[Standard] Trickle Long_Life')
        old=self.snap()
        self.runcli('battery','profile','unknown',ok=False);self.assertEqual(old,self.snap())
        self.runcli('battery','profile','stationary')
        self.assertEqual(self.get(p),'Trickle')
        self.put(p,'Standard Trickle [Long_Life]')
        self.runcli('battery','profile','high-capacity')
        self.assertEqual(self.get(p),'Standard')
        self.put(p,'[Standard] Trickle Long_Life')
        self.runcli('battery','profile','balanced')
        self.assertEqual(self.get(p),'Long_Life')
        self.put(p,'[Standard]')
        old=self.snap();self.runcli('battery','profile','balanced',ok=False);self.assertEqual(old,self.snap())

    def cpu_fixture(self):
        base='/sys/devices/system/cpu/cpufreq'
        self.put(base+'/boost','1')
        for index in [0,1]:
            for n,v in {'scaling_driver':'amd-pstate-epp','cpuinfo_min_freq':'400000','cpuinfo_max_freq':'5000000','scaling_min_freq':'1000000','scaling_max_freq':'4000000','boost':'1'}.items():
                self.put(f'{base}/policy{index}/{n}',v)
        return base
    def gpu_fixture(self):
        p='/sys/bus/pci/devices/0000:01:00.0'
        for n,v in {'vendor':'0x10de','device':'0x2d19','subsystem_vendor':'0x1d05','subsystem_device':'0x605c'}.items():self.put(p+'/'+n,v)
        base='/sys/devices/platform/slayerctl/test-gpu'
        self.put(base+'/limits','5.00,115.00,50.00,50.00')
        self.put(base+'/power_limit','50.000')
        return base
    def test_cpu_controls(self):
        base=self.cpu_fixture()
        old=self.snap();self.runcli('cpu','status');self.assertEqual(old,self.snap())
        self.runcli('cpu','boost','off')
        for p in ['/boost','/policy0/boost','/policy1/boost']:self.assertEqual(self.get(base+p),'0')
        self.runcli('cpu','boost','on')
        self.runcli('cpu','limits','4500','4800')
        for index in [0,1]:
            self.assertEqual(self.get(f'{base}/policy{index}/scaling_min_freq'),'4500000')
            self.assertEqual(self.get(f'{base}/policy{index}/scaling_max_freq'),'4800000')
        self.runcli('cpu','limits','500','2000')
        self.runcli('cpu','limits','reset')
        self.assertEqual(self.get(base+'/policy1/scaling_max_freq'),'5000000')
        old=self.snap()
        for args in [('limits','300','2000'),('limits','1000','6000'),('limits','3000','2000'),('boost','yes'),('power','50')]:
            self.runcli('cpu',*args,ok=False);self.assertEqual(old,self.snap())
        self.runcli('--dry-run','cpu','boost','off');self.assertEqual(old,self.snap())
        self.runcli('--dry-run','cpu','limits','500','2000');self.assertEqual(old,self.snap())
    def test_cpu_validate_all_before_writes(self):
        base=self.cpu_fixture();self.put(base+'/policy1/cpuinfo_max_freq','4100000')
        old=self.snap();self.runcli('cpu','limits','1000','4500',ok=False);self.assertEqual(old,self.snap())
    def test_cpu_rollback(self):
        self.cpu_fixture()
        for command in [('cpu','limits','4500','4800'),('cpu','limits','500','2000'),('cpu','boost','off')]:
            for failure in [1,2,3]:
                self.put('/.slayerctl-fail-write',str(failure));old=self.snap()
                self.runcli(*command,ok=False);self.assertEqual(old,self.snap(),(command,failure))
    def test_battery_rollback(self):
        for failure in [1,2]:
            self.put('/.slayerctl-fail-write',str(failure));old=self.snap()
            self.runcli('battery','limit','80','60',ok=False);self.assertEqual(old,self.snap())
    def test_keyboard_rollback(self):
        for failure in [1,2,3]:
            self.put('/.slayerctl-fail-write',str(failure));old=self.snap()
            self.runcli('keyboard','color','ffffff',ok=False);self.assertEqual(old,self.snap())
        for failure in [1,2,3]:
            self.put('/.slayerctl-fail-write',str(failure));old=self.snap()
            self.runcli('keyboard','brightness','80',ok=False);self.assertEqual(old,self.snap())
    def test_gpu_limits_and_rollback(self):
        base=self.gpu_fixture();old=self.snap()
        self.assertIn('power_settable: candidate',self.runcli('gpu','limits'));self.assertEqual(old,self.snap())
        for value in ['4','116','-1','50.5','NaN']:
            self.runcli('gpu','power-limit',value,ok=False);self.assertEqual(old,self.snap())
        self.runcli('--dry-run','gpu','power-limit','65');self.assertEqual(old,self.snap())
        self.runcli('gpu','power-limit','65');self.assertEqual(self.get(base+'/power_limit'),'65.000')
        self.runcli('gpu','power-limit','reset');self.assertEqual(self.get(base+'/power_limit'),'50.000')
        self.put('/.slayerctl-fail-write','1');old=self.snap()
        self.runcli('gpu','power-limit','65',ok=False);self.assertEqual(old,self.snap())
    def test_gpu_unavailable_current_blocks_write(self):
        base=self.gpu_fixture();self.put(base+'/power_limit','[N/A]');old=self.snap()
        self.assertIn('power_settable: no',self.runcli('gpu','limits'))
        self.runcli('gpu','power-limit','60',ok=False);self.assertEqual(old,self.snap())
        self.runcli('--dry-run','gpu','power-limit','60',ok=False);self.assertEqual(old,self.snap())
    def test_gpu_identity_gate(self):
        self.gpu_fixture();self.put('/sys/bus/pci/devices/0000:01:00.0/subsystem_device','0x1234');old=self.snap()
        self.runcli('gpu','power-limit','60',ok=False);self.assertEqual(old,self.snap())
    def test_fan_curve_offline_only(self):
        curve='40:30,60:60,90:100'
        self.runcli('fan','curve','save','sessiz',curve,curve)
        self.assertIn('sessiz',self.runcli('fan','curve','list'))
        self.assertIn('cpu='+curve,self.runcli('fan','curve','show','sessiz'))
        old=self.snap();self.runcli('fan','curve','apply','sessiz',ok=False);self.assertEqual(old,self.snap())
        for value in ['40:30,60:60,90:90','40:30,60:20,90:100','40:30,40:60,90:100','40:30,60:60,90:100,','40:0,60:60,90:100','40:30,60:60']:
            self.runcli('fan','curve','save','bad',value,curve,ok=False);self.assertEqual(old,self.snap())
    def test_ac_dc_event_cache_and_pd(self):
        self.put('/sys/class/power_supply/AC/type','Mains');self.put('/sys/class/power_supply/AC/online','1')
        self.runcli('keyboard','profile','create','ac','ff0000','50')
        self.runcli('keyboard','profile','create','dc','0000ff','30')
        self.runcli('config','set','rgb_profile_ac','ac','rgb_profile_battery','dc')
        self.runcli('power-event')
        p='/sys/class/leds/rgb:kbd_backlight/multi_intensity'
        self.assertEqual(self.get(p),'255 0 0')
        self.assertTrue(self.get('/run/slayerctl/power-state').startswith('ac:'))
        self.runcli('keyboard','color','00ff00')
        old=self.snap();self.runcli('power-event');self.assertEqual(old,self.snap())
        self.put('/sys/class/power_supply/AC/online','0');self.runcli('power-event');self.assertEqual(self.get(p),'0 0 255')
        self.put('/sys/class/power_supply/PD/type','USB_PD');self.put('/sys/class/power_supply/PD/online','1')
        self.runcli('power-event');self.assertEqual(self.get(p),'255 0 0')
        self.runcli('config','set','rgb_profile_ac','dc')
        self.assertFalse((self.root/'run/slayerctl/power-state').exists())
        self.runcli('power-event');self.assertEqual(self.get(p),'0 0 255')
    def test_ac_dc_failures_are_not_cached(self):
        self.runcli('keyboard','profile','create','ac','ff0000','50')
        self.runcli('config','set','rgb_profile_ac','ac')
        old=self.snap();self.runcli('power-event',ok=False);self.assertEqual(old,self.snap())
        self.put('/sys/class/power_supply/AC/type','Mains');self.put('/sys/class/power_supply/AC/online','unknown')
        old=self.snap();self.runcli('power-event',ok=False);self.assertEqual(old,self.snap())
        self.put('/sys/class/power_supply/AC/online','1')
        self.runcli('config','set','rgb_profile_ac','absent')
        self.runcli('power-event',ok=False);self.assertFalse((self.root/'run/slayerctl/power-state').exists())
    def test_config_schema_and_atomic_replacement(self):
        old=self.snap()
        for args in [('cpu_min_mhz','500'),('cpu_min_mhz','3000','cpu_max_mhz','2000'),('cpu_boost','yes'),('battery_profile','trickle'),('desktop_profile_ac','turbo')]:
            self.runcli('config','set',*args,ok=False);self.assertEqual(old,self.snap())
        self.runcli('config','set','cpu_min_mhz','500','cpu_max_mhz','2500','cpu_boost','off','battery_profile','balanced')
        self.assertIn('cpu_boost=off',self.runcli('config','show'))
        self.assertEqual(list((self.root/'etc/slayerctl').glob('*.??????')),[])
        old=self.snap();self.runcli('apply-config','unexpected',ok=False);self.assertEqual(old,self.snap())
        self.put('/etc/slayerctl/slayerctl.conf','cpu_boost=on\ncpu_boost=off')
        old=self.snap();self.runcli('apply-config',ok=False);self.assertEqual(old,self.snap())
    def test_semantic_settings(self):
        for name,attr in [('fn-lock','fn_lock'),('win-lock','win_lock'),('usb-charging','usb_sleep_charging'),('ac-recovery','ac_auto_boot')]:
            old=self.snap();self.runcli('settings',name,'on',ok=False);self.assertEqual(old,self.snap())
            path='/sys/devices/platform/slayerctl/'+attr;self.put(path,'0')
            self.assertIn(': 0',self.runcli('settings',name,'status'))
            self.runcli('settings',name,'on');self.assertEqual(self.get(path),'1')
            old=self.snap();self.runcli('--dry-run','settings',name,'off');self.assertEqual(old,self.snap())
            self.runcli('settings',name,'off');self.assertEqual(self.get(path),'0')
    def test_settings_rollback(self):
        self.put('/sys/devices/platform/slayerctl/fn_lock','0');self.put('/.slayerctl-fail-write','1')
        old=self.snap();self.runcli('settings','fn-lock','on',ok=False);self.assertEqual(old,self.snap())
    def test_exact_bios_gate_blocks_every_transport(self):
        self.hardware_fixture();self.cpu_fixture();self.gpu_fixture()
        self.put('/sys/devices/platform/slayerctl/fn_lock','0')
        self.put('/sys/devices/platform/slayerctl/fan_mode','auto')
        self.put('/sys/devices/platform/slayerctl/fan_modes_available','auto boost')
        self.runcli('keyboard','profile','create','safe','ffffff','50')
        self.runcli('config','set','keyboard_color','ffffff')
        commands=[('keyboard','brightness','50'),('keyboard','color','ffffff'),('keyboard','key','1','ffffff'),('keyboard','hardware','wave'),('keyboard','identify','1'),('keyboard','effect','wave','--duration','1'),('keyboard','reset'),('keyboard','profile','apply','safe'),('battery','limit','80'),('battery','profile','balanced'),('cpu','boost','on'),('cpu','limits','1000','4000'),('gpu','power-limit','65'),('settings','fn-lock','on'),('usb','wake','1-4','off'),('profile','set','balanced'),('fan','boost'),('apply-config',)]
        for bios in ['N.1.47GAR17','N.1.46GAR16-test','']:
            self.put('/sys/class/dmi/id/bios_version',bios);old=self.snap()
            for command in commands:
                output=self.runcli(*command,ok=False);self.assertIn('BIOS uyumsuz',output,command);self.assertEqual(old,self.snap(),command)
                self.runcli('--dry-run',*command,ok=False);self.assertEqual(old,self.snap(),command)
            self.assertIn('hardware_writes: blocked',self.runcli('status'))
            self.assertIn('model-bios [blocked]',self.runcli('caps'))
            self.runcli('bios');self.assertEqual(old,self.snap())
        self.put('/sys/class/dmi/id/bios_version','N.1.46GAR16')
        for field in ['product_sku','product_name','sys_vendor']:
            path='/sys/class/dmi/id/'+field;value=self.get(path);self.put(path,'OTHER');old=self.snap()
            self.runcli('keyboard','hardware','wave',ok=False);self.runcli('cpu','boost','on',ok=False);self.assertEqual(old,self.snap())
            self.put(path,value)
        self.assertIn('hardware_writes: allowed',self.runcli('status'))
    def test_smt_without_cpufreq_and_locked_states(self):
        path='/sys/devices/system/cpu/smt/control'
        self.put(path,'on');self.put('/sys/devices/system/cpu/smt/active','1')
        self.put('/sys/devices/system/cpu/online','0-31')
        old=self.snap();self.runcli('cpu','smt','status');self.assertEqual(old,self.snap())
        self.runcli('cpu','smt','off');self.assertEqual(self.get(path),'off')
        self.runcli('cpu','smt','on');self.assertEqual(self.get(path),'on')
        old=self.snap();self.runcli('--dry-run','cpu','smt','off');self.assertEqual(old,self.snap())
        for value in ['forceoff','notsupported','notimplemented','unexpected']:
            self.put(path,value);old=self.snap()
            self.runcli('cpu','smt','on',ok=False);self.assertEqual(old,self.snap())
        self.put(path,'on');self.put('/.slayerctl-fail-write','1');old=self.snap()
        self.runcli('cpu','smt','off',ok=False);self.assertEqual(old,self.snap())
        self.put('/sys/class/dmi/id/bios_version','N.1.47GAR17');old=self.snap()
        self.runcli('cpu','smt','off',ok=False);self.assertEqual(old,self.snap())

    def camera_fixture(self):
        base='/sys/bus/usb/devices/5-1.4'
        for name,value in {'idVendor':'2b7e','idProduct':'c906','bcdDevice':'0004','removable':'fixed','authorized':'1'}.items():self.put(base+'/'+name,value)
        return base
    def test_camera_privacy_only_internal_identity(self):
        base=self.camera_fixture();old=self.snap()
        self.runcli('settings','camera','status');self.assertEqual(old,self.snap())
        self.runcli('settings','camera','off');self.assertEqual(self.get(base+'/authorized'),'0')
        self.runcli('settings','camera','on');self.assertEqual(self.get(base+'/authorized'),'1')
        for name,value in [('idVendor','0000'),('idProduct','c907'),('bcdDevice','0005'),('removable','removable')]:
            original=self.get(base+'/'+name);self.put(base+'/'+name,value);old=self.snap()
            self.runcli('settings','camera','off',ok=False);self.assertEqual(old,self.snap())
            self.put(base+'/'+name,original)
        self.put('/.slayerctl-fail-write','1');old=self.snap()
        self.runcli('settings','camera','off',ok=False);self.assertEqual(old,self.snap())
    def test_camera_ambiguous_and_bios_mismatch(self):
        base=self.camera_fixture()
        for name in ['idVendor','idProduct','bcdDevice','removable','authorized']:
            self.put('/sys/bus/usb/devices/5-1.3/'+name,self.get(base+'/'+name))
        old=self.snap();self.runcli('settings','camera','off',ok=False);self.assertEqual(old,self.snap())
        self.put('/sys/class/dmi/id/bios_version','OTHER');old=self.snap()
        self.runcli('settings','camera','off',ok=False);self.assertEqual(old,self.snap())
    def test_mux_semantics_and_rollback(self):
        base='/sys/devices/platform/slayerctl'
        self.put(base+'/mux_modes_available','hybrid dedicated integrated');self.put(base+'/mux_mode','hybrid')
        old=self.snap();self.runcli('gpu','status');self.assertEqual(old,self.snap())
        self.runcli('--dry-run','gpu','mode','integrated');self.assertEqual(old,self.snap())
        text=self.runcli('gpu','mode','integrated');self.assertIn('Yeniden başlatma gerekir',text)
        self.assertEqual(self.get(base+'/mux_mode'),'integrated')
        self.runcli('gpu','mode','dedicated');self.assertEqual(self.get(base+'/mux_mode'),'dedicated')
        self.runcli('gpu','mode','hybrid');self.assertEqual(self.get(base+'/mux_mode'),'hybrid')
        old=self.snap();self.runcli('gpu','mode','raw',ok=False);self.assertEqual(old,self.snap())
        self.put('/.slayerctl-fail-write','1');old=self.snap()
        self.runcli('gpu','mode','integrated',ok=False);self.assertEqual(old,self.snap())
        self.put('/sys/class/dmi/id/bios_version','OTHER');old=self.snap()
        self.runcli('gpu','mode','integrated',ok=False);self.assertEqual(old,self.snap())
    def test_fan_profile_rollback_and_unsupported_usb_charge(self):
        base='/sys/devices/platform/slayerctl'
        self.put(base+'/fan_mode','auto');self.put(base+'/fan_modes_available','auto boost')
        self.runcli('fan','boost');self.assertEqual(self.get(base+'/fan_mode'),'boost')
        self.runcli('fan','auto');self.assertEqual(self.get(base+'/fan_mode'),'auto')
        self.put(base+'/usb_sleep_charging_supported','0');old=self.snap()
        self.assertIn('desteği bildirmiyor',self.runcli('settings','usb-charging','on',ok=False));self.assertEqual(old,self.snap())
        self.put('/.slayerctl-fail-write','1');old=self.snap()
        self.runcli('fan','boost',ok=False);self.assertEqual(old,self.snap())
        self.runcli('profile','set','performance',ok=False);self.assertEqual(old,self.snap())
    def test_new_persistence_and_no_mux_replay(self):
        base='/sys/devices/platform/slayerctl';self.put(base+'/fn_lock','0')
        self.put(base+'/fan_mode','auto');self.put(base+'/fan_modes_available','auto boost')
        self.put('/sys/devices/system/cpu/smt/control','on');cam=self.camera_fixture()
        self.runcli('config','set','fn_lock','on','cpu_smt','off','fan_mode','boost','camera','off')
        self.runcli('apply-config')
        for path,value in [(base+'/fn_lock','1'),(base+'/fan_mode','boost'),('/sys/devices/system/cpu/smt/control','off'),(cam+'/authorized','0')]:self.assertEqual(self.get(path),value)
        old=self.snap()
        for key in ['mux_mode','cpu_ccd']:
            self.runcli('config','set',key,'integrated',ok=False);self.assertEqual(old,self.snap())
    def test_cpu_topology_and_no_ccd_writes(self):
        for cpu in [0,1,16,17]:
            base=f'/sys/devices/system/cpu/cpu{cpu}'
            for name,value in {'topology/core_id':str(cpu%16),'topology/physical_package_id':'0','cache/index3/shared_cpu_list':'0-1,16-17','online':'1'}.items():self.put(base+'/'+name,value)
        old=self.snap();text=self.runcli('cpu','topology')
        self.assertIn('Fiziksel çekirdek: 2',text);self.assertIn('Çevrimiçi iş parçacığı: 4',text)
        self.runcli('cpu','ccd','status');self.assertEqual(old,self.snap())
        self.runcli('cpu','ccd','off',ok=False);self.assertEqual(old,self.snap())

    def test_adapter_event_preserves_baseline_hardware_choices(self):
        base='/sys/devices/platform/slayerctl'
        self.put(base+'/fn_lock','0');self.put(base+'/fan_mode','auto');self.put(base+'/fan_modes_available','auto boost')
        self.put('/sys/devices/system/cpu/smt/control','on');cam=self.camera_fixture()
        self.put('/sys/class/power_supply/AC/type','Mains');self.put('/sys/class/power_supply/AC/online','1')
        self.runcli('keyboard','profile','create','adapter','ff0000','50')
        self.runcli('config','set','fn_lock','on','camera','off','cpu_smt','off','fan_mode','boost')
        self.runcli('config','set','profile','performance','battery_end','80','keyboard_color','00ff00','rgb_profile_ac','adapter')
        paths=[base+'/fn_lock',base+'/fan_mode','/sys/devices/system/cpu/smt/control',cam+'/authorized','/sys/firmware/acpi/platform_profile','/sys/class/power_supply/BAT0/charge_control_end_threshold']
        old={p:self.get(p) for p in paths}
        self.runcli('power-event')
        self.assertEqual(old,{p:self.get(p) for p in paths})
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight/multi_intensity'),'255 0 0')
        self.runcli('keyboard','color','0000ff');self.put('/sys/class/power_supply/AC/online','0')
        self.runcli('power-event')
        self.assertEqual(self.get('/sys/class/leds/rgb:kbd_backlight/multi_intensity'),'0 0 255')
        self.assertEqual(old,{p:self.get(p) for p in paths})
    def test_rgb_failure_after_store_restores_buffer_and_state(self):
        for command in [('keyboard','color','ffffff'),('keyboard','reset'),('keyboard','identify','1'),('keyboard','effect','wave','--duration','1','--fps','2')]:
            for failure in [1,2,3,4,5]:
                self.put('/.slayerctl-fail-write-after',str(failure));old=self.snap()
                self.runcli(*command,ok=False)
                self.assertEqual(old,self.snap(),(command,failure))
    def test_invalid_saved_rgb_never_opens_batch(self):
        self.put('/sys/class/leds/rgb:kbd_backlight_1/multi_intensity','not rgb')
        old=self.snap();self.runcli('keyboard','color','ffffff',ok=False);self.assertEqual(old,self.snap())
        self.runcli('keyboard','effect','wave','--duration','1',ok=False);self.assertEqual(old,self.snap())
    def test_post_store_cpu_and_usb_rollback(self):
        self.cpu_fixture()
        commands=[('cpu','limits','4500','4800'),('cpu','limits','500','2000'),('cpu','limits','500','800'),('cpu','boost','off')]
        for command in commands:
            for failure in [1,2,3]:
                self.put('/.slayerctl-fail-write-after',str(failure));old=self.snap()
                self.runcli(*command,ok=False);self.assertEqual(old,self.snap(),(command,failure))
        self.put('/.slayerctl-fail-write-after','1')
        for command in [('usb','wake','1-4','off'),('usb','powersave','1-4','on')]:
            old=self.snap();self.runcli(*command,ok=False);self.assertEqual(old,self.snap())
    def test_ambiguous_camera_does_not_disable_other_information(self):
        base=self.camera_fixture()
        for name in ['idVendor','idProduct','bcdDevice','removable','authorized']:
            self.put('/sys/bus/usb/devices/5-1.3/'+name,self.get(base+'/'+name))
        old=self.snap()
        self.assertIn('camera: ambiguous',self.runcli('status'))
        self.assertIn('camera [blocked]',self.runcli('caps'))
        self.runcli('ui-data');self.runcli('bios');self.assertEqual(old,self.snap())
    def test_corrupt_layout_can_be_reset_without_hardware_writes(self):
        self.put('/etc/slayerctl/keyboard-layout.conf','1=W\n2=W')
        old=self.snap();self.runcli('keyboard','layout','list',ok=False);self.assertEqual(old,self.snap())
        self.runcli('keyboard','layout','reset')
        self.runcli('keyboard','layout','list')
        for name,value in old.items():
            if name!='etc/slayerctl/keyboard-layout.conf':self.assertEqual((self.root/name).read_bytes(),value)
    def test_information_commands_reject_extra_arguments(self):
        old=self.snap()
        for args in [('keyboard','status','extra'),('fan','status','extra'),('gpu','status','extra'),('profile','status','extra'),('usb','list','extra')]:
            self.runcli(*args,ok=False);self.assertEqual(old,self.snap())
    def test_usb_product_control_characters_are_not_printed(self):
        self.put('/sys/bus/usb/devices/1-4/product','Camera\x1b[31m\x7f')
        out=self.runcli('usb','list');self.assertNotIn('\x1b',out);self.assertNotIn('\x7f',out)

    def test_ui_colors_follow_recorded_led_mapping(self):
        self.runcli('keyboard','layout','set','W','1')
        self.runcli('keyboard','key','W','ff0000')
        old=self.snap();out=self.runcli('ui-data');self.assertEqual(old,self.snap())
        self.assertIn('key\tW\tW\t2\t3\t1\t1\t1',out)
        self.assertIn('key-rgb\t1\t255\t0\t0',out)
        self.put('/sys/class/leds/rgb:kbd_backlight_1/multi_intensity','broken')
        old=self.snap();out=self.runcli('ui-data');self.assertEqual(old,self.snap())
        self.assertNotIn('key-rgb\t1\t',out)
    def test_usb_summary_separates_ports_from_devices(self):
        self.put('/sys/bus/usb/devices/usb1/idVendor','1d6b')
        self.put('/sys/bus/usb/devices/usb1/bDeviceClass','09')
        old=self.snap();out=self.runcli('ui-data');self.assertEqual(old,self.snap())
        self.assertIn('value\tusb_connected_devices\t5',out)
        self.assertIn('value\tusb_connected_hubs\t1',out)
        self.assertIn('value\tusb_external_a\t3',out)
        self.assertIn('value\tusb_external_c\t2',out)
        self.put('/sys/class/dmi/id/product_sku','OTHER')
        self.assertNotIn('value\tusb_external_',self.runcli('ui-data'))

    def test_desktop_calls_are_isolated(self):
        old=self.snap()
        for args in [('power','status'),('power','set','performance'),('service','status'),('gui',)]:
            self.runcli(*args,ok=False);self.assertEqual(old,self.snap())

unittest.main(verbosity=2)
