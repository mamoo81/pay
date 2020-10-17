find_package(Qt5Test)
if (${Qt5Test_FOUND})
    enable_testing()
    add_subdirectory(wallet)

    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS
            test_wallet
    )
endif ()
