cmake_minimum_required(VERSION 3.16)

#cmake_policy(SET CMP0167 OLD)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum OS X deployment version")
endif()

project(motioncam-fs VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(DEPENDENCIES_PATH deps)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/VirtualFileSystemImpl_MCRAW.cpp
        src/CameraMetadata.cpp
        src/CameraFrameMetadata.cpp
        src/AudioWriter.cpp
        src/Utils.cpp

        include/mainwindow.h
        include/Types.h
        include/IVirtualFileSystem.h
        include/IFuseFileSystem.h
        include/VirtualFileSystemImpl_MCRAW.h
        include/LRUCache.h
        include/AudioWriter.h
        include/Measure.h
        include/SingleApplication.h
        include/CameraMetadata.h
        include/CameraFrameMetadata.h
        include/Utils.h

        ui/mainwindow.ui
)

qt_add_resources(PROJECT_SOURCES resources.qrc)

if(WIN32)
    list(APPEND PROJECT_SOURCES
            resources/app.rc
            src/win/FuseFileSystemImpl_Win.cpp
            src/win/virtualizationInstance.cpp
            src/win/dirInfo.cpp

            include/win/dirInfo.h
            include/win/FuseFileSystemImpl_Win.h
            include/win/virtualizationInstance.h
    )
    find_library(projected-fs ProjectedFSLib)
    set(platform-specific ${projected-fs})

elseif(APPLE)
    list(APPEND PROJECT_SOURCES
            src/macos/FuseFileSystemImpl_MacOS.cpp
            include/macos/FuseFileSystemImpl_MacOS.h
    )
    find_library(fuse_t fuse_t)
    set(platform-specific ${fuse_t})

elseif(UNIX AND NOT APPLE)
    list(APPEND PROJECT_SOURCES
            ${PROJECT_SOURCE_DIR}/src/linux/FuseFileSystemImpl_Linux.cpp
            ${PROJECT_SOURCE_DIR}/include/linux/FuseFileSystemImpl_Linux.h
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FUSE REQUIRED fuse)

    include_directories(${FUSE_INCLUDE_DIRS})
    link_directories(${FUSE_LIBRARY_DIRS})
    set(platform-specific ${FUSE_LIBRARIES})
endif()

set(CMAKE_AUTOUIC_SEARCH_PATHS ui)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(motioncam-fs
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(motioncam-fs SHARED
                ${PROJECT_SOURCES}
        )
    else()
        add_executable(motioncam-fs
                ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(motioncam-fs PRIVATE include)

# FUSE2:
target_compile_definitions(motioncam-fs PRIVATE _FILE_OFFSET_BITS=64 FUSE_USE_VERSION=26)

if(UNIX AND NOT APPLE)
    target_include_directories(motioncam-fs PRIVATE
            ${PROJECT_SOURCE_DIR}/deps/thread-pool/include
    )
endif()

# Add motioncam decoder as static library
add_library(motioncam-decoder STATIC
        ${DEPENDENCIES_PATH}/motioncam-decoder/lib/Decoder.cpp
        ${DEPENDENCIES_PATH}/motioncam-decoder/lib/RawData.cpp
        ${DEPENDENCIES_PATH}/motioncam-decoder/lib/RawData_Legacy.cpp
)

target_include_directories(motioncam-decoder
        PUBLIC
        ${DEPENDENCIES_PATH}/bw64/include
        ${DEPENDENCIES_PATH}/motioncam-decoder/thirdparty
        ${DEPENDENCIES_PATH}/motioncam-decoder/lib/include
)

# Dependencies via vcpkg or system
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Boost
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_DEBUG_LIBS OFF)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_NO_BOOST_CMAKE ON)

find_package(Boost 1.74 REQUIRED COMPONENTS filesystem locale)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

target_link_libraries(motioncam-fs PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        ${Boost_FILESYSTEM_LIBRARY}
        spdlog::spdlog
        fmt::fmt
        motioncam-decoder
        ${platform-specific}
)

# macOS bundle properties
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.motioncam.motioncam-fs)
endif()

set_target_properties(motioncam-fs PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

# install() removed; AppImage packaging will handle layout


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(motioncam-fs)
endif()
