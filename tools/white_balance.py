#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Interactive R9T white-point measurement. GUI stays unprivileged.

The short-lived privileged child accepts only bounded RGB triplets. It uses
native LED batching, restores on cancel/EOF, and exits after confirmation.
The measurement is saved to the user's runtime directory, not persistent config.
"""
import fcntl
import json
import os
from pathlib import Path
import sys
import subprocess
import time


def triplet(value):
    if not isinstance(value, list) or len(value) != 3 or any(type(c) is not int or not 0 <= c <= 255 for c in value):
        raise ValueError('RGB değerleri 0–255 aralığında üç tam sayı olmalı.')
    return value


def backend():
    if os.geteuid() != 0:
        raise PermissionError('LED yardımcısı yönetici yetkisi gerektirir.')
    for name, expected in [('sys_vendor','GAME GARAJ'),('product_name','SLAYER R9T'),('product_sku','R9T V1'),('bios_version','N.1.46GAR16')]:
        if Path('/sys/class/dmi/id', name).read_text().strip() != expected:
            raise ValueError('Bu cihaz için R9T ışık sözleşmesi doğrulanamadı.')
    controls = next(Path('/sys/bus/hid/devices').glob('*048D:600B.*/controls'))
    hdev = controls.parent
    interface = hdev.resolve().parent
    assert hdev.joinpath('driver').resolve().name == 'ite_8291'
    assert interface.joinpath('bInterfaceNumber').read_text().strip() == '01'
    assert interface.parent.joinpath('bcdDevice').read_text().strip() == '0003'
    leds = sorted(Path('/sys/class/leds').glob('rgb:kbd_backlight*'))
    if len(leds) != 126 or not controls.joinpath('frame_color').exists():
        raise ValueError('Güncel R9T LED sürücüsü gerekli.')
    for led in leds:
        assert led.joinpath('multi_index').read_text().split() == ['red','green','blue']
    saved = [(p, p.joinpath('multi_intensity').read_text()) for p in leds]
    brightness = Path('/sys/class/leds/rgb:kbd_backlight/brightness')
    old_brightness = brightness.read_text()
    old_effect = controls.joinpath('hardware_effect').read_text().split()
    point_file = controls/'white_point'
    old_point = point_file.read_text().split() if point_file.exists() else None
    profile_file=controls/'color_profile'
    old_profile=profile_file.read_text().strip() if profile_file.exists() else None
    maximum = int(Path('/sys/class/leds/rgb:kbd_backlight/max_brightness').read_text())
    assert maximum == 50
    lock = os.open('/run/lock/slayerctl-rgb.lock',os.O_WRONLY|os.O_CREAT|os.O_NOFOLLOW|os.O_CLOEXEC,0o600)
    committed = False
    current = None

    def emit(data):
        print(json.dumps(data), flush=True)

    def frame(values, level, point=None, profile=None):
        deadline = time.monotonic() + 2
        while True:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() > deadline: raise TimeoutError('RGB arayüzü meşgul.')
                time.sleep(.01)
        try:
            if controls.joinpath('buffer_input').read_text().strip() != '0':
                raise RuntimeError('Başka bir RGB işlemi devam ediyor.')
            controls.joinpath('buffer_input').write_text('1\n')
            try:
                if point is not None and point_file.exists():
                    point_file.write_text(' '.join(map(str,point))+'\n')
                if profile is not None and profile_file.exists():profile_file.write_text(profile+'\n')
                for p, color in values:
                    p.joinpath('multi_intensity').write_text(color)
                brightness.write_text(str(level) + '\n')
            finally:
                controls.joinpath('buffer_input').write_text('0\n')
        finally:
            fcntl.flock(lock, fcntl.LOCK_UN)

    try:
        emit({'ready':True,'brightness':maximum})
        for raw in sys.stdin:
            if len(raw) > 256: raise ValueError('Geçersiz ileti.')
            command = json.loads(raw)
            if not isinstance(command, dict): raise ValueError('Geçersiz ileti.')
            if set(command) == {'color'}:
                rgb = triplet(command['color'])
                color = ' '.join(map(str,rgb)) + '\n'
                frame([(p,color) for p in leds], maximum, [255,255,255] if current is None and old_point else None)
                current = rgb
                emit({'applied':rgb})
            elif set(command) == {'commit'}:
                rgb = triplet(command['commit'])
                if rgb != current or min(rgb) <= 0: raise ValueError('Önce geçerli beyazı uygulayın.')
                if point_file.exists():
                    frame(saved, int(old_brightness), rgb)
                    if old_effect[0] != '51': controls.joinpath('hardware_effect').write_text(' '.join(old_effect)+'\n')
                committed = True
                emit({'confirmed':rgb,'brightness':maximum})
                break
            elif command == {'restore':True}:
                break
            else: raise ValueError('Geçersiz ışık işlemi.')
    finally:
        if not committed:
            frame(saved, int(old_brightness), old_point, old_profile)
            if old_effect[0] != '51':
                controls.joinpath('hardware_effect').write_text(' '.join(old_effect)+'\n')
            emit({'restored':True})
        os.close(lock)


def gui():
    from PySide6.QtCore import Qt, QProcess, QTimer
    from PySide6.QtGui import QCloseEvent
    from PySide6.QtWidgets import QApplication,QWidget,QVBoxLayout,QGridLayout,QLabel,QSlider,QSpinBox,QPushButton,QHBoxLayout,QMessageBox
    app = QApplication(sys.argv)
    app.setApplicationName('Slayer · Beyaz dengesi')
    result = Path(os.environ.get('XDG_RUNTIME_DIR',f'/run/user/{os.getuid()}'))/'slayerctl-white-point.json'

    class Window(QWidget):
        def __init__(self):
            super().__init__()
            self.setWindowTitle('Slayer · Klavye beyaz dengesi')
            self.resize(580,400)
            self.sliders=[]
            self.ready=False; self.busy=False; self.confirmed=False; self.pending=None; self.applied=None
            self.read_buffer=b''
            self.was_kameleon=False
            try:
                self.was_kameleon=subprocess.check_output(['/usr/bin/slayerctl','keyboard','accent','status'],text=True,timeout=2).strip()=='true'
                if self.was_kameleon:subprocess.run(['/usr/bin/slayerctl','keyboard','accent','off'],check=True,timeout=2)
            except (OSError,subprocess.SubprocessError): pass
            try: initial=triplet(json.loads(result.read_text())['white_point'])
            except (OSError,ValueError,KeyError): initial=[255,255,255]
            layout=QVBoxLayout(self);layout.setSpacing(16)
            title=QLabel('Klavyede nötr beyazı bul');font=title.font();font.setPointSizeF(font.pointSizeF()*1.35);font.setBold(True);title.setFont(font);layout.addWidget(title)
            hint=QLabel('Kırmızı, yeşil ve mavi ışığın düzeyini ayarla. Pembeyi azaltmak için önce kırmızıyı azaltabilirsin. Klavyeye bakarak nötr beyazı bulduğunda “Beyaz bu” düğmesine bas.');hint.setWordWrap(True);layout.addWidget(hint)
            grid=QGridLayout();layout.addLayout(grid)
            for row,name in enumerate(['Kırmızı · R','Yeşil · G','Mavi · B']):
                label=QLabel(name);grid.addWidget(label,row,0)
                slider=QSlider(Qt.Orientation.Horizontal);slider.setRange(0,255);slider.setValue(initial[row]);slider.setAccessibleName(name);grid.addWidget(slider,row,1)
                spin=QSpinBox();spin.setRange(0,255);spin.setValue(initial[row]);spin.setAccessibleName(name+' değeri');grid.addWidget(spin,row,2)
                slider.valueChanged.connect(spin.setValue);spin.valueChanged.connect(slider.setValue)
                slider.valueChanged.connect(self.changed);self.sliders.append(slider)
            self.status=QLabel('Anlık LED yardımcısı hazırlanıyor. Gerekirse KDE penceresinde yönetici doğrulamasını tamamla.');self.status.setWordWrap(True);layout.addWidget(self.status)
            self.values=QLabel('R 255 · G 255 · B 255');layout.addWidget(self.values)
            layout.addStretch()
            buttons=QHBoxLayout();layout.addLayout(buttons)
            reset=QPushButton('Sıfırla');reset.clicked.connect(lambda:[s.setValue(255) for s in self.sliders]);buttons.addWidget(reset)
            cancel=QPushButton('Vazgeç');cancel.clicked.connect(self.close);buttons.addWidget(cancel)
            self.confirm=QPushButton('Beyaz bu · Kaydet');self.confirm.setEnabled(False);self.confirm.clicked.connect(self.accept);buttons.addWidget(self.confirm)
            self.timer=QTimer(self);self.timer.setInterval(60);self.timer.setSingleShot(True);self.timer.timeout.connect(self.send_pending)
            self.worker=QProcess(self);self.worker.setProgram('/usr/bin/pkexec');self.worker.setArguments(['/usr/bin/python3',str(Path(__file__).resolve()),'--backend'])
            self.worker.readyReadStandardOutput.connect(self.output)
            self.worker.readyReadStandardError.connect(self.error_output)
            self.worker.finished.connect(self.finished)
            self.worker.errorOccurred.connect(lambda _:self.calibration_failed('Işık yardımcısı başlatılamadı.'))
            from calibration_session import CalibrationSession
            self.calibration_session=CalibrationSession(self)
            self.status.setText('Sabit kalibrasyon parlaklığı hazırlanıyor…')
            self.calibration_session.begin(self.worker.start,self.calibration_failed)

        def calibration_failed(self,message):
            self.ready=False;self.applied=None;self.confirm.setEnabled(False)
            self.calibration_session.close()
            if self.worker.state()!=QProcess.ProcessState.NotRunning:
                self.send({'restore':True});self.worker.closeWriteChannel()
            self.status.setText(message)
        def rgb(self): return [s.value() for s in self.sliders]
        def changed(self,*_):
            self.pending=self.rgb();self.values.setText('R %d · G %d · B %d'%tuple(self.pending));self.confirm.setEnabled(False)
            if not self.timer.isActive():self.timer.start()
        def send(self, command):self.worker.write((json.dumps(command)+'\n').encode())
        def send_pending(self):
            if not self.ready or self.busy or self.pending is None:return
            rgb=self.pending;self.pending=None;self.busy=True;self.send({'color':rgb})
        def output(self):
            self.read_buffer+=bytes(self.worker.readAllStandardOutput())
            while b'\n' in self.read_buffer:
                raw,self.read_buffer=self.read_buffer.split(b'\n',1)
                try:reply=json.loads(raw)
                except ValueError:continue
                if reply.get('ready'):
                    self.ready=True;self.status.setText('Parlaklık %100 sabit. Sürgüler klavyeye anlık uygulanır.');self.changed()
                if 'applied' in reply:
                    self.busy=False;self.applied=reply['applied']
                    self.confirm.setEnabled(self.applied==self.rgb() and min(self.applied)>0)
                    if self.pending is not None:self.send_pending()
                if 'confirmed' in reply:
                    point=reply['confirmed'];payload={'white_point':point,'brightness':reply['brightness'],'model':'SLAYER R9T','confirmed_at':time.time()}
                    fd=os.open(result,os.O_WRONLY|os.O_CREAT|os.O_TRUNC|os.O_NOFOLLOW,0o600)
                    with os.fdopen(fd,'w') as out:json.dump(payload,out);out.write('\n')
                    self.confirmed=True;self.confirm.setEnabled(False)
                    for slider in self.sliders:slider.setEnabled(False)
                    self.status.setText('Beyaz noktası kaydedildi: %d / %d / %d. Bu pencereyi kapatabilirsin.'%tuple(point))
                    print('WHITE_POINT_CONFIRMED '+json.dumps(payload),flush=True)
                    self.persist=QProcess(self)
                    self.persist.setProgram('/usr/bin/busctl')
                    self.persist.setArguments(['--system','call','org.gamegaraj.Slayer1','/org/gamegaraj/Slayer1','org.gamegaraj.Slayer1','Apply','asas','5','keyboard','white-point',*map(str,point),'4','keyboard_white_point',','.join(map(str,point)),'keyboard_color_profile','off'])
                    self.persist.finished.connect(self.persisted)
                    self.persist.start()
                    self.status.setText('Beyaz noktası uygulanıyor ve şifreli ayarlara kaydediliyor…')
        def restore_accent(self):
            if self.was_kameleon:
                subprocess.run(['/usr/bin/slayerctl','keyboard','accent','on'],timeout=2)
                self.was_kameleon=False
        def persisted(self,code,*_):
            if code==0:
                self.status.setText('Beyaz dengesi kalıcı olarak kaydedildi. Önceki renkler yeni kalibrasyonla uygulanır; pencereyi kapatabilirsin.')
                self.restore_accent()
            else:
                self.status.setText('Ölçüm alındı; kalıcı ayar kaydedilemedi: '+bytes(self.persist.readAllStandardError()).decode(errors='replace')[-300:])
        def error_output(self):
            message=bytes(self.worker.readAllStandardError()).decode(errors='replace').strip()
            if message:self.status.setText(message[-400:])
        def accept(self):
            if self.applied==self.rgb() and self.ready and not self.busy:
                self.confirm.setEnabled(False);self.send({'commit':self.rgb()})
        def finished(self,*_):
            self.calibration_session.close()
            if not self.confirmed:self.status.setText('Yardımcı kapandı. Kalibrasyon kaydedilmedi; önceki ışık geri yüklendi.')
        def closeEvent(self,event:QCloseEvent):
            if not self.confirmed and self.worker.state()!=QProcess.ProcessState.NotRunning:
                self.send({'restore':True});self.worker.closeWriteChannel()
                if not self.worker.waitForFinished(3000):event.ignore();return
            self.calibration_session.close()
            self.restore_accent()
            event.accept()
    window=Window();window.show();window.raise_();window.activateWindow()
    return app.exec()

if __name__=='__main__':
    if sys.argv[1:]==['--backend']:
        try:backend()
        except Exception as e:
            print(str(e),file=sys.stderr,flush=True);sys.exit(1)
    else:
        try:sys.exit(gui())
        except ImportError:
            subprocess.run(['/usr/bin/kdialog','--error','Beyaz dengesi aracı için python3-pyside6 paketi gerekli.'])
            sys.exit(1)
