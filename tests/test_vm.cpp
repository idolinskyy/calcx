// Захист ВМ: стек, індекси, IP (пошкоджений / урізаний chunk).
#include "calc/chunk.hpp"
#include "calc/error.hpp"
#include "calc/value.hpp"
#include "calc/vm.hpp"

#include <sstream>

#include <doctest/doctest.h>

using calc::Chunk;
using calc::ErrorCode;
using calc::OpCode;
using calc::Value;
using calc::execute;

namespace {

[[nodiscard]] ErrorCode run_chunk(const Chunk &chunk) {
  std::ostringstream out;
  auto r = execute(chunk, out);
  REQUIRE_FALSE(r);
  return r.error().code;
}

} // namespace

TEST_CASE("VM rejects empty stack for unary Neg") {
  Chunk chunk;
  chunk.emit(OpCode::Neg);
  chunk.emit(OpCode::Halt);
  CHECK(run_chunk(chunk) == ErrorCode::UnexpectedToken);
}

TEST_CASE("VM rejects LoadConst past constants pool") {
  Chunk chunk;
  chunk.emit_u16(OpCode::LoadConst, 0);
  chunk.emit(OpCode::Halt);
  CHECK(run_chunk(chunk) == ErrorCode::UnexpectedToken);
}

TEST_CASE("VM rejects truncated u16 operand") {
  Chunk chunk;
  chunk.code.push_back(static_cast<std::uint8_t>(OpCode::LoadConst));
  chunk.code.push_back(0); // лише lo, без hi
  CHECK(run_chunk(chunk) == ErrorCode::UnexpectedToken);
}

TEST_CASE("VM rejects jump past end of code") {
  Chunk chunk;
  chunk.emit_u16(OpCode::Jump, 100);
  chunk.emit(OpCode::Halt);
  CHECK(run_chunk(chunk) == ErrorCode::UnexpectedToken);
}

TEST_CASE("VM rejects falling off code without Halt") {
  Chunk chunk;
  auto idx = chunk.add_constant(Value{calc::Integer{1}});
  REQUIRE(idx);
  chunk.emit_u16(OpCode::LoadConst, *idx);
  chunk.emit(OpCode::Pop);
  // без Halt
  CHECK(run_chunk(chunk) == ErrorCode::UnexpectedToken);
}

TEST_CASE("well-formed chunk still runs") {
  Chunk chunk;
  auto idx = chunk.add_constant(Value{calc::Integer{42}});
  REQUIRE(idx);
  chunk.emit_u16(OpCode::LoadConst, *idx);
  chunk.emit(OpCode::Print);
  chunk.emit(OpCode::Halt);
  std::ostringstream out;
  auto r = execute(chunk, out);
  REQUIRE(r);
  CHECK(out.str() == "42\n");
}
