import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "ui"

ApplicationWindow {
    id: win
    width: 1280
    height: 860
    minimumWidth: 860
    minimumHeight: 620
    visible: true
    title: "Easel"

    // Everything takes the shell's look from the current theme's shell.toml,
    // so easel sits on the desktop like the bar, menus, and panels do.
    readonly property var s: theme.style
    readonly property real gap: 8 * s.spacing
    color: s.background
    font.family: s.fontFamily
    font.pixelSize: s.fontSize

    property int selected: 0
    // Set once the user confirms quitting. Qt.quit() closes the window first,
    // and the close check below must not veto a quit that was already confirmed.
    property bool quitting: false
    function quitNow() { quitting = true; Qt.quit() }
    readonly property bool typing: sceneField.activeFocus || varyField.activeFocus
    readonly property bool overlayOpen: help.visible || quitDialog.visible || varyCard.visible || removeCard.visible

    function clampSelection() {
        selected = Math.max(0, Math.min(selected, studio.candidates.length - 1))
    }
    function move(step) {
        if (studio.candidates.length > 0) {
            selected = (selected + step + studio.candidates.length) % studio.candidates.length
            grid.positionViewAtIndex(selected, GridView.Contain)
        }
    }
    function requestQuit() {
        if (studio.anyRunning)
            quitDialog.open()
        else
            win.quitNow()
    }
    function canUse(index) {
        const c = studio.candidates[index]
        return c !== undefined && (c.state === "ready" || c.state === "kept")
    }
    function askVary(index) {
        if (!canUse(index))
            return
        win.selected = index
        varyCard.open()
    }

    Connections {
        target: studio
        function onCandidatesChanged() { win.clampSelection() }
        function onThemeChanged() { win.selected = 0 }
        function onSelectCandidate(index) { win.selected = index; grid.forceActiveFocus() }
    }

    onClosing: (close) => {
        if (studio.anyRunning && !win.quitting) {
            close.accepted = false
            quitDialog.open()
        }
    }

    // Letter keys act on candidates only while nothing is being typed; Enter
    // in the scene field generates.
    readonly property bool keysLive: !typing && !overlayOpen
    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: help.visible ? help.close() : help.open() }
    Shortcut { sequence: "Q"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Space"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.generate() }
    Shortcut { sequences: ["Return", "Enter"]; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.keep(win.selected) }
    Shortcut { sequence: "D"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.discard(win.selected) }
    Shortcut { sequence: "V"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.askVary(win.selected) }
    Shortcut { sequence: "R"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: if (win.canUse(win.selected)) studio.toggleReference(studio.candidates[win.selected].path) }
    Shortcut { sequence: "P"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.preview(win.selected) }
    Shortcut { sequence: "H"; enabled: win.keysLive || history.visible; context: Qt.ApplicationShortcut; onActivated: history.visible ? history.close() : history.open() }
    Shortcut { sequence: "I"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.pickImports() }
    Shortcut { sequence: "C"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.copyPrompt() }
    Shortcut { sequence: "Left"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(-1) }
    Shortcut { sequence: "Right"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(1) }
    Shortcut { sequence: "Up"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(-grid.columns) }
    Shortcut { sequence: "Down"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(grid.columns) }
    Shortcut { sequences: ["/", "Ctrl+L"]; enabled: !win.overlayOpen; context: Qt.ApplicationShortcut; onActivated: sceneField.forceActiveFocus() }
    Shortcut {
        sequence: "Escape"; enabled: !varyCard.visible; context: Qt.ApplicationShortcut
        onActivated: {
            if (help.visible) help.close()
            else if (quitDialog.visible) quitDialog.close()
            else if (removeCard.visible) removeCard.close()
            else if (history.visible) history.close()
            else if (studio.busy) studio.cancel()
            else grid.forceActiveFocus()
        }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        DropArea {
            anchors.fill: parent
            keys: ["text/uri-list"]
            onDropped: (drop) => { studio.importUrls(drop.urls); drop.accept() }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18 * s.spacing
            spacing: 14 * s.spacing

            // Theme, its palette, and how images will be made.
            RowLayout {
                Layout.fillWidth: true
                spacing: win.gap * 1.5

                ODropdown {
                    id: themePicker
                    Layout.preferredWidth: 260 * s.spacing
                    model: studio.themes
                    textRole: "name"
                    valueRole: "slug"
                    Component.onCompleted: currentIndex = indexOfValue(studio.themeSlug)
                    onActivated: studio.loadTheme(currentValue)
                    Connections {
                        target: studio
                        function onThemeChanged() { themePicker.currentIndex = themePicker.indexOfValue(studio.themeSlug) }
                    }
                }

                Row {
                    spacing: 2
                    Repeater {
                        model: studio.palette
                        Rectangle {
                            required property var modelData
                            width: 16 * s.spacing; height: 16 * s.spacing
                            radius: Math.min(3, theme.radius)
                            color: modelData.color
                            border.width: 1
                            border.color: Qt.alpha(s.text, 0.2)
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                OText {
                    text: studio.manual ? qsTr("manual · copy the prompt to any image platform")
                                        : qsTr("generating with %1").arg(studio.backend)
                    color: studio.manual ? s.accent : Qt.alpha(s.text, 0.55)
                }
                OText { text: studio.exportSize; muted: true }
                // What the image agent is doing, for every theme. Click to go
                // to that theme's batch.
                Repeater {
                    model: studio.activity
                    OButton {
                        required property var modelData
                        primary: true
                        text: {
                            const t = modelData.seconds
                            const clock = Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0")
                            const done = modelData.total - modelData.pending
                            return qsTr("%1 · generating %2/%3 · %4").arg(modelData.themeName).arg(done).arg(modelData.total).arg(clock)
                        }
                        onClicked: modelData.theme === studio.themeSlug ? studio.openBatch(modelData.batch)
                                                                      : studio.loadTheme(modelData.theme)
                    }
                }
                OButton {
                    text: qsTr("history %1").arg(studio.history.length)
                    onClicked: history.visible ? history.close() : history.open()
                }
            }

            // References: the theme's backgrounds, then any candidates chosen
            // as references. Clicking toggles.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: win.gap
                OText {
                    text: studio.references.length === 0
                          ? qsTr("Style references · click backgrounds or candidates to use them (optional)")
                          : qsTr("%n reference(s)", "", studio.references.length)
                    muted: studio.references.length === 0
                }
                ListView {
                    id: references
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96 * s.spacing
                    orientation: ListView.Horizontal
                    spacing: win.gap
                    clip: true
                    model: studio.backgrounds
                    delegate: Thumbnail {
                        required property var modelData
                        width: height * 16 / 9
                        height: references.height
                        path: modelData.path
                        chosen: modelData.selected
                        dimmed: studio.references.length > 0 && !modelData.selected
                        caption: modelData.easel ? qsTr("easel") : ""
                        removable: modelData.easel
                        onClicked: studio.toggleReference(modelData.path)
                        onRemoveRequested: { removeCard.path = modelData.path; removeCard.open() }
                    }
                    footer: Row {
                        height: references.height
                        leftPadding: studio.extraReferences.length > 0 ? win.gap : 0
                        spacing: win.gap
                        Repeater {
                            model: studio.extraReferences
                            Thumbnail {
                                required property var modelData
                                width: height * studio.aspect
                                height: references.height
                                path: modelData.path
                                chosen: true
                                caption: qsTr("candidate")
                                onClicked: studio.toggleReference(modelData.path)
                            }
                        }
                    }
                }
            }

            // The scene and the batch size.
            RowLayout {
                Layout.fillWidth: true
                spacing: win.gap
                OField {
                    id: sceneField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Describe a scene: a quiet harbor at dawn, fishing boats, autumn trees")
                    text: studio.scene
                    onTextEdited: studio.scene = text
                    onAccepted: { studio.generate(); grid.forceActiveFocus() }
                    Keys.onEscapePressed: grid.forceActiveFocus()
                }
                OText { text: qsTr("candidates"); muted: true; visible: !studio.manual }
                OStepper {
                    visible: !studio.manual
                    from: 1; to: 6
                    value: studio.count
                    onModified: (value) => studio.count = value
                }
                OButton {
                    primary: true
                    enabled: studio.scene.trim().length > 0 && !studio.busy
                    text: studio.manual ? qsTr("Make prompt") : qsTr("Generate")
                    onClicked: { studio.generate(); grid.forceActiveFocus() }
                }
                OButton {
                    visible: studio.busy
                    text: qsTr("Cancel")
                    onClicked: studio.cancel()
                }
            }

            // Manual mode: the prompt to copy and how to bring images back.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: manualColumn.implicitHeight + 2 * manualColumn.anchors.margins
                visible: studio.manual && studio.manualPrompt.length > 0
                radius: theme.radius
                color: Qt.alpha(s.controlColor, s.normalFill)
                border.width: s.borderWidth
                border.color: Qt.alpha(s.controlColor, s.normalBorderAlpha)
                ColumnLayout {
                    id: manualColumn
                    anchors.fill: parent
                    anchors.margins: 14 * s.spacing
                    spacing: win.gap
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120 * s.spacing
                        TextArea {
                            text: studio.manualPrompt
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            color: s.text
                            selectionColor: Qt.alpha(s.accent, s.selectionFill)
                            font.family: s.fontFamily
                            font.pixelSize: s.fontSize
                            background: null
                        }
                    }
                    OText { Layout.fillWidth: true; text: studio.manualSteps.trim(); wrapMode: Text.Wrap; muted: true }
                    RowLayout {
                        spacing: win.gap
                        OButton { text: qsTr("Copy prompt"); primary: true; onClicked: studio.copyPrompt() }
                        OButton { text: qsTr("Copy reference paths"); visible: studio.references.length > 0; onClicked: studio.copyReferencePaths() }
                        OButton { text: qsTr("Open reference folder"); visible: studio.references.length > 0; onClicked: studio.openReferenceFolder() }
                        OButton { text: qsTr("Import images…"); onClicked: studio.pickImports() }
                        OText { text: qsTr("or drop them onto this window"); muted: true }
                    }
                }
            }

            // The prompt the batch on screen was made from.
            OText {
                Layout.fillWidth: true
                visible: studio.candidates.length > 0 && studio.batchScene !== ""
                text: "“" + studio.batchScene + "”" + (studio.batchChange ? qsTr("  ·  varied: %1").arg(studio.batchChange) : "")
                elide: Text.ElideRight
                muted: true
            }

            // The batch. Columns are chosen so the candidates come out as large
            // as the space allows, down to one per row.
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                focus: true
                model: studio.candidates
                currentIndex: win.selected

                readonly property int columns: {
                    const n = Math.max(1, count)
                    let best = 1, bestWidth = 0
                    for (let c = 1; c <= n; ++c) {
                        const rows = Math.ceil(n / c)
                        const w = Math.min(width / c, height / rows * studio.aspect)
                        if (w > bestWidth + 0.5) { best = c; bestWidth = w }
                    }
                    return best
                }
                readonly property int rows: Math.max(1, Math.ceil(count / columns))
                cellWidth: Math.floor(Math.min(width / columns, height / rows * studio.aspect))
                cellHeight: Math.floor(cellWidth / studio.aspect)
                // Center the grid when the cells don't fill the width.
                leftMargin: Math.max(0, (width - cellWidth * columns) / 2)

                delegate: Item {
                    id: cell
                    required property var modelData
                    required property int index
                    readonly property bool isSelected: index === win.selected
                    readonly property bool hasImage: modelData.state === "ready" || modelData.state === "kept"
                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        id: frame
                        anchors.fill: parent
                        anchors.margins: win.gap / 2
                        radius: theme.radius
                        color: Qt.alpha(s.controlColor, s.normalFill)
                        border.width: cell.isSelected ? 2 : s.borderWidth
                        border.color: cell.isSelected ? s.border : Qt.alpha(s.controlColor, s.normalBorderAlpha)
                        clip: true

                        Image {
                            anchors.fill: parent
                            anchors.margins: parent.border.width
                            visible: cell.hasImage
                            source: visible ? "file://" + modelData.path : ""
                            sourceSize.width: 1600
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                        }
                        OText {
                            anchors.centerIn: parent
                            visible: modelData.state === "pending"
                            text: qsTr("generating") + ".".repeat(dots.count)
                            muted: true
                            Timer { id: dots; property int count: 1; interval: 450; repeat: true; running: parent.visible; onTriggered: count = count % 3 + 1 }
                        }
                        OText {
                            anchors.fill: parent
                            anchors.margins: 16
                            visible: modelData.state === "failed"
                            text: modelData.error
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            color: s.accent
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Row {
                            anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10
                            spacing: 6
                            Badge { visible: modelData.state === "kept"; text: qsTr("kept") }
                            Badge { visible: modelData.reference; text: qsTr("reference") }
                            Badge { visible: studio.previewing === cell.index; text: qsTr("previewing") }
                        }

                        HoverHandler { id: cellHover }
                        TapHandler { onTapped: { win.selected = cell.index; grid.forceActiveFocus() } }
                        TapHandler { onDoubleTapped: studio.preview(cell.index) }

                        // Mouse actions, mirroring the keys, while hovered or selected.
                        Row {
                            anchors.bottom: parent.bottom; anchors.right: parent.right; anchors.margins: 10
                            spacing: 6
                            visible: cell.hasImage && (cellHover.hovered || cell.isSelected)
                            OButton { text: qsTr("keep"); visible: modelData.state === "ready"; onClicked: studio.keep(cell.index); solid: true }
                            OButton { text: qsTr("vary"); onClicked: win.askVary(cell.index); solid: true }
                            OButton { text: modelData.reference ? qsTr("unreference") : qsTr("reference"); onClicked: studio.toggleReference(modelData.path); solid: true }
                            OButton { text: qsTr("preview"); onClicked: studio.preview(cell.index); solid: true }
                            OButton { text: qsTr("discard"); onClicked: studio.discard(cell.index); solid: true }
                        }
                    }
                }

                OText {
                    anchors.centerIn: parent
                    visible: studio.candidates.length === 0
                    text: studio.manual ? qsTr("Describe a scene and press Enter for a prompt, or drop images here.")
                                        : qsTr("Describe a scene and press Enter to generate.")
                    muted: true
                }
            }

            // Status and the keys that matter most.
            RowLayout {
                Layout.fillWidth: true
                OText { Layout.fillWidth: true; text: studio.message; elide: Text.ElideRight }
                OText {
                    text: qsTr("space generate · enter keep · v vary · r reference · p preview · h history · ? keys")
                    muted: true
                }
            }
        }
    }

    component Badge: Rectangle {
        property alias text: label.text
        radius: Math.min(3, theme.radius)
        color: s.background
        border.width: 1
        border.color: s.border
        width: label.implicitWidth + 12; height: label.implicitHeight + 6
        OText { id: label; anchors.centerIn: parent }
    }

    // Overlay cards use the shell's popup surface: popup background with the
    // active-border color, over a dimmed window.
    component Card: Popup {
        anchors.centerIn: parent
        modal: true
        padding: 18 * s.spacing
        Overlay.modal: Rectangle { color: Qt.alpha(s.background, 0.5) }
        background: Rectangle { color: s.popupBackground; radius: theme.radius; border.width: 1; border.color: s.border }
    }

    // Every earlier batch for the theme, with its prompt. Click a candidate to
    // use it as a reference; open a batch to bring it back to the grid.
    Drawer {
        id: history
        edge: Qt.RightEdge
        width: Math.min(win.width * 0.4, 520 * s.spacing)
        height: win.height
        Overlay.modal: Rectangle { color: Qt.alpha(s.background, 0.4) }
        background: Rectangle {
            color: s.popupBackground
            Rectangle { width: 1; height: parent.height; color: s.border }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16 * s.spacing
            spacing: 12 * s.spacing

            RowLayout {
                Layout.fillWidth: true
                OText { Layout.fillWidth: true; text: qsTr("History · %1").arg(studio.themeName); scale: 1.167 }
                OText { text: qsTr("click to reference · double-click to open"); muted: true }
            }
            OText {
                visible: studio.history.length === 0
                text: qsTr("Nothing generated for this theme yet.")
                muted: true
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 18 * s.spacing
                model: studio.history
                ScrollIndicator.vertical: ScrollIndicator {}
                delegate: ColumnLayout {
                    id: batchItem
                    required property var modelData
                    width: ListView.view.width
                    spacing: 6 * s.spacing
                    RowLayout {
                        Layout.fillWidth: true
                        OText {
                            Layout.fillWidth: true
                            text: "“" + (batchItem.modelData.scene || qsTr("untitled")) + "”"
                            wrapMode: Text.Wrap
                        }
                        OButton { text: qsTr("open"); onClicked: { studio.openBatch(batchItem.modelData.batch); history.close() } }
                    }
                    OText {
                        Layout.fillWidth: true
                        text: [Qt.formatDateTime(new Date(batchItem.modelData.createdAt), "MMM d, hh:mm"),
                               batchItem.modelData.backend,
                               batchItem.modelData.change ? qsTr("varied: %1").arg(batchItem.modelData.change) : ""]
                              .filter(x => x).join("  ·  ")
                        wrapMode: Text.Wrap
                        muted: true
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: batchItem.modelData.candidates
                            Thumbnail {
                                required property var modelData
                                width: (batchItem.width - 6) / 2
                                height: width / studio.aspect
                                path: modelData.path
                                chosen: modelData.reference
                                caption: modelData.keptAs ? qsTr("kept") : ""
                                onClicked: studio.toggleReference(modelData.path)
                                TapHandler {
                                    onDoubleTapped: { studio.openBatch(batchItem.modelData.batch, modelData.path); history.close() }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Vary asks what to change. Enter varies with that change; dismissing it
    // varies the usual way, with the candidate as the only style reference.
    Card {
        id: varyCard
        property bool submitted: false
        width: Math.min(win.width * 0.6, 720 * s.spacing)
        onOpened: { submitted = false; varyField.text = ""; varyField.forceActiveFocus() }
        onClosed: {
            studio.vary(win.selected, submitted ? varyField.text : "")
            grid.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12 * s.spacing
            OText { text: qsTr("Vary this candidate"); scale: 1.167 }
            OField {
                id: varyField
                Layout.fillWidth: true
                placeholderText: qsTr("What should change? warmer light, add fog, a boat on the water…")
                onAccepted: { varyCard.submitted = true; varyCard.close() }
            }
            OText {
                Layout.fillWidth: true
                text: qsTr("Enter keeps this image and changes only that. Esc makes fresh takes on the same scene in its style.")
                wrapMode: Text.Wrap
                muted: true
            }
        }
    }

    Card {
        id: help
        ColumnLayout {
            spacing: 6 * s.spacing
            Repeater {
                model: [
                    ["space", qsTr("Generate a batch (or make the prompt in manual mode)")],
                    ["enter", qsTr("Keep the selected candidate")],
                    ["d", qsTr("Discard the selected candidate")],
                    ["v", qsTr("Vary: say what to change, or Esc for fresh takes")],
                    ["r", qsTr("Use the selected candidate as a reference")],
                    ["p", qsTr("Preview on an empty workspace")],
                    ["h", qsTr("History of earlier batches")],
                    ["← → ↑ ↓", qsTr("Move between candidates")],
                    ["/", qsTr("Type a scene")],
                    ["i", qsTr("Import images")],
                    ["c", qsTr("Copy the prompt")],
                    ["esc", qsTr("Close, cancel, or leave the scene field")],
                    ["?", qsTr("Show these keys")],
                    ["q", qsTr("Quit")]
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 16 * s.spacing
                    OText { Layout.preferredWidth: 80 * s.spacing; text: modelData[0]; color: s.accent }
                    OText { text: modelData[1] }
                }
            }
        }
    }

    Card {
        id: removeCard
        property string path
        ColumnLayout {
            spacing: 14 * s.spacing
            OText { text: qsTr("Remove this background?"); scale: 1.333 }
            OText {
                text: qsTr("%1 leaves %2's background cycle. It and its master go to the trash, and the candidate stays in history.")
                      .arg(removeCard.path.split("/").pop()).arg(studio.themeName)
                wrapMode: Text.Wrap
                Layout.maximumWidth: 520 * s.spacing
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: win.gap
                OButton { text: qsTr("Cancel"); onClicked: removeCard.close() }
                OButton { text: qsTr("Remove"); primary: true; onClicked: { studio.removeBackground(removeCard.path); removeCard.close() } }
            }
        }
    }

    Card {
        id: quitDialog
        ColumnLayout {
            spacing: 14 * s.spacing
            OText { text: qsTr("Quit Easel?"); scale: 1.333 }
            OText { text: qsTr("Images are still generating. Quitting stops them.") }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: win.gap
                OButton { text: qsTr("Cancel"); onClicked: quitDialog.close() }
                OButton { text: qsTr("Quit"); primary: true; onClicked: win.quitNow() }
            }
        }
    }
}
