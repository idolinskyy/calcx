# calx: бібліотека calc, CLI і тести (doctest).
cmake_minimum_required(VERSION 3.28)
project(calx VERSION 0.3.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Середовище інколи підсовує -std=c++20 у CMAKE_CXX_FLAGS — прибираємо, щоб
# лишився лише стандарт проєкту (C++23) і clangd не плутався.
if(DEFINED CMAKE_CXX_FLAGS)
  string(REGEX REPLACE "(^| )-std=(gnu\\+\\+|c\\+\\+)[0-9a-z]+" ""
         CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
endif()

# Симлінк у корені репозиторію: багато інструментів шукають саме тут.
file(CREATE_LINK
  "${CMAKE_BINARY_DIR}/compile_commands.json"
  "${CMAKE_SOURCE_DIR}/compile_commands.json"
  SYMBOLIC
)

add_library(calc
  src/value.cpp
  src/lexer.cpp
  src/parser.cpp
  src/compiler.cpp
  src/vm.cpp
  src/debug.cpp
  src/engine.cpp
  src/disasm.cpp
  src/format.cpp
  src/builtin.cpp
  src/constants.cpp
  src/repl_helpers.cpp
)
target_include_directories(calc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(calc PRIVATE -Wall -Wextra -Wpedantic)

include(FetchContent)

# replxx — readline-заміна з історією, стрілками та підсвіткою синтаксису.
set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(REPLXX_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  replxx
  GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
  GIT_TAG release-0.0.4
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(replxx)
# Upstream має багато -Wsign-conversion; для наших таргетів — system headers.
if(TARGET replxx)
  target_compile_options(replxx PRIVATE -w)
  get_target_property(_replxx_inc replxx INTERFACE_INCLUDE_DIRECTORIES)
  if(_replxx_inc)
    set_property(TARGET replxx PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                 "${_replxx_inc}")
  endif()
endif()

add_executable(calc_cli src/main.cpp src/repl.cpp)
target_link_libraries(calc_cli PRIVATE calc replxx::replxx)
# Імʼя calx, щоб не конфліктувати з пакетом Debian/Ubuntu `calc`.
set_target_properties(calc_cli PROPERTIES OUTPUT_NAME calx)
target_compile_options(calc_cli PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS calc_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
  "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
  "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

option(CALX_BUILD_IDE "Build calx-ide (Qt 6.12 Widgets)" OFF)
set(CALX_QT_ROOT "" CACHE PATH "Optional Qt prefix (official installer gcc_64)")
if(CALX_QT_ROOT AND EXISTS "${CALX_QT_ROOT}")
  list(PREPEND CMAKE_PREFIX_PATH "${CALX_QT_ROOT}")
endif()
if(CALX_BUILD_IDE)
  add_subdirectory(ide)
endif()

# CPack (.deb / .tar.gz) — локальне пакування через scripts/package.sh.
set(CPACK_PACKAGE_NAME "calx")
set(CPACK_PACKAGE_VENDOR "calx")
set(CPACK_PACKAGE_CONTACT "Ihor Dolinskyi")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Calculator language and optional Qt IDE (calx / calx-ide)")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_STRIP_FILES TRUE)
if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB;TGZ")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ihor Dolinskyi")
  set(CPACK_DEBIAN_PACKAGE_SECTION "math")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  if(CALX_BUILD_IDE AND CALX_BUNDLE_QT)
    # Не тягнути системний libqt6*: у пакеті свій Qt з префікса збірки.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
      "libc6, libstdc++6, libgcc-s1, libgl1, libopengl0, libegl1, libx11-6, libx11-xcb1, libxcb1, libxkbcommon0, libxkbcommon-x11-0, libxcb-cursor0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render0, libxcb-render-util0, libxcb-shape0, libxcb-shm0, libxcb-sync1, libxcb-xfixes0, libxcb-xkb1, libxcb-util1, libfontconfig1, libfreetype6, libpng16-16 | libpng16-16t64, zlib1g, libglib2.0-0 | libglib2.0-0t64, libdbus-1-3, libzstd1, libssl3")
  else()
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  endif()
endif()
include(CPack)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(doctest)

enable_testing()
add_executable(calc_tests
  tests/main.cpp
  tests/test_value.cpp
  tests/test_lexer.cpp
  tests/test_parser.cpp
  tests/test_bytecode.cpp
  tests/test_disasm.cpp
  tests/test_ops_logic.cpp
  tests/test_format.cpp
  tests/test_if.cpp
  tests/test_loops.cpp
  tests/test_functions.cpp
  tests/test_collections.cpp
  tests/test_vm.cpp
  tests/test_debug.cpp
  tests/test_repl.cpp
  tests/test_e2e.cpp
  src/repl.cpp
)
target_link_libraries(calc_tests PRIVATE calc doctest::doctest replxx::replxx)
target_compile_options(calc_tests PRIVATE
  -Wall -Wextra -Wpedantic
  -fsanitize=address,undefined
  -fno-omit-frame-pointer
)
target_link_options(calc_tests PRIVATE -fsanitize=address,undefined)
add_test(NAME calc_tests COMMAND calc_tests)
# LSan у деяких середовищах конфліктує з ptrace; вимикаємо лише detect_leaks.
set_tests_properties(calc_tests PROPERTIES
  ENVIRONMENT "LSAN_OPTIONS=detect_leaks=0"
)
