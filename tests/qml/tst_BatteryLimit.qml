// SPDX-License-Identifier: MIT
import QtQuick
import QtTest
import "../../gui/qml/components"

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
    SignalSpy {
        id: applied
        signalName: "applyRequested"
    }
    TestCase {
        name: "BatteryLimit"
        when: windowShown
        function cleanup() {
            applied.target = null;
            applied.clear();
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
    }
}
