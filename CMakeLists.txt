cmake_minimum_required(VERSION 3.20)
project(CopyPath)
set(CMAKE_CXX_STANDARD 20)

set(USER_NAME "CN=") # user name, must start with 'CN='.This must be same as the name in certificate
set(COMMAND_NAME "CopyPath") # name show in menu
set(COMMAND_UUID "") # uuid


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.in.h ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AppxManifest.in.xml ${CMAKE_CURRENT_SOURCE_DIR}/sparse_package/AppxManifest.xml)

add_library(context_menu SHARED command.cpp register.cpp source.def)

# all functions are implemented in context_menu, installer and uninstaller only call these functions
add_executable(installer installer.cpp)
add_executable(uninstaller uninstaller.cpp)
target_link_libraries(installer PRIVATE context_menu)
target_link_libraries(uninstaller PRIVATE context_menu)