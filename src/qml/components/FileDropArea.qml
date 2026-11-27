import QtQuick

DropArea {
    id: root

    property string destinationPath: ""
    signal transferRequested(var paths, string destinationPath, bool moveOperation)

    keys: ["text/uri-list"]
    property int defaultAction: Qt.CopyAction

    function sourcePaths(event) {
        if (dragHelper.active && dragHelper.activePaths.length > 0)
            return dragHelper.activePaths

        const entries = event.hasUrls ? event.urls : (event.hasText ? event.text.split("\n") : [])
        const paths = []
        for (const entry of entries) {
            const path = event.hasUrls ? entry.toString() : entry.trim()
            if (path)
                paths.push(path.startsWith("file://") ? decodeURIComponent(path.substring(7)) : path)
        }
        return paths
    }

    function canTransfer(paths) {
        return destinationPath && paths.length > 0
            && !paths.some(path => destinationPath === path || destinationPath.startsWith(path + "/"))
            && !paths.every(path => fileOps.parentPath(path) === destinationPath)
    }

    function acceptAction(event, preferredAction) {
        const action = dragHelper.fileDropAction(preferredAction, event.supportedActions)
        if (action === Qt.IgnoreAction)
            event.accepted = false
        else
            event.accept(action)
        return action
    }

    onEntered: drag => {
        const paths = sourcePaths(drag)
        if (!canTransfer(paths)) {
            drag.accepted = false
            return
        }
        defaultAction = dragHelper.defaultFileDropAction(paths, destinationPath)
        acceptAction(drag, defaultAction)
    }
    onPositionChanged: drag => acceptAction(drag, defaultAction)

    onDropped: drop => {
        const paths = sourcePaths(drop)
        if (!canTransfer(paths)) {
            drop.accepted = false
            return
        }
        const action = acceptAction(drop, dragHelper.defaultFileDropAction(paths, destinationPath))
        if (action !== Qt.IgnoreAction)
            root.transferRequested(paths, destinationPath, action === Qt.MoveAction)
    }
}
