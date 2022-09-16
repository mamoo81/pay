# For some unknown reason Boost doesn't get found using the
# cmake shipped 'find_package' when we use cross-compilation.

# So we use a bit of a hack here as we assume the build
# is being done using the docker, so we can just do some
# basic checks and give up if that fails.


if (NOT EXISTS "/opt/android-boost/include/boost/filesystem/path.hpp")
    message(FATAL_ERROR "Missing boost headers")
endif()
if (NOT EXISTS "/opt/android-boost/lib/libboost_filesystem.a")
    message(FATAL_ERROR "Missing boost static libs")
endif()

set(Boost_FOUND TRUE)
set(Boost_INCLUDE_DIRS /opt/android-boost/include)
set(Boost_LIBRARY_DIRS /opt/android-boost/lib)

set(Boost_LIBRARIES
    ${Boost_LIBRARY_DIRS}/libboost_filesystem.a
    ${Boost_LIBRARY_DIRS}/libboost_system.a
    ${Boost_LIBRARY_DIRS}/libboost_chrono.a
    ${Boost_LIBRARY_DIRS}/libboost_iostreams.a
    ${Boost_LIBRARY_DIRS}/libboost_program_options.a
    ${Boost_LIBRARY_DIRS}/libboost_thread.a
)
set(Boost_FILESYSTEM_FOUND ON)
set(Boost_SYSTEM_FOUND ON)
set(Boost_chrono_FOUND ON)
set(Boost_iostreams_FOUND ON)
set(Boost_program_options_FOUND ON)
set(Boost_thread_FOUND ON)
set(Boost_VERSION_STRING 1.67.0)

include_directories(${Boost_INCLUDE_DIRS})
