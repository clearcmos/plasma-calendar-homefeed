// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid
import plasma.applet.io.github.clearcmos.nextevent

PlasmoidItem {
    id: root

    NextEvent {
        id: next
    }

    preferredRepresentation: compactRepresentation
    // Never shown, but without one Plasma does not create the compact view.
    fullRepresentation: Item {}
    toolTipMainText: next.valid ? next.title : "No upcoming events"
    toolTipSubText: next.location.length > 0 ? next.when + "\n" + next.location : next.when

    compactRepresentation: RowLayout {
        id: row

        // Room for any ordinary title; a runaway one is elided instead of
        // pushing the rest of the panel aside.
        readonly property real cap: Kirigami.Units.gridUnit * 40

        spacing: Kirigami.Units.smallSpacing
        Layout.minimumWidth: Math.min(row.implicitWidth, row.cap)
        Layout.preferredWidth: Layout.minimumWidth
        Layout.maximumWidth: Layout.minimumWidth

        Kirigami.Icon {
            source: Plasmoid.icon
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
        }

        PlasmaComponents.Label {
            visible: next.valid
            text: next.title
            elide: Text.ElideRight
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
        }

        PlasmaComponents.Label {
            visible: next.valid
            text: next.when
            opacity: 0.75
            textFormat: Text.PlainText
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
