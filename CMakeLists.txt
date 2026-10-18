cmake_minimum_required(VERSION 3.20)
project(plasma-calendar-homefeed VERSION 0.2.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.0.0")
set(PLASMA_MIN_VERSION "6.0.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Network Qml Quick)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons Declarative)
find_package(Plasma ${PLASMA_MIN_VERSION} REQUIRED)

add_subdirectory(plugin)
add_subdirectory(applet)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# `cmake --build build --target clang-format` rewrites sources in place using
# the KDE style shipped with ECM; CI runs it and fails on any resulting diff.
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/plugin/*.cpp
    ${CMAKE_SOURCE_DIR}/plugin/*.h
    ${CMAKE_SOURCE_DIR}/applet/*.cpp
    ${CMAKE_SOURCE_DIR}/applet/*.h
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
