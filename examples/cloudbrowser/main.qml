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
                property bool isDirectory: display.is_directory

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
                        source: display.thumbnail
                    }
                    Item {
                        height: fileEntry.thumbnailHeight
                        width: text.width + fileEntry.padding
                        Text {
                            x: fileEntry.padding
                            id: text
                            anchors.verticalCenter: parent.verticalCenter
                            text: display.name
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
            if (event.key === Qt.Key_Return && currentIndex != -1) {
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

    MouseArea {
        id: uploadButton
        visible: !cloudView.focus
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 100
        height: 100
        Rectangle {
            anchors.fill: parent
            anchors.margins: padding
            border.color: "black"
            color: "grey"
            radius: padding
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

    MouseArea {
        id: createDirectoryButton
        visible: uploadButton.visible
        width: 100
        height: 100
        anchors.bottom: parent.bottom
        anchors.right: uploadButton.left
        Rectangle {
            anchors.fill: parent
            anchors.margins: padding
            border.color: "black"
            color: "grey"
            radius: padding
            Text {
                id: createDirectoryText
                y: 20
                anchors.left: parent.left
                anchors.right: parent.right
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: "Create directory"
            }
            TextField {
                id: directoryName
                placeholderText: "Name"
                anchors.top: createDirectoryText.bottom
                anchors.left: parent.left
                anchors.right: parent.right
            }
        }
        onPressed: window.createDirectory(directoryName.text)
    }

    Rectangle {
        visible: window.movedItem != ""
        anchors.margins: padding
        anchors.right: createDirectoryButton.left
        anchors.bottom: parent.bottom
        width: 100
        height: childrenRect.height + 2 * padding
        border.color: "black"
        color: "grey"
        radius: padding
        Text {
            x: padding
            y: padding
            wrapMode: Text.Wrap
            anchors.left: parent.left
            anchors.right: parent.right
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Moving " + window.movedItem
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
            }
            Column {
                Row { Text { text: "Download a file" } }
                Row { Text { text: "Quit a file" } }
                Row { Text { text: "Pause file" } }
                Row { Text { text: "Delete file" } }
                Row { Text { text: "Move file" } }
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

    WebView {
        id: browser
        visible: false
        anchors.fill: parent
    }
}
