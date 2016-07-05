import QtQuick 2.5
import QtWebKit 3.0
import QtQuick.Controls 1.0
import QtMultimedia 5.5

Item {
    id: root

    Gradient {
        id: colors
        GradientStop { position: 0.0; color: "#8EE2FE"}
        GradientStop { position: 0.66; color: "#7ED2EE"}
    }

    ListView {
        id: cloudView
        focus: true
        width: 200
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
            cloudView.focus = true;
        }
        onSuccessfullyAuthorized: {
            directory.visible = true;
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
                return
            var t = directoryModel[currentIndex];
            if (event.key === Qt.Key_Return) {
                if (t.directory)
                    window.changeCurrentDirectory(t);
                else {
                    window.play(t, "stream");
                }
            } else if (event.key === Qt.Key_S)
                window.play(t, "stream");
            else if (event.key === Qt.Key_M)
                window.play(t, "memory");
            else if (event.key === Qt.Key_F)
                window.play(t, "file");
            else if (event.key === Qt.Key_P) {
                window.stop();
            } else if (event.key === Qt.Key_L)
                window.play(t, "link");
        }
    }

    VideoOutput {
        anchors.fill: parent
        source: window
    }

    Rectangle {
        property int padding: 5

        anchors.margins: padding
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        border.color: "black"
        radius: padding
        color: "grey"
        width: content.width + 2 * padding
        height: content.height + 2 * padding

        Row {
            id: content
            x: parent.padding
            y: parent.padding

            Column {
                width: 20
                Row { Text { text: "F" } }
                Row { Text { text: "M" } }
                Row { Text { text: "S" } }
                Row { Text { text: "P" } }
                Row { Text { text: "L" } }
            }
            Column {
                Row { Text { text: "Download a file" } }
                Row { Text { text: "Download file to memory" } }
                Row { Text { text: "Stream a file" } }
                Row { Text { text: "Pause file" } }
                Row { Text { text: "Play from link"} }
            }
        }
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
