cmake_minimum_required(VERSION 3.10)

project(NetworkChecker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Network)
find_package(PkgConfig REQUIRED)
find_package(KF5NetworkManagerQt REQUIRED)

pkg_check_modules(LibNM REQUIRED IMPORTED_TARGET libnm)

get_target_property(KF5_QT_INCLUDE_DIRS KF5::NetworkManagerQt INTERFACE_INCLUDE_DIRECTORIES)

set(TRY_KF5_LIBRARIES
    IMPORTED_LOCATION_DEBIAN
    IMPORTED_LOCATION_NOCONFIG
)

foreach(LETTER ${TRY_KF5_LIBRARIES})
    if (NOT KF5_QT_LIBRARIES)
        get_target_property(KF5_QT_LIBRARIES KF5::NetworkManagerQt ${LETTER})
    endif()
endforeach()

if (NOT KF5_QT_LIBRARIES)
    find_library(KF5_QT_LIBRARIES libKF5NetworkManagerQt.so)
endif()

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
    ${KF5_QT_INCLUDE_DIRS}
    PkgConfig::LibNM
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    ${KF5_QT_LIBRARIES}
    PkgConfig::LibNM
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
