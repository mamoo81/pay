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
#include "ExampleModuleInfo.h"

ModuleInfo * ExampleModuleInfo::build()
{
    ModuleInfo *info = new ModuleInfo();
    info->setId("exampleModule");
    info->setTitle(tr("Example Module"));
    info->setDescription(tr("The example module is helpful in order to show the concept of a module "
        "and what it can do in Flowee Pay. This text is shown in the module explorer UI."));
    info->setIconSource("qrc:/example/example.svg");

    // A section is a definition on where in the application this module is able
    // to be plugged-in.
    auto sendButtonExample = new ModuleSection(ModuleSection::SendMethod, info);
    sendButtonExample->setText(tr("Example Module"));
    sendButtonExample->setSubtext(tr("This is some helptext"));
    // notice that the directory it is in, is registered in the example-data.qrc file
    sendButtonExample->setStartQMLFile("qrc:/example/ExamplePage.qml");
    info->addSection(sendButtonExample);

    auto menuExample = new ModuleSection(ModuleSection::MainMenuItem, info);
    menuExample->setText(tr("Example Module"));
    menuExample->setSubtext(tr("This is some helptext"));
    // notice that the directory it is in, is registered in the example-data.qrc file
    menuExample->setStartQMLFile("qrc:/example/ExamplePage.qml");
    info->addSection(menuExample);

    return info;
}
