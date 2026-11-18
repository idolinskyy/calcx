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

  auto prec = analyze_format("{:.{}f}");
  REQUIRE(prec);
  CHECK(prec->arg_count == 2);

  auto both = analyze_format("{:{}.{}f}");
  REQUIRE(both);
  CHECK(both->arg_count == 3);

  auto manual = analyze_format("{1:.{0}f}");
  REQUIRE(manual);
  CHECK(manual->arg_count == 2);

  auto phrase = analyze_format("до {} — {:.{}f}");
  REQUIRE(phrase);
  CHECK(phrase->arg_count == 3);

  CHECK_FALSE(analyze_format("{:.{}f} {0}"));
  CHECK_FALSE(analyze_format("{0:.{}f}"));
  CHECK_FALSE(analyze_format("{:.{5.2}f}"));
  CHECK_FALSE(analyze_format("{:{:{}}}"));
  CHECK_FALSE(analyze_format("{:.{}"));
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
  CHECK(run_ok("print(\"{:.{}f}\", 1.5, 3);") == "1.500\n");
  CHECK(run_ok("print(\"{:.{}f}\", 1.5, 0);") == "2\n");
  CHECK(run_ok("print(\"{:{}}\", 42, 6);") == "    42\n");
  CHECK(run_ok("print(\"{:0{}}\", 42, 4);") == "0042\n");
  CHECK(run_ok("print(\"{:{}.{}f}\", 1.5, 8, 2);") == "    1.50\n");
  CHECK(run_ok("print(\"{1:.{0}f}\", 3, 1.5);") == "1.500\n");
  CHECK(run_ok("print(\"до {} — {:.{}f}\", 2, 1.25, 2);") == "до 2 — 1.25\n");
  CHECK(run_ok("for (let n = 0; n < 3; ++n) { print(\"{:.{}f}\", 1.5, n); }") ==
        "2\n1.5\n1.50\n");

  std::ostringstream err;
  CHECK_FALSE(run("print(\"{:.{}f}\", 1.5);", err));
  CHECK_FALSE(run("print(\"{:.{}f}\", 1.5, -1);", err));
  CHECK_FALSE(run("print(\"{:.{}f}\", 1.5, 1.5);", err));
  CHECK_FALSE(run("print(\"{:.{}f}\", 1.5, true);", err));
}
