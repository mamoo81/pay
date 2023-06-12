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
#include "ModuleInfo.h"

ModuleInfo::ModuleInfo(QObject *parent)
    : QObject(parent)
{
}

QString ModuleInfo::id() const
{
    return m_id;
}

void ModuleInfo::setId(const QString &newId)
{
    m_id = newId;
}

QString ModuleInfo::translationUnit() const
{
    return m_translationUnit;
}

void ModuleInfo::setTranslationUnit(const QString &name)
{
    m_translationUnit = name;
}
