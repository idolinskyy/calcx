// Строгі тести компіляції в байткод (порядок опкодів).
#include "calc/chunk.hpp"
#include "calc/compiler.hpp"
#include "calc/engine.hpp"
#include "calc/lexer.hpp"
#include "calc/parser.hpp"

#include <doctest/doctest.h>
#include <vector>

using calc::compile;
using calc::compile_source;
using calc::ErrorCode;
using calc::OpCode;
using calc::parse;
using calc::tokenize;

namespace {

[[nodiscard]] std::vector<OpCode> opcodes_of(const calc::Chunk &chunk) {
  std::vector<OpCode> ops;
  std::size_t i = 0;
  while (i < chunk.code.size()) {
    const auto op = static_cast<OpCode>(chunk.code[i++]);
    ops.push_back(op);
    switch (op) {
    case OpCode::LoadConst:
    case OpCode::LoadLocal:
    case OpCode::StoreLocal:
    case OpCode::Jump:
    case OpCode::JumpIfFalse:
    case OpCode::MakeArray:
    case OpCode::LoadField:
    case OpCode::StoreField:
      i += 2;
      break;
    case OpCode::PrintFmt:
    case OpCode::Call:
    case OpCode::CallBuiltin:
      i += 3; // u16 + u8
      break;
    case OpCode::MakeRecord: {
      if (i + 1 >= chunk.code.size()) {
        break;
      }
      const auto lo = chunk.code[i];
      const auto hi = chunk.code[i + 1];
      const auto count = static_cast<std::uint16_t>(
          lo | (static_cast<std::uint16_t>(hi) << 8));
      i += 2 + static_cast<std::size_t>(count) * 2;
      break;
    }
    case OpCode::MakeIter:
      i += 1; // u8 mode
      break;
    default:
      break;
    }
  }
  return ops;
}

[[nodiscard]] bool contains_sequence(const std::vector<OpCode> &ops,
                                     std::initializer_list<OpCode> seq) {
  if (seq.size() == 0) {
    return true;
  }
  const std::vector<OpCode> needle(seq);
  if (needle.size() > ops.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= ops.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      if (ops[i + j] != needle[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("let a = 1 + 2; print(a); emits stack bytecode sequence") {
  auto chunk = compile_source("let a = 1 + 2; print(a);");
  REQUIRE(chunk);
  const auto ops = opcodes_of(*chunk);

  // LoadConst, LoadConst, Add, StoreLocal, LoadLocal, Print, Halt
  REQUIRE(contains_sequence(ops, {OpCode::LoadConst, OpCode::LoadConst,
                                  OpCode::Add, OpCode::StoreLocal,
                                  OpCode::LoadLocal, OpCode::Print}));
  CHECK(ops.back() == OpCode::Halt);
  CHECK(chunk->local_count == 1);
  CHECK(chunk->constants.size() == 2);
}

TEST_CASE("print format compiles to PrintFmt") {
  auto chunk = compile_source("let x = 1; print(\"v={}\", x);");
  REQUIRE(chunk);
  const auto ops = opcodes_of(*chunk);
  CHECK(contains_sequence(ops, {OpCode::LoadLocal, OpCode::PrintFmt}));
  REQUIRE(chunk->formats.size() == 1);
  CHECK(chunk->formats[0] == "v={}");
}

TEST_CASE("format placeholder mismatch fails at compile time") {
  auto too_few = compile_source("let x = 1; print(\"{} {}\", x);");
  REQUIRE_FALSE(too_few);
  CHECK(too_few.error().code == ErrorCode::FormatMismatch);

  auto too_many = compile_source("let x = 1; print(\"{}\", x, x);");
  REQUIRE_FALSE(too_many);
  CHECK(too_many.error().code == ErrorCode::FormatMismatch);
}
