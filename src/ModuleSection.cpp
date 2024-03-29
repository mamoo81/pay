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
#include "ModuleSection.h"

ModuleSection::ModuleSection(SectionType type, QObject *parent)
    : QObject(parent),
    m_type(type)
{
}

QString ModuleSection::text() const
{
    return m_text;
}

QString ModuleSection::subtext() const
{
    return m_subtext;
}

QStringList ModuleSection::requiredModules() const
{
    return m_requiredModules;
}

QString ModuleSection::startQMLFile() const
{
    return m_startQMLfile;
}

ModuleSection::SectionType ModuleSection::type() const
{
    return m_type;
}

bool ModuleSection::enabled() const
{
    return m_enabled;
}

void ModuleSection::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    emit enabledChanged();
}

void ModuleSection::setStartQMLFile(const QString &filename)
{
    m_startQMLfile = filename;
}

void ModuleSection::setRequiredModules(const QStringList &modules)
{
    m_requiredModules = modules;
}

void ModuleSection::setSubtext(const QString &newSubtext)
{
    m_subtext = newSubtext;
}

void ModuleSection::setText(const QString &newText)
{
    m_text = newText;
}
