// Попередження про цикли без виходу.
#include "calc/lexer.hpp"
#include "calc/loops.hpp"
#include "calc/parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

using calc::find_loop_warnings;

namespace {

[[nodiscard]] std::vector<calc::LoopWarning> warnings_of(std::string_view src) {
  auto lex = calc::tokenize(src);
  REQUIRE(lex.error.code == calc::ErrorCode::Ok);
  auto program = calc::parse(lex.tokens);
  REQUIRE(program);
  return find_loop_warnings(*program);
}

[[nodiscard]] bool has_text(const std::vector<calc::LoopWarning> &ws,
                            std::string_view part) {
  for (const auto &w : ws) {
    if (w.message.find(part) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("loop without exit") {
  CHECK(has_text(warnings_of("while (true) { print(1); }"), "не має виходу"));
  CHECK(warnings_of("while (false) { print(1); }").empty());
  CHECK(has_text(warnings_of("while (i < n) { print(i); }"), "не має виходу"));
  CHECK(warnings_of("while (i < n) { ++i; }").empty());
  CHECK(warnings_of("let i = 0; while (i < 3) { ++i; }").empty());
  CHECK(warnings_of("for (let i = 0; i < n; ++i) { print(i); }").empty());
  CHECK(has_text(warnings_of("for (;;) { print(1); }"), "не має виходу"));
  CHECK(warnings_of("for (x in a) { print(x); }").empty());
  CHECK(warnings_of("while (true) { break; }").empty());
  CHECK(warnings_of("while (true) { if (ready) { break; } }").empty());
  CHECK(has_text(warnings_of("while (true) { while (true) { break; } }"),
                 "не має виходу"));
}

TEST_CASE("exit blocked by unchecked sign") {
  const char *newton = R"(
fun sqrt_newton(x, eps) {
  let y = 0.0;
  if (x < 0) {
    return sqrt_newton(abs(x), eps) * i;
  }
  if (x == 0 or x == 1) {
    return x;
  }
  if (x > 1) {
    y = x * 0.5;
  } else {
    y = 1;
  }
  let y_next = 0;
  while (true) {
    y_next = 0.5 * (y + x / y);
    if (abs(y_next - y) <= eps * y_next) {
      break;
    }
    y = y_next;
  }
  return y_next;
}
)";
  const auto ws = warnings_of(newton);
  REQUIRE(ws.size() == 1);
  CHECK(has_text(ws, "eps"));
  CHECK(has_text(ws, "від'ємний"));

  const char *guarded = R"(
fun sqrt_newton(x, eps) {
  if (eps < 0) {
    return 0;
  }
  let y = 1;
  let y_next = 0;
  while (true) {
    y_next = 0.5 * (y + x / y);
    if (abs(y_next - y) <= eps * y_next) {
      break;
    }
    y = y_next;
  }
  return y_next;
}
)";
  CHECK(warnings_of(guarded).empty());

  CHECK(has_text(warnings_of(R"(
fun f(eps) {
  let d = 1;
  while (true) {
    if (abs(d) <= eps) { break; }
  }
}
)"),
                 "eps"));

  // Від'ємний eps виходить через return, але нуль лишає abs(1) <= eps хибним.
  {
    const auto inner = warnings_of(R"(
fun f(eps) {
  while (true) {
    if (eps < 0) { return 0; }
    if (abs(1) <= eps) { break; }
  }
}
)");
    CHECK(inner.size() == 1);
    CHECK(has_text(inner, "дорівнює нулю"));
    CHECK_FALSE(has_text(inner, "від'ємний"));
  }

  CHECK(warnings_of(R"(
fun f(eps) {
  while (true) {
    if (eps <= 0) { return 0; }
    if (abs(1) <= eps) { break; }
  }
}
)")
            .empty());

  CHECK(warnings_of(R"(
fun f(x) {
  while (true) {
    if (x > 0) { break; }
    ++x;
  }
}
)")
            .empty());
}

TEST_CASE("examples warn only on the unguarded newton loop") {
  namespace fs = std::filesystem;
  const fs::path dir{CALX_EXAMPLES_DIR};
  REQUIRE(fs::is_directory(dir));
  int files = 0;
  for (const auto &ent : fs::directory_iterator(dir)) {
    if (ent.path().extension() != ".calc") {
      continue;
    }
    ++files;
    std::ifstream in(ent.path());
    REQUIRE(in);
    std::ostringstream buf;
    buf << in.rdbuf();
    const auto ws = warnings_of(buf.str());
    if (ent.path().filename() == "32_newton_side.calc") {
      CHECK(has_text(ws, "eps"));
    } else {
      INFO(ent.path().filename().string());
      CHECK(ws.empty());
    }
  }
  CHECK(files == 40);
}
