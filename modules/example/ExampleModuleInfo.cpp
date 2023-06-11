#include "ExampleModuleInfo.h"

void ExampleModuleInfo::init(ModuleManager *manager)
{
    ModuleInfo *info = new ModuleInfo(manager);
    info->setId("exampleModule");

    manager->registerModule(info);
}
