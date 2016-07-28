import QtQuick 2.5
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.2

Item {
    property int padding: 5

    id: root

    Rectangle {
        anchors.fill: parent
        color: "white"
    }

    Gradient {
        id: colors
        GradientStop { position: 0.0; color: "#8EE2FE" }
        GradientStop { position: 0.66; color: "#7ED2EE" }
    }

    Item {
        id: leftSide
        width: childrenRect.width
        height: parent.height
        ListView {
            id: cloudView
            focus: true
            anchors.top: parent.top
            anchors.bottom: help.top
            width: 0.3 * root.width
            model: cloudModel
            clip: true
            delegate: Component {
                MouseArea {
                    property string text: modelData

                    width: cloudView.width
                    height: cloudView.height * 0.15
                    Text {
                        anchors.centerIn: parent
                        fontSizeMode: Text.Fit
                        text: modelData
                        font.pointSize: 32
                    }
                    onClicked: {
                        cloudView.currentIndex = index;
                        cloudView.focus = false;
                        window.initializeCloud(modelData);
                    }
                }
            }
            highlight: Rectangle {
                color: "lightgray"
            }

            Keys.onPressed: {
                if (event.key === Qt.Key_Return) {
                    window.initializeCloud(currentItem.text);
                    cloudView.focus = false;
                }
            }
        }
        Rectangle {
            id: help
            anchors.margins: padding
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            border.color: "black"
            radius: padding
            color: "grey"
            width: content.width + 2 * padding
            height: content.height + 2 * padding

            Row {
                id: content
                x: padding
                y: padding

                Column {
                    width: childrenRect.width + padding
                    Row { Text { text: "D" } }
                    Row { Text { text: "Q" } }
                    Row { Text { text: "P" } }
                    Row { Text { text: "DEL" } }
                    Row { Text { text: "M" } }
                    Row { Text { text: "F5" } }
                }
                Column {
                    Row { Text { text: "Download a file" } }
                    Row { Text { text: "Quit a file" } }
                    Row { Text { text: "Pause file" } }
                    Row { Text { text: "Delete file" } }
                    Row { Text { text: "Move file" } }
                    Row { Text { text: "Refresh" } }
                }
            }
        }
        ProgressBar {
            id: uploadProgress
            visible: false
            anchors.bottom: help.top
            anchors.right: help.right
            anchors.left: parent.left
        }

        ProgressBar {
            id: downloadProgress
            visible: false
            anchors.bottom: uploadProgress.top
            anchors.right: help.right
            anchors.left: parent.left
        }
    }

    Connections {
        target: window
        onOpenBrowser: {
            browser.visible = true;
            browser.url = url;
        }
        onCloseBrowser: {
            if (browser.visible) directory.focus = true;
            browser.visible = false;
        }
        onRunListDirectory: {
            directory.visible = true;
            directory.focus = true;
        }
        onUploadProgressChanged: {
            uploadProgress.visible = total != 0;
            uploadProgress.value = now / total;
        }
        onDownloadProgressChanged: {
            downloadProgress.visible = total != 0;
            downloadProgress.value = now / total;
        }
        onCurrentItemChanged: {
            directory.currentIndex = index;
        }
        onPlayQmlPlayer: player.item.mediaplayer.play()
        onStopQmlPlayer: player.item.mediaplayer.stop()
        onPauseQmlPlayer: {
            if (player.item.mediaplayer.playbackState ===
                    MediaPlayer.PausedState)
                player.item.mediaplayer.play();
            else
                player.item.mediaplayer.pause();
        }
        onShowQmlPlayer: player.item.videooutput.visible = true
        onHideQmlPlayer: player.item.videooutput.visible = false
    }

    ListView {
        id: directory
        anchors.left: leftSide.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: buttons.top
        anchors.margins: padding
        model: directoryModel
        clip: true
        delegate: Component {
            MouseArea {
                property int thumbnailWidth: thumbnailHeight
                property int thumbnailHeight: root.height * 0.1
                property int padding: root.padding
                property bool isDirectory: display.is_directory
                property string name: display.name

                id: fileEntry
                height: thumbnailHeight + 2 * padding
                width: directory.width
                onClicked: {
                    directory.currentIndex = index;
                    if (isDirectory)
                        window.changeCurrentDirectory(index);
                    else
                        window.play(index);
                }

                Row {
                    id: row
                    x: padding
                    y: padding
                    Item {
                        width: fileEntry.thumbnailWidth
                        height: fileEntry.thumbnailHeight
                        Image {
                            anchors.centerIn: parent
                            width: parent.width - 2 * padding
                            height: parent.height - 2 * padding
                            source: display.icon
                        }
                    }
                    Image {
                        width: fileEntry.thumbnailWidth
                        height: fileEntry.thumbnailHeight
                        source: display.thumbnail
                    }
                    Item {
                        height: fileEntry.thumbnailHeight
                        width: fileEntry.width -
                               2 * (fileEntry.thumbnailWidth + padding)
                        Text {
                            x: fileEntry.padding
                            id: text
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: contentHeight
                            text: display.name
                            fontSizeMode: Text.Fit
                            font.pointSize: 16
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
        highlight: Rectangle {
            color: "lightgray"
        }

        Keys.onPressed: {
            if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Back) {
                if (!window.goBack()) {
                    directory.focus = false;
                    cloudView.focus = true;
                    browser.visible = false;
                }
            }
            else if (event.key === Qt.Key_Return && currentIndex != -1) {
                if (currentItem.isDirectory)
                    window.changeCurrentDirectory(currentIndex);
                else
                    window.play(currentIndex);
            } else if (event.key === Qt.Key_D && currentIndex != -1) {
                downloadFileDialog.visible = true;
                downloadFileDialog.file = currentIndex;
                downloadFileDialog.open();
            } else if (event.key === Qt.Key_F5)
                window.listDirectory();
            else if (event.key === Qt.Key_Delete && currentIndex != -1)
                window.deleteItem(currentIndex);
            else if (event.key === Qt.Key_M) {
                window.markMovedItem(currentIndex)
            }
        }
        FileDialog {
            property var file

            id: downloadFileDialog
            selectFolder: true
            onAccepted: window.downloadFile(file, fileUrl)
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: {
            if (!cloudView.focus)
                directory.focus = true;
            mouse.accepted = false;
        }
    }

    Item {
        id: buttons
        anchors.left: leftSide.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        Component.onCompleted: height = childrenRect.height
        ActionButton {
            id: goBackButton
            visible: uploadButton.visible
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            text: "Go back"
            onClicked: {
                if (!window.goBack()) {
                    directory.focus = false;
                    cloudView.focus = true;
                }
            }
        }

        ActionButton {
            id: uploadButton
            visible: !cloudView.focus
            anchors.bottom: parent.bottom
            anchors.right: goBackButton.left
            text: "Upload file"
            FileDialog {
                id: fileDialog
                onAccepted: window.uploadFile(fileDialog.fileUrl)
            }
            onClicked: fileDialog.visible = true
        }

        ActionTextButton {
            id: createDirectoryButton
            visible: uploadButton.visible
            anchors.bottom: parent.bottom
            anchors.right: uploadButton.left
            text: "Create\ndirectory"
            placeholdertext: "Name"
            onClicked: window.createDirectory(displaytext)
        }

        ActionTextButton {
            id: renameButton
            visible: uploadButton.visible
            anchors.bottom: parent.bottom
            anchors.right: createDirectoryButton.left
            text: "Change\nname"
            textfield: directory.currentItem ? directory.currentItem.name : ""
            onClicked: window.renameItem(directory.currentIndex, displaytext)
        }

        ActionButton {
            visible: window.movedItem != ""
            anchors.margins: padding
            anchors.right: renameButton.left
            anchors.bottom: parent.bottom
            text: "Moving " + window.movedItem
        }
    }

    Loader {
        id: player
        anchors.fill: parent
        source: "MediaPlayer.qml"
    }

    MouseArea {
        property string url

        id: browser
        visible: false
        anchors.fill: parent
        enabled: visible
        onUrlChanged: webview.item.url = url
        Rectangle {
            anchors.fill: parent
            color: "green"
        }
        Text {
            anchors.centerIn: parent
            text: "<a href=\"" + browser.url + "\">Authorize library</a>"
            wrapMode: Text.Wrap
            onLinkActivated: Qt.openUrlExternally(link)
        }
        Loader {
            id: webview
            source: "WebView.qml"
            anchors.fill: parent
        }
    }
}
