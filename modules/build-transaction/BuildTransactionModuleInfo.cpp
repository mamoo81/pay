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
#include "BuildTransactionModuleInfo.h"

void BuildTransactionModuleInfo::init(ModuleManager *manager)
{
    auto module = new ModuleInfo(manager);
    module->setTranslationUnit("module-build-transaction");

    auto sendButton = new ModuleSection(ModuleSection::SendMethod, module);
    sendButton->setText(tr("Build Transaction"));
    // notice that the directory it is in is registered in the data.qrc header
    sendButton->setStartQMLFile("qrc:/build-transaction/PayToOthers.qml");
    module->addSection(sendButton);

    manager->registerModule(module);
}
