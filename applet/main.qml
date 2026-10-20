// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid
import plasma.applet.io.github.clearcmos.nextevent

PlasmoidItem {
    id: root

    NextEvent {
        id: next
    }

    preferredRepresentation: compactRepresentation
    toolTipMainText: next.valid ? next.title : "No upcoming events"
    toolTipSubText: next.location.length > 0 ? next.when + "\n" + next.location : next.when

    compactRepresentation: MouseArea {
        id: compact

        // Room for any ordinary line; a runaway title is elided instead of
        // pushing the rest of the panel aside.
        readonly property real cap: Kirigami.Units.gridUnit * 50
        property bool wasExpanded: false

        Layout.minimumWidth: Math.min(row.implicitWidth, compact.cap)
        Layout.preferredWidth: Layout.minimumWidth
        Layout.maximumWidth: Layout.minimumWidth
        onPressed: compact.wasExpanded = root.expanded
        onClicked: root.expanded = !compact.wasExpanded

        RowLayout {
            id: row

            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

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

            Repeater {
                model: next.departures

                delegate: RowLayout {
                    id: departure

                    required property var modelData

                    spacing: 0
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.fillHeight: true

                    PlasmaComponents.Label {
                        text: departure.modelData.text
                        color: departure.modelData.late ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                        textFormat: Text.PlainText
                        Layout.fillHeight: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    // Takes its own click, so the popup is left as it was.
                    PlasmaCore.ToolTipArea {
                        visible: next.canSend
                        mainText: "Send " + departure.modelData.mode + " trip to phone"
                        implicitWidth: send.implicitWidth
                        implicitHeight: send.implicitHeight
                        Layout.alignment: Qt.AlignVCenter

                        PlasmaComponents.ToolButton {
                            id: send

                            anchors.fill: parent
                            text: "Send " + departure.modelData.mode + " trip to phone"
                            display: QQC2.AbstractButton.IconOnly
                            // Otherwise the last one starts with a focus frame.
                            focusPolicy: Qt.NoFocus
                            icon.name: "smartphone-symbolic"
                            icon.width: Kirigami.Units.iconSizes.small
                            icon.height: Kirigami.Units.iconSizes.small
                            onClicked: next.send(departure.modelData.mode)
                        }
                    }
                }
            }
        }
    }

    fullRepresentation: PlasmaExtras.Representation {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20
        Layout.preferredWidth: Kirigami.Units.gridUnit * 26
        Layout.preferredHeight: Kirigami.Units.gridUnit * 28
        collapseMarginsHint: true

        header: PlasmaExtras.PlasmoidHeading {
            contentItem: PlasmaExtras.Heading {
                level: 1
                text: "Upcoming"
            }
        }

        // Wide enough for any row's time, so titles line up.
        TextMetrics {
            id: clockWidth

            font: Kirigami.Theme.defaultFont
            text: new Date(2000, 0, 1, 12, 0).toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
        }

        TextMetrics {
            id: allDayWidth

            font: Kirigami.Theme.defaultFont
            text: "all day"
        }

        contentItem: PlasmaComponents.ScrollView {
            ListView {
                id: list

                model: next.upcoming
                clip: true

                footer: ColumnLayout {
                    width: ListView.view.width
                    spacing: 0
                    visible: next.notices.length > 0

                    PlasmaExtras.ListSectionHeader {
                        text: "Notices"
                        Layout.fillWidth: true
                    }

                    Repeater {
                        model: next.notices

                        delegate: PlasmaComponents.Label {
                            required property string modelData

                            text: modelData
                            font: Kirigami.Theme.smallFont
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                            Layout.rightMargin: Kirigami.Units.largeSpacing
                            Layout.topMargin: Kirigami.Units.smallSpacing
                        }
                    }
                }

                PlasmaExtras.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: list.count === 0
                    iconName: "view-calendar-upcoming-events"
                    text: "No upcoming events"
                }

                delegate: ColumnLayout {
                    id: entry

                    required property var modelData

                    width: ListView.view.width
                    spacing: 0

                    PlasmaExtras.ListSectionHeader {
                        visible: entry.modelData.heading.length > 0
                        text: entry.modelData.heading
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        Layout.bottomMargin: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            text: entry.modelData.time
                            opacity: 0.75
                            textFormat: Text.PlainText
                            Layout.preferredWidth: Math.max(clockWidth.advanceWidth, allDayWidth.advanceWidth)
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true

                            PlasmaComponents.Label {
                                text: entry.modelData.title
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                            }

                            Repeater {
                                model: entry.modelData.lines

                                delegate: PlasmaComponents.Label {
                                    required property string modelData

                                    text: modelData
                                    opacity: 0.75
                                    font: Kirigami.Theme.smallFont
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                }
                            }

                            Repeater {
                                model: entry.modelData.departures

                                delegate: PlasmaComponents.Label {
                                    required property var modelData

                                    text: modelData.summary
                                    color: modelData.late ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                                    font: Kirigami.Theme.smallFont
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
