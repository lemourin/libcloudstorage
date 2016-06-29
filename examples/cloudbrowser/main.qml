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

    ScrollView {
        id: browserView
        visible: false
        anchors.fill: parent

        WebView {
            id: browser
            anchors.fill: parent
        }
    }

    ListView {
        id: directory
        anchors.left: cloudView.right
        anchors.top: parent.top
        width: 200
        height: 400
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
                if (window.playing()) {
                    window.stop();
                } else if (!window.goBack()) {
                    directory.focus = false;
                    cloudView.focus = true;
                }
            } else if (event.key === Qt.Key_Return) {
                var t = directoryModel[currentIndex];
                if (t.directory)
                    window.changeCurrentDirectory(t);
                else {
                    window.play(t);
                }
            }
        }
    }

    VideoOutput {
        anchors.fill: parent
        source: window
    }
}
