// SPDX-License-Identifier: MIT
import QtQuick
import QtTest
import "../../gui/qml/components"
import "../../plasma/org.gamegaraj.slayerctl.camera/contents/ui"

Item {
    width: 440
    height: 1000
    Component {
        id: batteryComponent
        BatteryLimit {
            width: 320
            supported: true
            busy: false
            currentLimit: "80"
            reason: ""
        }
    }
    Component {
        id: cameraComponent
        CameraView {
            width: 320
            cameraState: 1
            busy: false
            errorText: ""
            eventMonitoring: true
        }
    }
    SignalSpy {
        id: applied
        signalName: "applyRequested"
    }
    SignalSpy {
        id: toggled
        signalName: "enableRequested"
    }
    TestCase {
        name: "NativeCameraAndBattery"
        when: windowShown
        function cleanup() {
            applied.target = null;
            applied.clear();
            toggled.target = null;
            toggled.clear();
        }
        function test_battery_explicit_apply() {
            const view = createTemporaryObject(batteryComponent, parent);
            verify(view !== null);
            applied.target = view;
            const value = findChild(view, "batteryLimitValue");
            const button = findChild(view, "batteryLimitApply");
            verify(!button.enabled);
            compare(applied.count, 0);
            value.forceActiveFocus();
            keyClick(Qt.Key_Up);
            compare(value.value, 81);
            compare(applied.count, 0);
            verify(button.enabled);
            mouseClick(button);
            compare(applied.count, 1);
            compare(applied.signalArguments[0][0], 81);
            view.busy = true;
            verify(!value.enabled && !button.enabled);
        }
        function test_battery_unsupported_visible() {
            const view = createTemporaryObject(batteryComponent, parent, {
                supported: false,
                currentLimit: "",
                reason: "Fixture: yüzde eşiği yok."
            });
            verify(view !== null);
            applied.target = view;
            const value = findChild(view, "batteryLimitValue");
            const button = findChild(view, "batteryLimitApply");
            verify(value.visible && !value.enabled);
            compare(value.displayText, "—");
            verify(!button.enabled);
            mouseClick(button);
            compare(applied.count, 0);
        }
        function test_battery_invalid_readback_data() {
            return [
                {
                    tag: "empty",
                    value: ""
                },
                {
                    tag: "fraction",
                    value: "80.5"
                },
                {
                    tag: "invalid",
                    value: "unknown"
                },
                {
                    tag: "range",
                    value: "101"
                }
            ];
        }
        function test_battery_invalid_readback(data) {
            const view = createTemporaryObject(batteryComponent, parent, {
                currentLimit: data.value
            });
            verify(view !== null);
            verify(!view.known);
            verify(!findChild(view, "batteryLimitApply").enabled);
            compare(findChild(view, "batteryLimitValue").displayText, "—");
        }
        function test_camera_mouse_keyboard_and_busy() {
            const view = createTemporaryObject(cameraComponent, parent);
            verify(view !== null);
            toggled.target = view;
            const button = findChild(view, "cameraToggle");
            verify(button.enabled);
            mouseClick(button);
            compare(toggled.count, 1);
            compare(toggled.signalArguments[0][0], false);
            view.cameraState = 0;
            button.forceActiveFocus();
            keyClick(Qt.Key_Space);
            compare(toggled.count, 2);
            compare(toggled.signalArguments[1][0], true);
            view.busy = true;
            mouseClick(button);
            compare(toggled.count, 2);
            view.busy = false;
            view.cameraState = -1;
            verify(!button.enabled);
        }
        function test_camera_narrow_error_geometry() {
            const view = createTemporaryObject(cameraComponent, parent, {
                width: 280,
                cameraState: -1,
                errorText: "Kamera ayarı uygulanamadı. Yönetici doğrulaması tamamlanmadı.",
                eventMonitoring: false
            });
            verify(view !== null);
            const button = findChild(view, "cameraToggle");
            waitForPolish(view);
            verify(view.implicitHeight > 100);
            verify(button.width <= view.availableWidth);
            verify(button.mapToItem(view, 0, 0).x >= 0);
        }
    }
}
