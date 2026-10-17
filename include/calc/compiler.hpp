// Компіляція AST у плаский байткод.
#pragma once

#include "calc/ast.hpp"
#include "calc/chunk.hpp"
#include "calc/error.hpp"

#include <expected>

namespace calc {

using CompileResult = std::expected<Chunk, Diagnostic>;

/// Компілює програму в Chunk.
/// Параметр: program — AST.
/// Повертає Chunk або першу діагностику.
[[nodiscard]] CompileResult compile(const Program& program);

}  // namespace calc
