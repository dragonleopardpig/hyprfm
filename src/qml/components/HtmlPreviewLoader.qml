import QtQuick

Loader {
    property string html: ""
    property url baseUrl
    property real zoom: 1
    readonly property bool ready: status === Loader.Ready && !item.failed

    active: typeof hyprfmHtmlWebEngine !== "undefined" && hyprfmHtmlWebEngine && html !== ""
    visible: ready
    focus: visible
    source: "qrc:/qml/components/HtmlPreview.qml"
}
