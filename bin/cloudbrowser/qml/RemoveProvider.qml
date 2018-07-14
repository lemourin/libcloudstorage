import QtQuick 2.7
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.1 as Kirigami

Kirigami.ScrollablePage {
  id: page
  anchors.fill: parent
  title: "Remove Provider"

  ListView {
    id: list
    model: cloud.userProviders
    delegate: Kirigami.BasicListItem {
      reserveSpaceForIcon: false
      backgroundColor: ListView.isCurrentItem ? Kirigami.Theme.highlightColor :
                                                Kirigami.Theme.backgroundColor
      height: 60
      onClicked: {
        list.currentIndex = index;
        cloud.removeProvider(modelData);
      }
      contentItem: ProviderEntry {
        image_width: 120
        image_height: 35
        label: modelData.label
        provider: modelData.type
      }
    }
  }
}
