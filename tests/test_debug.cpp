// DebugSession: step і breakpoints.
#include "calc/debug.hpp"
#include "calc/engine.hpp"

#include <doctest/doctest.h>

using calc::DebugSession;
using calc::DebugState;
using calc::compile_source;

TEST_CASE("debug step through print program") {
  auto chunk = compile_source("print(1);\nprint(2);\n");
  REQUIRE(chunk);
  DebugSession dbg;
  dbg.load(*chunk);
  CHECK(dbg.state() == DebugState::Paused);

  while (dbg.state() != DebugState::Finished) {
    auto r = dbg.step_instruction();
    REQUIRE(r);
  }
  CHECK(dbg.output() == "1\n2\n");
}

TEST_CASE("debug breakpoint on second print line") {
  auto chunk = compile_source("print(1);\nprint(2);\n");
  REQUIRE(chunk);
  DebugSession dbg;
  dbg.load(*chunk);
  dbg.set_breakpoint(2, true);
  auto r = dbg.continue_run();
  REQUIRE(r);
  CHECK(dbg.state() == DebugState::Paused);
  CHECK(dbg.current_line() == 2);
  CHECK(dbg.output() == "1\n");
  r = dbg.continue_run();
  REQUIRE(r);
  CHECK(dbg.state() == DebugState::Finished);
  CHECK(dbg.output() == "1\n2\n");
}
