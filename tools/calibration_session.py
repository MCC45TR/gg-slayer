#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Thin calibration client: ctl owns idle inhibition and brightness settling."""
from PySide6.QtCore import QObject
from PySide6.QtDBus import QDBusConnection, QDBusMessage, QDBusPendingCallWatcher, QDBusServiceWatcher

SERVICE = 'org.gamegaraj.Slayer.Session1'
PATH = '/org/gamegaraj/Slayer/Session1'


class CalibrationSession(QObject):
    def __init__(self, parent):
        super().__init__(parent)
        self.closed = False
        self.requested = False
        self.failed = None
        self.service = QDBusServiceWatcher(SERVICE, QDBusConnection.sessionBus(),
                                          QDBusServiceWatcher.WatchModeFlag.WatchForUnregistration, self)
        self.service.serviceUnregistered.connect(self.lost)

    def lost(self, _name):
        if self.requested and not self.closed:
            self.close()
            if self.failed:
                self.failed('ctl bağlantısı kapandı. Ölçüm durduruldu; aracı yeniden açarak devam edebilirsin.')

    def begin(self, ready, failed):
        self.requested = True
        self.failed = failed
        message = QDBusMessage.createMethodCall(SERVICE, PATH, SERVICE, 'BeginCalibration')
        pending = QDBusPendingCallWatcher(QDBusConnection.sessionBus().asyncCall(message, 20000), self)

        def finished():
            reply = pending.reply()
            pending.deleteLater()
            if self.closed:
                return
            if reply.type() == QDBusMessage.MessageType.ErrorMessage or reply.arguments() != [True]:
                self.close()
                failed('Kalibrasyon için ctl oturum bağlantısı hazırlanamadı. '
                       'Güncel slayerctl paketinin kurulu olduğunu kontrol et. ' + reply.errorMessage())
                return
            ready()

        pending.finished.connect(finished)

    def close(self):
        if self.closed:
            return
        self.closed = True
        if self.requested:
            message = QDBusMessage.createMethodCall(SERVICE, PATH, SERVICE, 'EndCalibration')
            QDBusConnection.sessionBus().asyncCall(message, 2000)
