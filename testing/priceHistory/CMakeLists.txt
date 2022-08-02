# This file is part of the Flowee project
# Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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

set(CMAKE_AUTOMOC ON)

include_directories(${CMAKE_SOURCE_DIR} ${Qt5Test_INCLUDE_DIRS})

add_executable(test_price_history
    TestPriceHistory.cpp
)
target_link_libraries(test_price_history pay_lib Qt5::Test)
add_test(NAME Pay_test_price_history COMMAND test_price_history)
