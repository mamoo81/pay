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

#add_subdirectory(example)
#add_subdirectory(build-transaction)

# Find all modules to auto-invoke and write them to a file.
message("")
message("Modules")
message("=======")
set(INCLUDES_LIST "")
set(CLASSES_LIST "")
file(GLOB sub_directories ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach (child ${sub_directories})
    if (IS_DIRECTORY ${child})
        file(GLOB subModule ${child}/*ModuleInfo.h)
        if (EXISTS ${subModule})
            add_subdirectory(${child})
            file (RELATIVE_PATH module ${CMAKE_CURRENT_SOURCE_DIR} ${child})
            message("-- Including module '${module}'")
            get_filename_component(className ${subModule} NAME_WE)
            message("   ${className}")
            list (APPEND CLASSES_LIST ${className})

            file (RELATIVE_PATH include_file ${CMAKE_CURRENT_SOURCE_DIR} ${subModule})
            list (APPEND INCLUDES_LIST ${include_file})
        endif()
    endif()
endforeach()

# After compiling them, generate a little cpp file that actually loads those modules
# into the ModulesManager
set (MODULES_LOAD_CPP "#include <FloweePay.h>\n")
foreach (include ${INCLUDES_LIST})
    set (MODULES_LOAD_CPP "${MODULES_LOAD_CPP}#include <${include}>\n")
endforeach()

set (MODULES_LOAD_CPP "${MODULES_LOAD_CPP}\nvoid load_all_modules() {\n  auto *m=FloweePay::instance()->modules();\n")
foreach (className ${CLASSES_LIST})
    set (MODULES_LOAD_CPP "${MODULES_LOAD_CPP}  ${className}::init(m);\n")
endforeach()

# finally, write out the cpp file.
file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/modules-load.cpp "${MODULES_LOAD_CPP}}")
