# This file is part of the Flowee project
# Copyright (C) 2023 Tom Zander <tom@flowee.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

project(build-transaction_module)

set (SOURCES
    BuildTransactionModuleInfo.cpp
)
qt6_add_resources(SOURCES data.qrc)
add_library (build-transaction_module_lib STATIC ${SOURCES})
target_link_libraries(build-transaction_module_lib pay_lib)

