#include "ModuleManager.h"

#include <QFile>
#include <Logger.h>

#include <stdexcept>

    // : m_basedir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
    // boost::filesystem::create_directories(boost::filesystem::path(m_basedir.toStdString()));

ModuleManager::ModuleManager(QObject *parent)
    : QObject(parent)
{
}

void ModuleManager::init()
{
    extern void load_all_modules();
    load_all_modules();
}

void ModuleManager::registerModule(ModuleInfo *info)
{
    logFatal() << "haha";
}
