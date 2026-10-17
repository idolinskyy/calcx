// Строгі тести форматування print у стилі std::format / std::print.
#include "calc/engine.hpp"
#include "calc/format.hpp"

#include <sstream>
#include <string>

#include <doctest/doctest.h>

using calc::analyze_format;
using calc::ErrorCode;
using calc::format_print;
using calc::Integer;
using calc::Real;
using calc::run;
using calc::Value;

namespace {

[[nodiscard]] std::string run_ok(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE(r);
  return out.str();
}

} // namespace

TEST_CASE("analyze_format auto and manual indices") {
  auto a = analyze_format("x={} y={}");
  REQUIRE(a);
  CHECK(a->arg_count == 2);

  auto m = analyze_format("{1} {0}");
  REQUIRE(m);
  CHECK(m->arg_count == 2);

  auto reuse = analyze_format("{0} {0}");
  REQUIRE(reuse);
  CHECK(reuse->arg_count == 1);

  auto spec = analyze_format("{:.2f} {:d}");
  REQUIRE(spec);
  CHECK(spec->arg_count == 2);

  CHECK_FALSE(analyze_format("{} {0}")); // змішування
  CHECK_FALSE(analyze_format("{"));      // незакритий
}

TEST_CASE("format_print specs like std::format") {
  Value args[] = {Value{Integer{42}}, Value{Real{3.14159L}}};
  auto s = format_print("n={:d} pi={:.2f}", args);
  REQUIRE(s);
  CHECK(*s == "n=42 pi=3.14");

  Value one[] = {Value{Integer{7}}};
  auto r = format_print("{0} again {0}", one);
  REQUIRE(r);
  CHECK(*r == "7 again 7");

  Value ab[] = {Value{Integer{10}}, Value{Integer{20}}};
  auto swap = format_print("{1} {0}", ab);
  REQUIRE(swap);
  CHECK(*swap == "20 10");
}

TEST_CASE("e2e print formatting") {
  CHECK(run_ok("print(\"n={:d}\", 42);") == "n=42\n");
  CHECK(run_ok("print(\"pi={:.2f}\", 22 / 7);") == "pi=3.14\n");
  CHECK(run_ok("print(\"{:>6}\", 42);") == "    42\n");
  CHECK(run_ok("print(\"{:x}\", 255);") == "ff\n");
  CHECK(run_ok("print(\"{{}}\");") == "{}\n");
  CHECK(run_ok("print(\"{1} then {0}\", 1, 2);") == "2 then 1\n");
  CHECK(run_ok("print(\"{0}/{0}\", 5);") == "5/5\n");
  CHECK(run_ok("print(\"half={}\", 1/2);") == "half=1/2\n");
  CHECK(run_ok("print(\"half={:.1f}\", 1/2);") == "half=0.5\n");
  CHECK(run_ok("print(\"ok={}\", true);") == "ok=true\n");
}
