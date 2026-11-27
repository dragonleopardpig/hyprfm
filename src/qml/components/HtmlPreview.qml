import QtQuick
import QtWebEngine
import HyprFM.Html

WebEngineView {
    id: page
    objectName: "browserHtmlPreview"
    readonly property string html: parent.html
    readonly property url baseUrl: parent.baseUrl
    property bool failed: false

    focus: true
    zoomFactor: parent.zoom
    profile: OfflineProfile
    settings.javascriptEnabled: false
    settings.localContentCanAccessRemoteUrls: false
    settings.unknownUrlSchemePolicy: WebEngineSettings.DisallowUnknownUrlSchemes
    settings.pdfViewerEnabled: false
    settings.dnsPrefetchEnabled: false

    function showDocument() {
        failed = false
        loadHtml(html, baseUrl)
    }

    onHtmlChanged: Qt.callLater(showDocument)
    onBaseUrlChanged: Qt.callLater(showDocument)
    Component.onCompleted: Qt.callLater(showDocument)
    onNavigationRequested: request => {
        // loadHtml is an API-initiated typed navigation in WebEngine.
        if (!request.isMainFrame || request.navigationType !== WebEngineNavigationRequest.TypedNavigation)
            request.action = WebEngineNavigationRequest.IgnoreRequest
    }
    onLoadingChanged: info => {
        if (info.status === WebEngineView.LoadFailedStatus)
            failed = true
    }
    onRenderProcessTerminated: failed = true
    onContextMenuRequested: request => { request.accepted = true }
    onFileDialogRequested: request => { request.accepted = true }

    Keys.onShortcutOverride: event => {
        if (event.matches(StandardKey.Copy) || event.matches(StandardKey.SelectAll))
            event.accepted = true
    }
    Keys.onPressed: event => {
        if (event.matches(StandardKey.Copy)) {
            triggerWebAction(WebEngineView.Copy)
            event.accepted = true
        } else if (event.matches(StandardKey.SelectAll)) {
            triggerWebAction(WebEngineView.SelectAll)
            event.accepted = true
        }
    }
}
