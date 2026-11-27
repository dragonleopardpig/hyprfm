import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprFM
import Quill as Q

Item {
    id: root
    property Item scope: null
    property bool available: true
    property Flickable scrollTarget: null
    property Flickable keyboardScrollTarget: scrollTarget
    property var pageWheel: null
    property real factor: 1.0
    readonly property int zoomPercent: Math.round(factor * 100)
    readonly property bool isDefaultZoom: zoomPercent === 100
    readonly property real minimum: 0.25
    readonly property real maximum: 4.0
    property bool sourceAvailable: false
    property bool showingSource: false
    signal sourceToggled()
    signal interacted()
    onFactorChanged: zoomSlider.value = factor
    implicitHeight: Theme.controlSize + 8
    height: available ? implicitHeight : 0
    visible: available

    function setFactor(value) {
        factor = Math.max(minimum, Math.min(maximum, isFinite(value) ? value : 1))
    }
    function reset() { setFactor(1) }
    // One wheel notch or one Ctrl++ / Ctrl+- press.
    function zoomSteps(steps) { setFactor(factor * Math.exp(steps * 0.14)) }
    function focusPreview() {
        interacted()
        Qt.callLater(function() { if (root.scope && root.scope.visible && !root.scope.activeFocus) root.scope.forceActiveFocus() })
    }
    function wheelZoom(wheel) {
        var steps = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y / 10 : wheel.angleDelta.y / 120
        if (steps === 0) return false
        zoomSteps(steps * (wheel.inverted ? -1 : 1))
        focusPreview()
        return true
    }

    function scrollContent(wheel) {
        if (pageWheel && factor <= 1) { pageWheel(wheel); return true }
        if (!scrollTarget) return false
        var dx = wheel.pixelDelta.x !== 0 ? wheel.pixelDelta.x : wheel.angleDelta.x / 120 * 42
        var dy = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y / 120 * 42
        if (wheel.modifiers & Qt.ShiftModifier) { dx = dx || dy; dy = 0 }
        if (wheel.inverted) { dx = -dx; dy = -dy }
        scrollTarget.contentX = Math.max(0, Math.min(scrollTarget.contentWidth - scrollTarget.width, scrollTarget.contentX - dx))
        scrollTarget.contentY = Math.max(0, Math.min(scrollTarget.contentHeight - scrollTarget.height, scrollTarget.contentY - dy))
        return dx !== 0 || dy !== 0
    }

    function handleScrollKey(event) {
        const target = keyboardScrollTarget
        if (!target || !target.visible || (event.modifiers & ~Qt.KeypadModifier) !== 0)
            return

        const top = target.originY - target.topMargin
        const bottom = Math.max(top, target.originY + target.contentHeight - target.height + target.bottomMargin)
        const step = 42
        const page = Math.max(step, target.height - step)
        let position = target.contentY
        switch (event.key) {
        case Qt.Key_Up: position -= step; break
        case Qt.Key_Down: position += step; break
        case Qt.Key_PageUp: position -= page; break
        case Qt.Key_PageDown: position += page; break
        case Qt.Key_Home: position = top; break
        case Qt.Key_End: position = bottom; break
        default: return
        }

        target.cancelFlick()
        target.contentY = Math.max(top, Math.min(bottom, position))
        event.accepted = true
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6
        Q.Slider {
            id: zoomSlider
            objectName: "previewZoomSlider"
            Layout.fillWidth: true
            Layout.minimumWidth: 48
            from: root.minimum
            to: root.maximum
            stepSize: 0.05
            decimals: 2
            activeFocusOnTab: true
            value: root.factor
            Accessible.name: qsTr("Preview zoom")
            onMoved: value => { root.setFactor(value); root.focusPreview() }
            Keys.onLeftPressed: event => { root.setFactor(root.factor - 0.05); event.accepted = true }
            Keys.onRightPressed: event => { root.setFactor(root.factor + 0.05); event.accepted = true }
        }
        HoverRect {
            id: zoomPercentButton
            objectName: "previewZoomPercent"
            Layout.preferredWidth: Math.ceil(48 * Theme.uiScale)
            Layout.preferredHeight: Theme.compactControlSize
            Layout.alignment: Qt.AlignVCenter
            tooltip: qsTr("Reset preview zoom to 100%")
            shortcut: "text_reset"
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Preview zoom %1%, reset to 100%").arg(root.zoomPercent)
            onClicked: { root.reset(); root.focusPreview() }
            Text {
                anchors.centerIn: parent
                text: root.zoomPercent + "%"
                color: root.isDefaultZoom ? Theme.subtext : Theme.text
                font.pointSize: Theme.fontSmall
                font.weight: root.isDefaultZoom ? Font.Normal : Font.Medium
            }
        }
        Q.Button {
            objectName: "previewSourceToggle"
            visible: root.sourceAvailable
            text: root.showingSource ? qsTr("Rendered") : qsTr("Source")
            Accessible.name: root.showingSource ? qsTr("Show rendered preview") : qsTr("Show source")
            variant: "ghost"
            size: "small"
            onClicked: {
                root.sourceToggled()
                root.focusPreview()
            }
        }
        HoverRect {
            id: resetZoomButton
            objectName: "resetPreviewZoom"
            Layout.preferredWidth: Math.max(Theme.controlSize, Math.ceil(46 * Theme.uiScale))
            Layout.preferredHeight: Theme.compactControlSize
            Layout.alignment: Qt.AlignVCenter
            tooltip: qsTr("Reset preview zoom to 100%")
            shortcut: "text_reset"
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Reset preview zoom to 100%")
            color: !root.isDefaultZoom
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, hovered ? 0.22 : 0.12)
                : (hovered ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.1) : "transparent")
            border.width: root.isDefaultZoom ? 0 : 1
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, hovered ? 0.55 : 0.35)
            onClicked: { root.reset(); root.focusPreview() }
            Text {
                objectName: "resetPreviewZoomLabel"
                anchors.centerIn: parent
                visible: !root.isDefaultZoom
                text: "100%"
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.weight: Font.DemiBold
            }
            IconUndo {
                anchors.centerIn: parent
                visible: root.isDefaultZoom
                size: 16
                color: Theme.subtext
                opacity: resetZoomButton.hovered ? 0.9 : 0.5
            }
        }
    }
    // The interface text-size keys act on the preview while it has focus;
    // Main.qml's text-size shortcuts stand down then (previewOwnsFocus()).
    readonly property bool keysActive: root.available && !!root.scope && root.scope.visible && root.scope.activeFocus
    Shortcut {
        sequences: [config.shortcutMap["text_larger"], "Ctrl+="]
        enabled: root.keysActive
        onActivated: root.zoomSteps(1)
    }
    Shortcut {
        sequence: config.shortcutMap["text_smaller"]
        enabled: root.keysActive
        onActivated: root.zoomSteps(-1)
    }
    Shortcut {
        sequence: config.shortcutMap["text_reset"]
        enabled: root.keysActive
        onActivated: root.reset()
    }
    Item {
        // Input stays local to this preview; ordinary scrolling and other
        // modifiers continue to its content instead of changing file-view zoom.
        parent: root.scope ? root.scope : root
        anchors.fill: parent
        z: 100
        enabled: root.available
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.ControlModifier
            onWheel: wheel => { blocking = root.wheelZoom(wheel) }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.ControlModifier | Qt.ShiftModifier
            onWheel: wheel => { blocking = root.wheelZoom(wheel) }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.NoModifier
            onWheel: wheel => { blocking = root.scrollContent(wheel) }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.NoModifier
            orientation: Qt.Horizontal
            onWheel: wheel => { blocking = root.scrollContent(wheel) }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.ShiftModifier
            onWheel: wheel => { blocking = root.scrollContent(wheel) }
        }
    }
}
