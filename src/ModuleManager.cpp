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
#include "ModuleManager.h"

#include <QFile>
#include <Logger.h>

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
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
    assert(info);
    if (info == nullptr)
        return;

    auto translations = info->translationUnit();
    if (!translations.isEmpty()) {
        auto *translator = new QTranslator(this);
        /*
         * We try to load it from the default location as used in the translations subdir (:/i18n)
         * and if that fails we try again at the root level of the QRC because individual modules
         * can then just use one data.qrc and include translations, if they manage their own.
         */
        if (translator->load(QLocale(), translations, QLatin1String("_"), QLatin1String(":/i18n")))
            QCoreApplication::installTranslator(translator);
        else if (translator->load(QLocale(), translations, QLatin1String("_"), QLatin1String(":")))
            QCoreApplication::installTranslator(translator);
        else
            delete translator;
    }
}
