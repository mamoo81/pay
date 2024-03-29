# This file is part of the Flowee project
# Copyright (C) 2020 Tom Zander <tom@flowee.org>
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

find_package(Qt6Test)
if (${Qt6Test_FOUND})
    enable_testing()
    add_definitions(-DIN_TESTS)
    add_subdirectory(wallet)
    add_subdirectory(walletHistoryModel)
    add_subdirectory(value)
    add_subdirectory(priceHistory)

    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS
        test_wallet
        test_wallethistorymodel
        test_value
        test_price_history
    )
endif ()
