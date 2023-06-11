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
