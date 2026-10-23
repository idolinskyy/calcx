# calx-ide: Qt Widgets IDE поверх бібліотеки calc.
find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CALX_IDE_VERSION_STRING "0.1.0")
string(TIMESTAMP CALX_IDE_BUILD_TIME_READABLE "%d.%m.%Y %H:%M:%S UTC" UTC)

add_executable(calx-ide
  src/main.cpp
  src/MainWindow.cpp
  src/editor/CalxEditor.cpp
  src/editor/CalxHighlighter.cpp
  src/repl/ReplWidget.cpp
  src/debug/DebugController.cpp
  src/docks/ConsoleDock.cpp
  src/docks/VarsDock.cpp
  src/docks/FunsDock.cpp
  src/docks/ConstsDock.cpp
  src/docks/DisasmDock.cpp
  src/util/CalxAssist.cpp
  src/util/CalxFormatter.cpp
  src/help/HelpDialog.cpp
)

target_include_directories(calx-ide PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(calx-ide PRIVATE calc Qt6::Widgets)
# Qt #define emit конфліктує з calc::Chunk::emit.
target_compile_definitions(calx-ide PRIVATE
  QT_NO_EMIT
  "CALX_IDE_VERSION_STRING=\"${CALX_IDE_VERSION_STRING}\""
  "CALX_IDE_BUILD_TIME_READABLE=\"${CALX_IDE_BUILD_TIME_READABLE}\""
)
target_compile_options(calx-ide PRIVATE -Wall -Wextra -Wpedantic)

# Після cmake --install бінарник має знаходити саме ту Qt, з якою зібрано
# (інакше підхоплюється системна libQt6*.so іншої мінорної версії).
get_filename_component(_calx_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
set_target_properties(calx-ide PROPERTIES
  INSTALL_RPATH "${_calx_qt_prefix}/lib"
  BUILD_WITH_INSTALL_RPATH FALSE
)

install(TARGETS calx-ide RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
