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
#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <QObject>

#include "ModuleInfo.h"

#include <functional>

class ModuleManager : public QObject
{
    Q_OBJECT
    /**
     * This property holds all the registered modules, regardless of being enabled or not.
     */
    Q_PROPERTY(QList<ModuleInfo*> registeredModules READ registeredModules CONSTANT)
    /**
     * This property holds all the module-sections which are enabled and have the type 'send-menu'.
     * \see ModduleSection::SectionType
     */
    Q_PROPERTY(QList<ModuleSection*> sendMenuItems READ sendMenuSections CONSTANT)
public:
    explicit ModuleManager(QObject *parent = nullptr);

    /// This is automatically called by a module at statup to register itself.
    void load(const char *translationUnit, const std::function<ModuleInfo*()> &function);

    QList<ModuleInfo *> registeredModules() const;

    // lists per type
    QList<ModuleSection*> sendMenuSections() const;

private:
    QList<ModuleInfo*> m_modules;
    QList<ModuleSection*> m_sendMenuSections;
    QString m_configFile;
};

#endif
