cmake_minimum_required(VERSION 3.19)
project(first_1 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    SerialPort
    TextToSpeech
)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort TextToSpeech Xml)

qt_standard_project_setup()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


qt_add_executable(first_1
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    src/config/appsettings.cpp src/config/appsettings.h src/core/excelimporter.cpp src/core/excelimporter.h src/core/models.h src/core/protocol.cpp src/core/protocol.h src/core/randomcolorresolver.cpp src/core/randomcolorresolver.h src/core/workflowengine.cpp src/core/workflowengine.h src/services/serialservice.cpp src/services/serialservice.h
    src/ui/queuetablemodel.cpp src/ui/queuetablemodel.h
    src/ui/colortablemodel.cpp src/ui/colortablemodel.h
    src/ui/conflicttablemodel.cpp src/ui/conflicttablemodel.h
    src/ui/conflictcolordelegate.cpp src/ui/conflictcolordelegate.h
)

target_link_libraries(first_1
    PRIVATE
        Qt6::Widgets
        Qt6::SerialPort
        Qt6::TextToSpeech
        Qt6::Xml
        Qt6::GuiPrivate
)

include(GNUInstallDirs)

install(TARGETS first_1
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET first_1
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# ===== QXlsx =====
# 你的 QXlsx 放置路径（按实际改）
set(QXLSX_DIR ${CMAKE_SOURCE_DIR}/QXlsx)

# 让编译器能找到 QXlsx 头文件（在 header 目录）
target_include_directories(${PROJECT_NAME} PRIVATE
    ${QXLSX_DIR}/header
)

# 把 QXlsx 的源码编进工程（在 source 目录）
file(GLOB QXLSX_SOURCES
    ${QXLSX_DIR}/source/*.cpp
)

file(GLOB QXLSX_HEADERS
    ${QXLSX_DIR}/header/*.h
    ${QXLSX_DIR}/header/*.hpp
)

target_sources(${PROJECT_NAME} PRIVATE
    ${QXLSX_HEADERS}
)


target_sources(${PROJECT_NAME} PRIVATE
    ${QXLSX_SOURCES}
)


