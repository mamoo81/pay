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

MenuModel::MenuModel(QObject *parent)
    : QAbstractListModel{parent},
      m_current(&m_root)
{
    m_root.children.append({tr("Settings"), "./GuiSettings.qml", {}});
    m_root.children.append({tr("Network Details"), "./NetView.qml", {}});
    m_root.children.append({tr("About"), "./About.qml", {}});
    m_root.children.append({tr("Wallets"), "AccountsList.qml", {}});
    /*
    m_root.children.append({tr("Settings"), "", {
            { tr("Security"), "", {} },
            { tr("Settings 2"), "", {} },
    }}); */

}

int MenuModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // only for the (invalid) root node we return a count, since this is a list not a tree
        return 0;

    assert(m_current);
    return m_current->children.size();
}

QVariant MenuModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    assert(index.row() >= 0);
    assert(m_current);
    assert(m_current->children.size() > index.row());
    const auto &item = m_current->children.at(index.row());

    switch (role) {
    case Name:
        return item.name;
    case Target:
        return item.target;
    case HasChildren:
        return !item.children.isEmpty();
    }
    return QVariant();
}

QHash<int, QByteArray> MenuModel::roleNames() const
{
    QHash<int, QByteArray> answer;
    answer[Name] = "name";
    answer[Target] = "target";
    answer[HasChildren] = "hasChildren";
    return answer;
}
