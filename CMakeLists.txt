cmake_minimum_required(VERSION 3.10)

project(NetworkChecker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Network)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/networkchecker.cpp
    src/networkcheckeradaptor.cpp
    src/networkcheckerbydomain.cpp
    src/networkcheckerbyip.cpp
    src/networkcheckerproxy.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::DBus
    Qt5::Network
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
    org.deepin.demo.NetworkChecker.service
    DESTINATION /lib/systemd/system
)

install(FILES
    org.deepin.demo.NetworkChecker.conf
    DESTINATION /etc/dbus-1/system.d
)
