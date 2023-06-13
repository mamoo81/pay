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
#ifndef MODULE_SECTION_H
#define MODULE_SECTION_H

#include <QObject>

/**
 * The module section represts a single Hook into the Flowee Pay user interface.
 * This class fits in the ModuleInfo and ModuleManager framework for modules.
 *
 * A module can fit into one or more placs in the UI and the user can enable those
 * separately in the modules overview.
 */
class ModuleSection : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text CONSTANT)
    Q_PROPERTY(QString subtext READ subtext CONSTANT)
    Q_PROPERTY(QString qml READ startQMLFile CONSTANT)
    Q_PROPERTY(bool isSendMethod READ isSendMethod CONSTANT)
public:
    /// The placement in the main app of this section.
    enum SectionType {
        SendMethod,     //< A specific way to send coin, shown in list of send-methods.
    };

    explicit ModuleSection(SectionType type, QObject *parent = nullptr);

    /**
     * This text is used on in the chosen UI-type to select this module.
     */
    void setText(const QString &newText);
    QString text() const;

    /// This text is the alt-text
    void setSubtext(const QString &newSubtext);
    QString subtext() const;

    /**
     * This section may only show if another module is enabled,
     * name that module (by id) here.
     * \sa ModuleInfo::id()
     */
    void setRequiredModules(const QStringList &modules);
    QStringList requiredModules() const;

    /**
     * The QML file that should be loaded to invoke this module.
     */
    void setStartQMLFile(const QString &filename);
    QString startQMLFile() const;

    SectionType type() const;
    bool isSendMethod() const {
        return m_type == SendMethod;
    }

    bool enabled() const;
    /**
     * This is set by the user or restored from the save file upon restart.
     */
    void setEnabled(bool on);

signals:
    void enabledChanged();

private:
    const SectionType m_type;
    QString m_text;
    QString m_subtext;
    QString m_startQMLfile;
    bool m_enabled = true;

    QStringList m_requiredModules;
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
};

#endif
