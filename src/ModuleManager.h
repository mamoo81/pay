#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <QObject>

#include "ModuleInfo.h"

class ModuleManager : public QObject
{
    Q_OBJECT
public:
    explicit ModuleManager(QObject *parent = nullptr);

    void init();

    void registerModule(ModuleInfo *info);


private:

};

#endif
