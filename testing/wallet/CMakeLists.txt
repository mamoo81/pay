set(CMAKE_INCLUDE_CURRENT_DIR ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_AUTOMOC ON)

message ("${CMAKE_SOURCE_DIR}")
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Test_INCLUDE_DIRS}
)

# include_directories(${LIBSERVER_INCLUDES} ${CMAKE_BINARY_DIR}/include)

add_executable(test_wallet
    TestWallet.cpp
    ${CMAKE_SOURCE_DIR}/Wallet.cpp
)
target_link_libraries(test_wallet
    flowee_utils
    flowee_p2p
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    Qt5::Test
)

add_test(NAME Pay_test_wallet COMMAND test_wallet)
