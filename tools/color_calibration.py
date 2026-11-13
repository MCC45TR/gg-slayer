#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Collect screen RGB -> raw LED RGB matches without changing the reference swatch."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

TARGETS = [
    ('Beyaz',[255,255,255]), ('Kırmızı',[255,0,0]),
    ('Yeşil',[0,255,0]), ('Mavi',[0,0,255]),
    ('Sarı',[255,255,0]), ('Camgöbeği',[0,255,255]),
    ('Macenta',[255,0,255]), ('Koyu gri · 32',[32,32,32]),
    ('Gri · 64',[64,64,64]), ('Gri · 96',[96,96,96]),
    ('Orta gri · 128',[128,128,128]), ('Gri · 160',[160,160,160]),
    ('Açık gri · 192',[192,192,192]), ('Açık gri · 224',[224,224,224]),
    ('Turuncu',[255,128,0]), ('Mor',[128,0,255]),
    ('Koyu camgöbeği',[0,128,128]), ('Pembe',[255,128,192]),
    ('KDE zeytin yeşili',[113,116,58]), ('Siyah',[0,0,0]),
]

def main():
    from PySide6.QtCore import Qt,QProcess,QTimer,QSignalBlocker
    from PySide6.QtGui import QColor,QPalette,QIcon,QColorSpace
    from PySide6.QtWidgets import QApplication,QWidget,QVBoxLayout,QHBoxLayout,QGridLayout,QLabel,QFrame,QSlider,QSpinBox,QPushButton,QProgressBar,QSizePolicy
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path.home()/'.local/share/slayerctl/screen-match-r9t.json')
    parser.add_argument('--preview',action='store_true')
    args=parser.parse_args()
    app=QApplication(sys.argv[:1]);app.setApplicationName('Slayer · Renk eşleştirmesi')
    app.setWindowIcon(QIcon.fromTheme('preferences-desktop-color'))
    output=args.output.resolve()
    seed=[160,255,255]
    runtime=Path(os.environ.get('XDG_RUNTIME_DIR',f'/run/user/{os.getuid()}'))/'slayerctl-white-point.json'
    try:
        value=json.loads(runtime.read_text())['white_point']
        if len(value)==3 and all(type(c) is int and 1<=c<=255 for c in value):seed=value
    except (OSError,ValueError,KeyError):pass
    records=[None]*len(TARGETS)
    if output.exists() and not args.preview:
        try:
            old=json.loads(output.read_text())
            if old.get('targets')==[rgb for _,rgb in TARGETS] and len(old.get('samples',[]))==len(TARGETS):
                if not old.get('complete'):records=old['samples']
                else:output.replace(output.with_name(output.stem+'.previous-'+str(int(time.time()))+'.json'))
        except (OSError,ValueError):pass

    class Swatch(QWidget):
        def __init__(self):
            super().__init__();self.setMinimumSize(320,320)
            self.setSizePolicy(QSizePolicy.Policy.Expanding,QSizePolicy.Policy.Expanding)
            self.patch=QFrame(self);self.patch.setAutoFillBackground(True);self.setAccessibleName('Ekrandaki sabit hedef renk')
        def color(self,rgb):
            palette=self.patch.palette();palette.setColor(QPalette.ColorRole.Window,QColor(*rgb));self.patch.setPalette(palette)
        def resizeEvent(self,event):
            size=min(self.width(),self.height());self.patch.setGeometry((self.width()-size)//2,(self.height()-size)//2,size,size);super().resizeEvent(event)

    class Window(QWidget):
        def __init__(self):
            super().__init__();self.setWindowTitle('Slayer · Ekran ve klavye renk eşleştirmesi')
            self.resize(940,680);self.setMinimumSize(720,560)
            self.index=next((i for i,r in enumerate(records) if r is None),0)
            self.records=records;self.complete=False;self.ready=args.preview;self.busy=False
            self.applied=None;self.pending=None;self.buffer=b'';self.worker=None
            self.calibration_session=None
            self.was_kameleon=False;self.sliders=[];self.spins=[]
            self.created_at=time.time()
            if not args.preview:
                try:
                    self.was_kameleon=subprocess.check_output(['/usr/bin/slayerctl','keyboard','accent','status'],text=True,timeout=2).strip()=='true'
                    if self.was_kameleon:subprocess.run(['/usr/bin/slayerctl','keyboard','accent','off'],check=True,timeout=2)
                except (OSError,subprocess.SubprocessError):pass
            layout=QVBoxLayout(self);layout.setContentsMargins(22,22,22,22);layout.setSpacing(16)
            self.title=QLabel();font=self.title.font();font.setPointSizeF(font.pointSizeF()*1.35);font.setBold(True);self.title.setFont(font);layout.addWidget(self.title)
            hint=QLabel('Büyük karedeki rengi klavyede yakala. Kare sabit kalır; sürgüler yalnız klavyeyi değiştirir. Ölçüm boyunca ekran parlaklığını ve gece rengi ayarını aynı tut.');hint.setWordWrap(True);layout.addWidget(hint)
            row=QHBoxLayout();row.setSpacing(24);layout.addLayout(row,1)
            self.swatch=Swatch();row.addWidget(self.swatch,3)
            controls=QVBoxLayout();row.addLayout(controls,2)
            self.reference=QLabel();self.reference.setWordWrap(True);controls.addWidget(self.reference)
            grid=QGridLayout();grid.setHorizontalSpacing(10);grid.setVerticalSpacing(22);controls.addLayout(grid)
            for i,name in enumerate(['Kırmızı · R','Yeşil · G','Mavi · B']):
                grid.addWidget(QLabel(name),i*2,0,1,2)
                slider=QSlider(Qt.Orientation.Horizontal);slider.setRange(0,255);slider.setAccessibleName(name);grid.addWidget(slider,i*2+1,0)
                spin=QSpinBox();spin.setRange(0,255);spin.setAccessibleName(name+' değeri');grid.addWidget(spin,i*2+1,1)
                slider.valueChanged.connect(spin.setValue);spin.valueChanged.connect(slider.setValue);slider.valueChanged.connect(self.changed)
                self.sliders.append(slider);self.spins.append(spin)
            self.drive=QLabel();controls.addWidget(self.drive)
            controls.addStretch()
            controls.addWidget(QLabel('Klavye parlaklığı: %100 sabit'))
            self.status=QLabel('Anlık ışık yardımcısı hazırlanıyor…');self.status.setWordWrap(True);layout.addWidget(self.status)
            self.progress=QProgressBar();self.progress.setRange(0,len(TARGETS));self.progress.setFormat('%v / %m renk kaydedildi');layout.addWidget(self.progress)
            buttons=QHBoxLayout();layout.addLayout(buttons)
            self.back=QPushButton('← Önceki renk');self.back.clicked.connect(self.previous);buttons.addWidget(self.back)
            close=QPushButton('Kapat · İlerleme korunsun');close.clicked.connect(self.close);buttons.addWidget(close)
            self.next=QPushButton('Bu renk tamam →');self.next.clicked.connect(self.save_next);buttons.addWidget(self.next)
            self.timer=QTimer(self);self.timer.setInterval(60);self.timer.setSingleShot(True);self.timer.timeout.connect(self.send_pending)
            self.show_target()
            if args.preview:
                self.status.setText('Önizleme · donanım ve dosya yazmaları kapalı');self.send_pending()
            else:
                self.worker=QProcess(self);self.worker.setProgram('/usr/bin/pkexec')
                self.worker.setArguments(['/usr/bin/python3',str(Path(__file__).with_name('white_balance.py').resolve()),'--backend'])
                self.worker.readyReadStandardOutput.connect(self.read_output)
                self.worker.readyReadStandardError.connect(self.read_error)
                self.worker.errorOccurred.connect(lambda _:self.calibration_failed('Işık yardımcısı başlatılamadı.'))
                self.worker.finished.connect(self.worker_finished)
                from calibration_session import CalibrationSession
                self.calibration_session=CalibrationSession(self)
                self.status.setText('Sabit kalibrasyon parlaklığı hazırlanıyor…')
                self.calibration_session.begin(self.worker.start,self.calibration_failed)
            self.write_data()

        def rgb(self):return [s.value() for s in self.sliders]
        def calibration_failed(self,message):
            self.ready=False;self.applied=None;self.next.setEnabled(False)
            if self.calibration_session:self.calibration_session.close()
            if self.worker and self.worker.state()!=QProcess.ProcessState.NotRunning:
                self.send({'restore':True});self.worker.closeWriteChannel()
            self.status.setText(message)
        def show_target(self):
            name,target=TARGETS[self.index]
            self.title.setText(f'{self.index+1} / {len(TARGETS)} · {name}')
            self.swatch.color(target)
            self.reference.setText('Ekrandaki hedef\n#'+''.join(f'{v:02X}' for v in target)+'\nRGB '+ ' / '.join(map(str,target)))
            sample=self.records[self.index]
            guess=sample['led'] if sample else [(v*w+127)//255 for v,w in zip(target,seed)]
            blockers=[QSignalBlocker(w) for w in self.sliders+self.spins]
            for slider,spin,value in zip(self.sliders,self.spins,guess):slider.setValue(value);spin.setValue(value)
            del blockers
            self.progress.setValue(sum(r is not None for r in self.records));self.back.setEnabled(self.index>0)
            self.changed()
        def changed(self,*_):
            self.pending=self.rgb();self.applied=None;self.next.setEnabled(False)
            self.drive.setText('Klavyeye gönderilen\nRGB '+' / '.join(map(str,self.pending)))
            if not self.timer.isActive():self.timer.start()
        def send(self,command):
            if self.worker:self.worker.write((json.dumps(command)+'\n').encode())
        def send_pending(self):
            if not self.ready or self.busy or self.pending is None:return
            rgb=self.pending;self.pending=None
            if args.preview:self.applied=rgb;self.next.setEnabled(True);return
            self.busy=True;self.send({'color':rgb})
        def read_output(self):
            self.buffer+=bytes(self.worker.readAllStandardOutput())
            while b'\n' in self.buffer:
                raw,self.buffer=self.buffer.split(b'\n',1)
                try:message=json.loads(raw)
                except ValueError:continue
                if message.get('ready'):
                    self.ready=True;self.status.setText('Sürgüler anlık uygulanır. Renk eşleştiğinde “Bu renk tamam” düğmesine bas.');self.send_pending()
                if 'applied' in message:
                    self.busy=False;self.applied=message['applied'];self.next.setEnabled(self.applied==self.rgb())
                    if self.pending is not None:self.send_pending()
        def read_error(self):
            text=bytes(self.worker.readAllStandardError()).decode(errors='replace').strip()
            if text:self.status.setText(text[-400:])
        def write_data(self):
            if args.preview:return
            screen=self.screen()
            data={'version':1,'model':'SLAYER R9T','targets':[rgb for _,rgb in TARGETS],
                  'samples':self.records,'complete':self.complete,'white_point_seed':seed,
                  'keyboard_brightness':50,'updated_at':time.time(),
                  'screen':{'name':screen.name(),'width':screen.size().width(),'height':screen.size().height(),'scale':screen.devicePixelRatio()},
                  'method':'manual screen comparison; raw uncorrected LED RGB'}
            output.parent.mkdir(parents=True,exist_ok=True)
            temporary=output.with_name(output.name+'.tmp')
            fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_TRUNC|os.O_NOFOLLOW,0o600)
            with os.fdopen(fd,'w') as stream:json.dump(data,stream,ensure_ascii=False,indent=2);stream.write('\n')
            temporary.replace(output)
        def save_next(self):
            if self.applied!=self.rgb() or self.busy:return
            name,target=TARGETS[self.index]
            self.records[self.index]={'name':name,'screen':target,'led':self.rgb(),'confirmed_at':time.time()}
            self.complete=all(r is not None for r in self.records)
            self.write_data();self.progress.setValue(sum(r is not None for r in self.records))
            if self.index<len(TARGETS)-1:self.index+=1;self.show_target()
            elif self.complete:
                self.next.setEnabled(False);self.back.setEnabled(False)
                for w in self.sliders+self.spins:w.setEnabled(False)
                self.status.setText('20 renk kaydedildi. Önceki klavye ışığı geri yükleniyor; verilerden renk düzeltme eğrisi oluşturulabilir.')
                print('CALIBRATION_COMPLETE '+str(output),flush=True)
                self.send({'restore':True})
                if self.worker:self.worker.closeWriteChannel()
            else:
                self.index=next(i for i,r in enumerate(self.records) if r is None);self.show_target()
        def previous(self):
            if self.index>0:self.index-=1;self.show_target()
        def restore_accent(self):
            if self.was_kameleon:
                subprocess.run(['/usr/bin/slayerctl','keyboard','accent','on'],timeout=2)
                self.was_kameleon=False
        def worker_finished(self,*_):
            if self.calibration_session:self.calibration_session.close()
            if self.complete:self.status.setText('20 / 20 renk kaydedildi. Renk dönüşümü hesaplanıp uygulanıyor…');self.fit_model()
            elif not args.preview:self.status.setText('Işık yardımcısı kapandı. Kaydedilen ölçümler korunuyor; aracı tekrar açınca devam edebilirsin.')
        def fit_model(self):
            self.fitter=QProcess(self);self.fitter.setProgram('/usr/bin/python3')
            model_file=output.with_name(output.stem+'-model.json')
            self.fitter.setArguments([str(Path(__file__).with_name('fit_color_profile.py').resolve()),'--input',str(output),'--output',str(model_file),'--apply'])
            self.fitter.finished.connect(self.fit_finished);self.fitter.start()
        def fit_finished(self,code,*_):
            if code==0:
                self.status.setText('Ekran eşleştirmesi uygulandı ve şifreli olarak kaydedildi. Pencereyi kapatabilirsin.')
                self.restore_accent()
            else:
                message=bytes(self.fitter.readAllStandardError()).decode(errors='replace')[-350:]
                self.status.setText('20 ölçüm korundu; profil uygulanamadı: '+message)
        def closeEvent(self,event):
            if hasattr(self,'fitter') and self.fitter.state()!=QProcess.ProcessState.NotRunning:
                self.status.setText('Renk modeli kaydediliyor…');event.ignore();return
            self.write_data()
            if self.worker and self.worker.state()!=QProcess.ProcessState.NotRunning:
                self.send({'restore':True});self.worker.closeWriteChannel()
                if not self.worker.waitForFinished(3000):event.ignore();return
            if self.calibration_session:self.calibration_session.close()
            self.restore_accent();event.accept()

    window=Window();window.show();window.raise_();window.activateWindow()
    handle=window.windowHandle()
    if handle and hasattr(handle,'setColorSpace'):handle.setColorSpace(QColorSpace(QColorSpace.NamedColorSpace.SRgb))
    screenshot=os.environ.get('SLAYER_CALIBRATION_SCREENSHOT')
    if args.preview and screenshot:
        def capture():window.grab().save(screenshot);app.quit()
        QTimer.singleShot(600,capture)
    return app.exec()

if __name__=='__main__':
    sys.exit(main())
