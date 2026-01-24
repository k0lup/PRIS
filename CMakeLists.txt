cmake_minimum_required(VERSION 3.10)

project(DipLang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Нужно из-за Q_OBJECT (в репо есть классы с Q_OBJECT)
set(CMAKE_AUTOMOC ON)
# UI и ресурсы сейчас не видны в найденных файлах, но можно оставить включенными:
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt5: минимум для этого кода — Core + Widgets
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Sql Help Concurrent)

add_executable(DipLang)

# Собираем все исходники из корня и из папки Directives
file(GLOB SRC_ROOT CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# (Опционально) подтянуть .h, чтобы они отображались в IDE
file(GLOB HDR_ROOT CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)


target_sources(DipLang PRIVATE
    ${SRC_ROOT} ${SRC_DIRS}
    ${HDR_ROOT} ${HDR_DIRS}
)

target_include_directories(DipLang PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(DipLang PRIVATE Qt5::Core Qt5::Widgets Qt5::Network Qt5::Sql Qt5::Help Qt5::Concurrent)
