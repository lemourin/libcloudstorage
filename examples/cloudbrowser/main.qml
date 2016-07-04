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
            Text {
                text: name
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
                if (window.playing())
                    window.stop();
            } else if (event.key === Qt.Key_L)
                window.play(t, "link");
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 100
        height: 100

        Row {
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

    VideoOutput {
        anchors.fill: parent
        source: window
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
