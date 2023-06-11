#ifndef MODULE_SECTION_H
#define MODULE_SECTION_H

#include <QObject>

class ModuleSection : public QObject
{
    Q_OBJECT
public:
    explicit ModuleSection(QObject *parent = nullptr);

/*
 * some info about how this section can be plugged into the UI.
 */

private:
    /// generic texts, for the UI button to activates this module.
    QString m_text;
    QString m_subtext;

    QStringList m_requiredModules;
};

#endif
