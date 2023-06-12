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
#pragma once

#include <ModuleManager.h>

class ExampleModuleInfo : public QObject
{
    Q_OBJECT
public:
    static ModuleInfo *build();

    /**
     * A module has its own translations, to make the wallet load
     * those translations we need to report what the basename of
     * our translation-unit is.
     * Note that returning nullptr is legal and just skips this part.
     *
     * Example: "module-build-transaction"
     */
    static const char *translationUnit() {
        return nullptr;
    }
};
