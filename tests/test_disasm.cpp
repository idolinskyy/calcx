// Тести текстового дампу байткоду (змінні, константи, опкоди).
#include "calc/disasm.hpp"
#include "calc/engine.hpp"
#include "calc/vm.hpp"

#include <sstream>
#include <string>

#include <doctest/doctest.h>

using calc::compile_source;
using calc::dump_chunk;

TEST_CASE("dump shows locals, constants and instruction names") {
  auto chunk = compile_source("let a = 1 + 2; print(a);");
  REQUIRE(chunk);

  std::ostringstream out;
  dump_chunk(*chunk, out);
  const std::string text = out.str();

  CHECK(text.find("local_count = 1") != std::string::npos);
  CHECK(text.find("a") != std::string::npos);
  CHECK(text.find("Integer(1)") != std::string::npos);
  CHECK(text.find("Integer(2)") != std::string::npos);
  CHECK(text.find("LoadConst") != std::string::npos);
  CHECK(text.find("Add") != std::string::npos);
  CHECK(text.find("StoreLocal") != std::string::npos);
  CHECK(text.find("LoadLocal") != std::string::npos);
  CHECK(text.find("Print") != std::string::npos);
  CHECK(text.find("Halt") != std::string::npos);
  // Числові значення опкодів і легенда.
  CHECK(text.find("1=LoadConst") != std::string::npos);
  CHECK(text.find("5=Pos") != std::string::npos);
  CHECK(text.find("6=Add") != std::string::npos);
  CHECK(text.find(" 1  LoadConst") != std::string::npos);
  CHECK(text.find(" 6  Add") != std::string::npos);
}

TEST_CASE("dump shows format pool for print fmt") {
  auto chunk = compile_source("let x = 1; print(\"v={}\", x);");
  REQUIRE(chunk);

  std::ostringstream out;
  dump_chunk(*chunk, out);
  const std::string text = out.str();

  CHECK(text.find("x") != std::string::npos);
  CHECK(text.find("PrintFmt") != std::string::npos);
  CHECK(text.find("v={}") != std::string::npos);
}

TEST_CASE("dump hides for-in temps unless show_temps") {
  auto chunk = compile_source("let a = [1, 2]; for (x in a) { print(x); }");
  REQUIRE(chunk);

  std::ostringstream hidden;
  dump_chunk(*chunk, hidden);
  const std::string h = hidden.str();
  CHECK(h.find("]  __it") == std::string::npos); // не в списку слотів
  CHECK(h.find("--dump-temps") != std::string::npos);
  CHECK(h.find("  [") != std::string::npos); // user locals still listed
  CHECK(h.find("a") != std::string::npos);

  std::ostringstream shown;
  dump_chunk(*chunk, shown, {}, true);
  const std::string s = shown.str();
  CHECK(s.find("__it") != std::string::npos);
  CHECK(s.find("__i") != std::string::npos);
  CHECK(s.find("службові слоти for-in") != std::string::npos);
}

TEST_CASE("dump with locals shows type and value") {
  auto chunk =
      compile_source("let a = 12 * (3 + 2); let compl = 2 + 2i, neg = -1.23;\n"
                     "let sum = a + neg;\n"
                     "print(sum);\n");
  REQUIRE(chunk);

  std::ostringstream discarded;
  auto ran = calc::execute_with_locals(*chunk, discarded);
  REQUIRE(ran);

  std::ostringstream out;
  dump_chunk(*chunk, out, ran->locals);
  const std::string text = out.str();

  CHECK(text.find("Integer") != std::string::npos);
  CHECK(text.find("Real") != std::string::npos);
  CHECK(text.find("Complex") != std::string::npos);
  CHECK(text.find("60") != std::string::npos);
  CHECK(text.find("-1.23") != std::string::npos);
  CHECK(text.find("58.77") != std::string::npos);
  CHECK(text.find("2 + 2i") != std::string::npos);
  CHECK(text.find("name") != std::string::npos);
  CHECK(text.find("type") != std::string::npos);
}
