import QtQuick

Item {
    id: root

    required property TextEdit editor
    required property string html
    required property real zoom
    required property real availableWidth
    property var placements: []
    readonly property var prepared: previewService.prepareMarkdownGraphics(editor.textDocument, html)
    readonly property real rightEdge: placements.reduce(
        (extent, graphic) => Math.max(extent, graphic.rect.x + graphic.rect.width), 0)
    readonly property string preparedHtml: prepared.html.replace(/<img\b(?![^>]*#hyprfm-\d+")/g,
        '<img style="max-width:' + Math.max(64, Math.floor(availableWidth)) + 'px"')

    // VectorImage is available starting with Qt 6.8. Keep older builds
    // usable and re-render the SVG at the requested size in their Image.
    property Component vectorImage
    function refresh() {
        if (!vectorImage && prepared.images?.length)
            vectorImage = Qt.createComponent("MarkdownVectorImage.qml")
        placements = []
        layoutTimer.restart()
    }

    Component.onCompleted: refresh()
    onPreparedChanged: refresh()
    onZoomChanged: layoutTimer.restart()
    onAvailableWidthChanged: layoutTimer.restart()

    Connections {
        target: root.editor
        function onTextChanged() { layoutTimer.restart() }
        function onContentHeightChanged() { layoutTimer.restart() }
        function onFontChanged() { layoutTimer.restart() }
    }

    Timer {
        id: layoutTimer
        interval: 0
        onTriggered: root.placements = previewService.layoutMarkdownGraphics(
            root.editor.textDocument, root.prepared.images || [], root.zoom, root.availableWidth)
    }

    Repeater {
        model: root.placements.length
        delegate: Loader {
            required property int index
            readonly property var graphic: root.placements[index]
            readonly property url svgSource: graphic.url
            x: graphic.rect.x + root.editor.leftPadding
            y: graphic.rect.y + root.editor.topPadding
            width: graphic.rect.width
            height: graphic.rect.height
            sourceComponent: root.vectorImage && root.vectorImage.status === Component.Ready
                ? root.vectorImage : rasterImage
        }
    }

    Component {
        id: rasterImage
        Image {
            source: parent ? parent.svgSource : ""
            sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio),
                                Math.ceil(height * Screen.devicePixelRatio))
        }
    }
}
