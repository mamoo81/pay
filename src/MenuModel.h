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
#ifndef MENUMODEL_H
#define MENUMODEL_H

#include <QAbstractListModel>
class ModuleManager;

class MenuModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit MenuModel(ModuleManager *parent);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;


private:
    void initData();

    enum Roles {
        Name,
        Target
    };

    struct MenuItem {
        QString name;
        QString target; // the QML component to load
    };

    QList<MenuItem> m_baseItems;

    // the view we currently present:
    QList<MenuItem> m_data;

    const ModuleManager * const m_moduleManager;
};

#endif
