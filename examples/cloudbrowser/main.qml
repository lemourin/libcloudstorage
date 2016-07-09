import QtQuick 2.5
import QtWebKit 3.0
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

    ListView {
        id: cloudView
        focus: true
        width: help.width + padding
        height: parent.height
        model: cloudModel
        delegate: Component {
            Text {
                text: modelData
                font.pixelSize: 25
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
   Connections {
        target: window
        onOpenBrowser: {
            browserView.visible = true;
            browser.url = url;
        }
        onCloseBrowser: {
            browserView.visible = false;
            if (!directory.focus) cloudView.focus = true;
        }
        onAddedItem: {
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
        onShowPlayer: {
            directory.focus = false;
        }
        onHidePlayer: {
            directory.focus = true;
        }
    }

    ListView {
        id: directory
        anchors.left: cloudView.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        model: directoryModel
        delegate: Component {
            Item {
                property int thumbnailWidth: 50
                property int thumbnailHeight: 50
                property int padding: 5
                property var view: ListView.view

                id: fileEntry
                height: thumbnailHeight + 2 * padding
                width: row.width + 2 * padding

                Row {
                    id: row
                    x: padding
                    y: padding
                    Image {
                        width: fileEntry.thumbnailWidth
                        height: fileEntry.thumbnailHeight
                        source: thumbnail
                    }
                    Item {
                        height: fileEntry.thumbnailHeight
                        width: text.width + fileEntry.padding
                        Text {
                            x: fileEntry.padding
                            id: text
                            anchors.verticalCenter: parent.verticalCenter
                            text: name
                        }
                    }
                }
            }
        }
        highlight: Rectangle {
            color: "lightgray"
        }

        Keys.onPressed: {
            if (event.key === Qt.Key_Backspace) {
                if (!window.goBack()) {
                    directory.focus = false;
                    cloudView.focus = true;
                }
            }
            if (currentIndex == -1)
                return;
            var t = directoryModel[currentIndex];
            if (event.key === Qt.Key_Return) {
                if (t.directory)
                    window.changeCurrentDirectory(t);
                else
                    window.play(t);
            } else if (event.key === Qt.Key_D) {
                downloadFileDialog.visible = true;
                downloadFileDialog.file = t;
                downloadFileDialog.open();
            } else if (event.key === Qt.Key_F5)
                window.listDirectory();
            else if (event.key === Qt.Key_Q)
                window.stop();
        }
        FileDialog {
            property var file

            id: downloadFileDialog
            selectFolder: true
            onAccepted: window.downloadFile(file, fileUrl)
        }
    }

    MultiPointTouchArea {
        visible: directory.focus && directory.currentIndex != -1
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 100
        height: 100
        Rectangle {
            anchors.fill: parent
            anchors.margins: 5
            border.color: "black"
            color: "grey"
            radius: 5
            Text {
                anchors.centerIn: parent
                text: "Upload file"
            }
        }
        FileDialog {
            id: fileDialog
            onAccepted: window.uploadFile(fileDialog.fileUrl)
        }
        onPressed: fileDialog.visible = true
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
                width: 20
                Row { Text { text: "D" } }
                Row { Text { text: "Q" } }
                Row { Text { text: "P" } }
            }
            Column {
                Row { Text { text: "Download a file" } }
                Row { Text { text: "Quit a file" } }
                Row { Text { text: "Pause file" } }
            }
        }
    }

    ProgressBar {
        id: uploadProgress
        visible: false
        anchors.bottom: help.top
        anchors.horizontalCenter: help.horizontalCenter
    }

    ProgressBar {
        id: downloadProgress
        visible: false
        anchors.bottom: uploadProgress.top
        anchors.horizontalCenter: uploadProgress.horizontalCenter
    }

    ScrollView {
        id: browserView
        visible: false
        anchors.fill: parent

        WebView {
            id: browser
            anchors.fill: parent
        }
    }

}
