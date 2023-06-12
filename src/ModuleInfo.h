/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
#ifndef MODULE_INFO_H
#define MODULE_INFO_H

#include <QObject>

#include "ModuleSection.h"

class ModuleInfo : public QObject
{
    Q_OBJECT
public:
    explicit ModuleInfo(QObject *parent = nullptr);

    QString id() const;
    void setId(const QString &newId);

    /**
     * A module ships its own translations, to make the wallet load
     * those translations you need to put them into the QRC system
     * and then let us know the base of the filenames you picked
     * for them.
     * Example: "module-build-transaction"
     */
    void setTranslationUnit(const QString &name);
    /// returns the set translation unit, or empty string if none
    QString translationUnit() const;

private:
    QString m_id;
    QString m_title;
    QString m_description;
    QString m_translationUnit;

    QList<ModuleSection> m_sections;
};

#endif
