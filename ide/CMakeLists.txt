# calx-ide: Qt Widgets IDE поверх бібліотеки calc.
find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CALX_IDE_VERSION_STRING "${PROJECT_VERSION}")
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

# Bundled Qt (офіційний інсталер поза /usr) → lib/.../calx-ide + wrapper у bin/.
set(CALX_BUNDLE_QT OFF CACHE INTERNAL "Bundle Qt next to calx-ide")
get_filename_component(_calx_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
if(NOT _calx_qt_prefix MATCHES "^/usr(/|$)")
  set(CALX_BUNDLE_QT ON CACHE INTERNAL "Bundle Qt next to calx-ide" FORCE)
  set(CALX_QT_PREFIX "${_calx_qt_prefix}" CACHE INTERNAL "")
  set(CALX_BUNDLE_LIBDIR "${CMAKE_INSTALL_LIBDIR}/calx-ide" CACHE INTERNAL "")
elseif(CALX_QT_ROOT AND EXISTS "${CALX_QT_ROOT}/lib"
       AND NOT CALX_QT_ROOT MATCHES "^/usr(/|$)")
  set(CALX_BUNDLE_QT ON CACHE INTERNAL "Bundle Qt next to calx-ide" FORCE)
  set(CALX_QT_PREFIX "${CALX_QT_ROOT}" CACHE INTERNAL "")
  set(CALX_BUNDLE_LIBDIR "${CMAKE_INSTALL_LIBDIR}/calx-ide" CACHE INTERNAL "")
endif()

# Propagate bundle flag to parent for CPack shlibdeps.
set(CALX_BUNDLE_QT "${CALX_BUNDLE_QT}" PARENT_SCOPE)

if(CALX_BUNDLE_QT)
  set_target_properties(calx-ide PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    INSTALL_RPATH_USE_LINK_PATH FALSE)
  install(TARGETS calx-ide
    RUNTIME DESTINATION ${CALX_BUNDLE_LIBDIR})
  configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/calx-ide-wrapper.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/calx-ide-wrapper.sh"
    @ONLY
    NEWLINE_STYLE UNIX)
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/calx-ide-wrapper.sh"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME calx-ide)
  install(CODE "
    set(CALX_QT_PREFIX \"${CALX_QT_PREFIX}\")
    set(CALX_BUNDLE_LIBDIR \"${CALX_BUNDLE_LIBDIR}\")
    include(\"${CMAKE_SOURCE_DIR}/cmake/InstallBundledQt.cmake\")
  ")
else()
  if(CALX_QT_ROOT AND EXISTS "${CALX_QT_ROOT}/lib")
    set_target_properties(calx-ide PROPERTIES
      INSTALL_RPATH "${CALX_QT_ROOT}/lib")
  else()
    set_target_properties(calx-ide PROPERTIES
      INSTALL_RPATH "${_calx_qt_prefix}/lib")
  endif()
  install(TARGETS calx-ide RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES "${CMAKE_SOURCE_DIR}/packaging/calx-ide.desktop"
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
foreach(size 48 64 128 256)
  install(FILES
    "${CMAKE_SOURCE_DIR}/packaging/icons/hicolor/${size}x${size}/apps/calx-ide.png"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps
    OPTIONAL)
endforeach()
