/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import MuseScore.Ui 1.0
import MuseScore.UiComponents 1.0

ListItemBlank {
    id: root

    property var modelData

    enum IconAndCheckMarkMode {
        None,
        ShowOne,
        ShowBoth
    }

    property int iconAndCheckMarkMode: StyledMenuItem.ShowOne
    property bool reserveSpaceForShortcutOrSubmenuIndicator: itemPrv.hasShortcut || itemPrv.hasSubMenu

    property int padding: 0

    signal handleMenuItem(string itemId)

    signal openSubMenuRequested(var menu)
    signal subMenuShowed(var menu)
    signal subMenuClosed()

    signal requestParentItemActive()

    implicitHeight: 32

    hoveredStateColor: ui.theme.accentColor
    pressedStateColor: ui.theme.accentColor
    enabled: (Boolean(modelData) && modelData.enabled !== undefined) ? Boolean(modelData.enabled) : true // default true

    isSelected: Boolean(itemPrv.showedSubMenu) || (itemPrv.isSelectable && itemPrv.isSelected) || navigation.active

    navigation.name: titleLabel.text
    navigation.accessible.role: MUAccessible.MenuItem
    navigation.accessible.name: {
        var text = titleLabel.text
        if (itemPrv.isCheckable) {
            text += " " + (itemPrv.isChecked ? qsTrc("appshell", "checked") : qsTrc("appshell", "unchecked"))
        } else if (itemPrv.isSelectable) {
            text += " " + (itemPrv.isSelected ? qsTrc("appshell", "selected") : qsTrc("appshell", "not selected"))
        }

        if (itemPrv.hasShortcut) {
            text += " " + itemPrv.shortcut
        }

        if (itemPrv.hasSubMenu) {
            text += " " + qsTrc("appshell", "menu")
        }

        return text.replace('&', '')
    }

    navigation.onNavigationEvent: {
        switch (event.type) {
        case NavigationEvent.Right:
            if (!itemPrv.hasSubMenu) {
                return
            }

            //! NOTE Go to submenu if shown
            if (!itemPrv.showedSubMenu) {
                itemPrv.showSubMenu()
            }

            var focused = itemPrv.showedSubMenu.requestFocus()

            if (focused) {
                event.accepted = true
            }

            break
        case NavigationEvent.Left:
            if (itemPrv.showedSubMenu) {
                itemPrv.closeSubMenu()
            }

            //! NOTE Go to parent item
            requestParentItemActive()
            break
        case NavigationEvent.Up:
        case NavigationEvent.Down:
            if (itemPrv.showedSubMenu) {
                itemPrv.closeSubMenu()
            }

            break
        }
    }

    navigation.onTriggered: root.clicked()

    QtObject {
        id: itemPrv

        property bool hasShortcut: Boolean(modelData) && Boolean(modelData.shortcut)
        property string shortcut: hasShortcut ? modelData.shortcut : ""

        property bool hasSubMenu: Boolean(modelData) && Boolean(modelData.subitems) && modelData.subitems.length > 0
        property var showedSubMenu: null

        property bool isCheckable: Boolean(modelData) && Boolean(modelData.checkable)
        property bool isChecked: isCheckable && Boolean(modelData.checked)

        property bool isSelectable: Boolean(modelData) && Boolean(modelData.selectable)
        property bool isSelected: isSelectable && Boolean(modelData.selected)

        property bool hasIcon: Boolean(modelData) && Boolean(modelData.icon) && modelData.icon !== IconCode.NONE

        function showSubMenu() {
            if (itemPrv.showedSubMenu) {
                return
            }

            var menuComponent = Qt.createComponent("StyledMenu.qml");
            var menu = menuComponent.createObject(root)
            menu.x = root.width
            menu.y = 0

            menu.navigationParentControl = root.navigation
            menu.navigation.name = root.navigation.name+"SubMenu"

            menu.model = modelData.subitems

            menu.handleMenuItem.connect(function(itemId) {
                Qt.callLater(root.handleMenuItem, itemId)
                menu.close()
            })

            menu.closed.connect(function() {
                itemPrv.showedSubMenu = null
                menu.destroy()
                subMenuClosed()
            })

            menu.opened.connect(function() {
                itemPrv.showedSubMenu = menu
                subMenuShowed(menu)
            })

            root.openSubMenuRequested(menu)
        }

        function closeSubMenu() {
            if (itemPrv.showedSubMenu) {
                itemPrv.showedSubMenu.isDoActiveParentOnClose = false
                itemPrv.showedSubMenu.close()
            }
        }
    }

    function calculatedLeftPartWidth() {
        let result = 0

        result += rowLayout.anchors.leftMargin
        if (primaryIconLabel.visible) {
            result += Math.ceil(primaryIconLabel.width)
            result += rowLayout.spacing
        }

        if (secondaryIconLabel.visible) {
            result += Math.ceil(secondaryIconLabel.width)
            result += rowLayout.spacing
        }

        result += Math.ceil(titleLabel.implicitWidth)
        result += rowLayout.spacing // Should theoretically not be necessary, but in practice it is

        return result
    }

    function calculatedRightPartWidth() {
        let result = 0

        if (shortcutLabel.visible) {
            result += rowLayout.spacing
            result += Math.ceil(shortcutLabel.width)
        }

        if (submenuIndicator.visible) {
            result += rowLayout.spacing
            result += Math.ceil(submenuIndicator.width)
        }

        result += rowLayout.anchors.rightMargin

        return result
    }

    RowLayout {
        id: rowLayout

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: submenuIndicator.visible ? 6 : 12

        spacing: 12

        StyledIconLabel {
            id: primaryIconLabel
            Layout.alignment: Qt.AlignLeft
            width: 16
            iconCode: {
                if (root.iconAndCheckMarkMode !== StyledMenuItem.ShowBoth && itemPrv.hasIcon) {
                    return itemPrv.hasIcon ? modelData.icon : IconCode.NONE
                } else if (itemPrv.isCheckable) {
                    return itemPrv.isChecked ? IconCode.TICK_RIGHT_ANGLE : IconCode.NONE
                } else  if (itemPrv.isSelectable) {
                    return itemPrv.isSelected ? IconCode.TICK_RIGHT_ANGLE : IconCode.NONE
                }

                return IconCode.NONE
            }
            visible: !isEmpty || root.iconAndCheckMarkMode !== StyledMenuItem.None
        }

        StyledIconLabel {
            id: secondaryIconLabel
            Layout.alignment: Qt.AlignLeft
            width: 16
            iconCode: itemPrv.hasIcon ? modelData.icon : IconCode.NONE
            visible: root.iconAndCheckMarkMode === StyledMenuItem.ShowBoth
        }

        StyledTextLabel {
            id: titleLabel
            Layout.fillWidth: true
            text: Boolean(modelData) && Boolean(modelData.title) ? modelData.title : ""
            horizontalAlignment: Text.AlignLeft
        }

        StyledTextLabel {
            id: shortcutLabel
            Layout.alignment: Qt.AlignRight
            text: itemPrv.shortcut
            horizontalAlignment: Text.AlignRight
            visible: !itemPrv.hasShortcut || (root.reserveSpaceForShortcutOrSubmenuIndicator)
        }

        StyledIconLabel {
            id: submenuIndicator
            Layout.alignment: Qt.AlignRight
            width: 16
            iconCode: itemPrv.hasSubMenu ? IconCode.SMALL_ARROW_RIGHT : IconCode.NONE
            visible: !isEmpty || (root.reserveSpaceForShortcutOrSubmenuIndicator && !shortcutLabel.visible)
        }
    }

    onHovered: {
        if (!itemPrv.hasSubMenu) {
            return
        }

        if (isHovered) {
            itemPrv.showSubMenu()
        } else {
            var mouseGlogalPos = mapToGlobal(Qt.point(mouseX, mouseY))
            var showedSubMenuGlobalPos = itemPrv.showedSubMenu.contentItem.mapToGlobal(0, 0)

            var eps = 8
            var subMenuWidth = itemPrv.showedSubMenu.width
            var subMenuHeight = itemPrv.showedSubMenu.height

            var isHoveredOnShowedSubMenu = (showedSubMenuGlobalPos.x < mouseGlogalPos.x + eps && mouseGlogalPos.x - eps < showedSubMenuGlobalPos.x + subMenuWidth)
                    && (showedSubMenuGlobalPos.y < mouseGlogalPos.y + eps && mouseGlogalPos.y - eps < showedSubMenuGlobalPos.y + subMenuHeight)

            if (isHoveredOnShowedSubMenu) {
                return
            }

            itemPrv.closeSubMenu()
        }
    }

    onClicked: {
        if (itemPrv.hasSubMenu) {
            itemPrv.showSubMenu()

            itemPrv.showedSubMenu.requestFocus()

            return
        }

        root.handleMenuItem(modelData.id)
    }
}
