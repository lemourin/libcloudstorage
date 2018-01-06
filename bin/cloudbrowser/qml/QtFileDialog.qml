import QtQuick 2.0
import Qt.labs.platform 1.0

Item {
  property string filename
  property string extension
  property string url
  property bool selectExisting

  signal ready()

  function open() {
    dialog.currentFile = cloud.home() + "/" + filename + (selectExisting ? "cloudbrowser" : "");
    var idx = filename.lastIndexOf('.');
    if (idx !== -1)
      extension = filename.substring(idx + 1);
    else
      extension = "";
    dialog.open();
  }

  FileDialog {
    id: dialog
    fileMode: selectExisting ? FileDialog.OpenFile : FileDialog.SaveFile
    nameFilters: selectExisting ? [] : ["(*." + extension + ")"]
    onAccepted: {
      url = file;
      filename = url.substring(url.lastIndexOf('/') + 1);
      ready();
    }
  }
}
