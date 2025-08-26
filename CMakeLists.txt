cmake_minimum_required(VERSION 3.10)
project(FW12Rotate)

set(CMAKE_CXX_STANDARD 20)

# Find pkg-config
find_package(PkgConfig REQUIRED)

# Find D-Bus
pkg_check_modules(DBUS REQUIRED dbus-1)

# Find inotify-tools
set(INOTIFY_INCLUDE_DIRS /usr/include/inotifytools)
set(INOTIFY_LIBRARIES inotifytools)
link_directories(/usr/lib)

# Add executable
add_executable(FW12Rotate main.cpp)

# Include directories
include_directories(${DBUS_INCLUDE_DIRS} ${INOTIFY_INCLUDE_DIRS})

# Link libraries
target_link_libraries(FW12Rotate ${DBUS_LIBRARIES} ${INOTIFY_LIBRARIES})