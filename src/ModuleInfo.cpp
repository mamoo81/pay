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

QString ModuleInfo::title() const
{
    return m_title;
}

void ModuleInfo::setTitle(const QString &newTitle)
{
    m_title = newTitle;
}

QString ModuleInfo::description() const
{
    return m_description;
}

void ModuleInfo::addSection(ModuleSection *section)
{
    // defensive programming here...
    assert(section);
    if (!section)
        return;
    section->setParent(this);
    if (m_sections.contains(section))
        return;
    m_sections.append(section);
    connect (section, SIGNAL(enabledChanged()), this, SIGNAL(enabledChanged()));
}

void ModuleInfo::setDescription(const QString &newDescription)
{
    m_description = newDescription;
}

QList<ModuleSection*> ModuleInfo::sections() const
{
    return m_sections;
}

bool ModuleInfo::enabled() const
{
    for (auto s : m_sections) {
        if (s->enabled())
            return true;
    }
    return false;
}

QString ModuleInfo::iconSource() const
{
    return m_iconSource;
}

void ModuleInfo::setIconSource(const QString &newIconSource)
{
    m_iconSource = newIconSource;
}
