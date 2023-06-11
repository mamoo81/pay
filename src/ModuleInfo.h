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

private:
    QString m_id;
    QString m_title;
    QString m_description;

    QList<ModuleSection> m_sections;
};

#endif
