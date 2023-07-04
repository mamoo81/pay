/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "MenuModel.h"
#include "ModuleManager.h"
#include "ModuleSection.h"

MenuModel::MenuModel(ModuleManager *mm)
    : QAbstractListModel{mm},
    m_moduleManager(mm)
{
    assert(mm);
    m_baseItems.append({tr("Explore"), "./ExploreModules.qml"});
    m_baseItems.append({tr("Settings"), "./GuiSettings.qml"});
    m_baseItems.append({tr("Security"), "./LockApplication.qml"});
    m_baseItems.append({tr("About"), "./About.qml"});
    m_baseItems.append({tr("Wallets"), "AccountsList.qml"});
    initData();

    connect (m_moduleManager, &ModuleManager::mainMenuSectionsChanged, this, [=]() {
        initData();
    });
}

int MenuModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;

    return m_data.size();
}

QVariant MenuModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    assert(index.row() >= 0);
    assert(m_data.size() > index.row());
    const auto &item = m_data.at(index.row());

    switch (role) {
    case Name:
        return item.name;
    case Target:
        return item.target;
    }
    return QVariant();
}

QHash<int, QByteArray> MenuModel::roleNames() const
{
    QHash<int, QByteArray> answer;
    answer[Name] = "name";
    answer[Target] = "target";
    return answer;
}

void MenuModel::initData()
{
    if (!m_data.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_data.size() - 1);
        m_data.clear();
        endRemoveRows();
    }
    m_data.append(m_baseItems.at(0));   // explore
    m_data.append(m_baseItems.at(1));   // settings
    m_data.append(m_baseItems.at(2));   // security

    // from plugins
    for (auto module : m_moduleManager->maindMenuSections()) {
        m_data.append( { module->text(), module->startQMLFile() } );
    }

    // and the rest.
    for (int i = 3; i < m_baseItems.size(); ++i)
        m_data.append(m_baseItems.at(i));

    beginInsertRows(QModelIndex(), 0, m_data.size() - 1);
    endInsertRows();
}
