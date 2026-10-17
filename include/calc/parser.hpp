// Парсер: токени → AST програми.
#pragma once

#include "calc/ast.hpp"
#include "calc/token.hpp"

#include <expected>
#include <vector>

namespace calc {

/// Результат розбору або діагностика.
using ParseResult = std::expected<Program, Diagnostic>;

/// Рекурсивний спуск над токенами (без End на кінці — End очікується).
/// Параметр: tokens — вихід лексера.
/// Повертає Program або Diagnostic.
[[nodiscard]] ParseResult parse(const std::vector<Token>& tokens);

}  // namespace calc
