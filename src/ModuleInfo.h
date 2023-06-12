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

/**
 * This is part of the Flowee Pay Modules section.
 *
 * A module is an optional product that is shipped in the Flowee Pay application
 * in a way that it can be enabled or disabled by the user.
 * The ModuleInfo is the metadata about the module, data that is shown to the user
 * to allow them to select if they like to enable this module or not.
 */
class ModuleInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QList<ModuleSection*> sections READ sections CONSTANT)
public:
    explicit ModuleInfo(QObject *parent = nullptr);

    QString id() const;
    void setId(const QString &newId);

    /**
     * >The title as shown in the module selector UI.
     */
    void setTitle(const QString &newTitle);
    QString title() const;

    /**
     * >The description of this module, as shown in the module selector UI.
     */
    void setDescription(const QString &newDescription);
    QString description() const;

    /**
     * Sections define where a module is used, add them to "plug" your module into the UI
     */
    void addSection(ModuleSection *section);
    QList<ModuleSection *> sections() const;

    bool enabled() const;
    void setEnabled(bool newEnabled);

signals:
    void enabledChanged();

private:
    QString m_id;
    QString m_title;
    QString m_description;
    QString m_translationUnit;
    bool m_enabled = true;

    QList<ModuleSection*> m_sections;
};

#endif
