import QtQuick 2.0
import QtMultimedia 5.6

Item {
    property MediaPlayer mediaplayer: mediaPlayer
    property VideoOutput videooutput: videoOutput

    MediaPlayer {
        id: mediaPlayer
        source: window.currentMedia
        onStatusChanged: {
            if (status == MediaPlayer.EndOfMedia) {
                videoOutput.visible = false;
                window.playNext();
            } else if (status == MediaPlayer.InvalidMedia)
                window.hidePlayer();
        }
    }
    VideoOutput {
        MouseArea {
            anchors.fill: parent
        }
        Rectangle {
            anchors.fill: parent
            color: "black"
            z: -1
        }
        id: videoOutput
        source: mediaPlayer
        anchors.fill: parent
        visible: false
    }

}
