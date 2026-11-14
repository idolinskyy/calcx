#!/usr/bin/env bash
# Збирає пакети calx / calx-ide: .deb і .tar.gz.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${CALX_BUILD_DIR:-$ROOT/build-package}"
DIST="$ROOT/dist"

CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCALX_BUILD_IDE=ON)
if [[ -n "${CALX_QT_ROOT:-}" ]]; then
  CMAKE_ARGS+=(-DCALX_QT_ROOT="$CALX_QT_ROOT" -DCMAKE_PREFIX_PATH="$CALX_QT_ROOT")
  echo "CALX_QT_ROOT: $CALX_QT_ROOT"
elif [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
else
  # Офіційний інсталер: ~/Qt/6.10.2/gcc_64, ~/Qt/6.12.0/gcc_64, …
  qt_prefix="$(ls -1d "$HOME"/Qt/[0-9]*/gcc_64 2>/dev/null | sort -V | tail -1 || true)"
  if [[ -n "$qt_prefix" ]]; then
    CMAKE_ARGS+=(-DCALX_QT_ROOT="$qt_prefix" -DCMAKE_PREFIX_PATH="$qt_prefix")
    echo "Qt prefix: $qt_prefix"
  fi
fi

cmake -S "$ROOT" -B "$BUILD" -G Ninja "${CMAKE_ARGS[@]}"
cmake --build "$BUILD" -j"$(nproc)" --target calc_cli calc_tests calx-ide
# ASAN/UBSAN збірки: LSan інколи падає під ptrace у CI/IDE — як у CTest.
LSAN_OPTIONS="${LSAN_OPTIONS:-detect_leaks=0}" "$BUILD/calc_tests"

rm -rf "$DIST"
mkdir -p "$DIST/AppDir"
cmake --install "$BUILD" --prefix "$DIST/AppDir/usr"

# CPack пише .deb/.tar.gz поруч із білдом.
cmake --build "$BUILD" --target package
find "$BUILD" -maxdepth 1 \( -name 'calx*.deb' -o -name 'calx*.tar.gz' \) \
  -exec cp -a {} "$DIST/" \;

echo "Готово. Каталог артефактів: $DIST"
ls -la "$DIST"
