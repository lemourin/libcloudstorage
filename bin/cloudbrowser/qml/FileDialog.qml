import QtQuick 2.0

Loader {
  property bool selectFolder
  property bool selectExisting
  property string filename
  property string fileUrl

  signal accepted()

  id: loader
  source: android ? "AndroidFileDialog.qml" : "QtFileDialog.qml"
  asynchronous: false

  onLoaded: {
    item.selectFolder = Qt.binding(function() { return selectFolder; });
    item.selectExisting = Qt.binding(function() { return selectExisting; });
    item.filename = Qt.binding(function() { return filename; });
  }

  Connections {
    target: loader.item
    onReady: {
      loader.fileUrl = target.url;
      loader.filename = target.filename;
      accepted();
    }
  }

  function open() {
    item.open();
  }
}
