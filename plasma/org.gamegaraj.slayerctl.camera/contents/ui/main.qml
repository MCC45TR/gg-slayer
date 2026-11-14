// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.gamegaraj.slayerctl.camera

PlasmoidItem {
    id: root
    switchWidth: Kirigami.Units.gridUnit * 16
    switchHeight: Kirigami.Units.gridUnit * 12
    toolTipMainText: qsTr("Dahili kamera")
    toolTipSubText: CameraControl.state === 1 ? qsTr("Kamera erişimi açık") : CameraControl.state === 0 ? qsTr("Kamera erişimi kapalı") : qsTr("Kamera durumu bilinmiyor")
    Plasmoid.icon: CameraControl.state === 0 ? "camera-off-symbolic" : "camera-web"
    // Disabling USB removes video nodes. Keep the native tray control available
    // so the camera can be re-enabled, independently of camera use indicators.
    Plasmoid.status: CameraControl.available ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    onExpandedChanged: if (root.expanded)
        CameraControl.refresh()
    fullRepresentation: CameraView {
        cameraState: CameraControl.state
        busy: CameraControl.busy
        errorText: CameraControl.error
        eventMonitoring: CameraControl.eventMonitoring
        onEnableRequested: enabled => CameraControl.setEnabled(enabled)
        onRefreshRequested: CameraControl.refresh()
    }
}
