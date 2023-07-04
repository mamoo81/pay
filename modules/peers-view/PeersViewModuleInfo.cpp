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
#include "PeersViewModuleInfo.h"

ModuleInfo * PeersViewModuleInfo::build()
{
    ModuleInfo *info = new ModuleInfo();
    info->setId("peersViewModule");
    info->setTitle(tr("Peers View"));
    info->setDescription(tr("This module provides a view of network servers we connect to "
        "often called 'peers'."));
    // info->setIconSource("qrc:/example/example.svg");

    auto menuExample = new ModuleSection(ModuleSection::MainMenuItem, info);
    menuExample->setText(tr("Network Details"));
    menuExample->setStartQMLFile("qrc:/peers-view/NetView.qml");
    info->addSection(menuExample);

    return info;
}
