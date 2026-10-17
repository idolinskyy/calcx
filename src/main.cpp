// Консольна точка входу: файл / stdin / інтерактивний REPL (-S / --dump-run).
#include "calc/disasm.hpp"
#include "calc/engine.hpp"
#include "calc/repl.hpp"
#include "calc/vm.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <unistd.h>

namespace {

/// Читає весь вміст потоку в рядок.
[[nodiscard]] std::string read_all(std::istream &in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

/// Друкує діагностику в stderr.
void report(const calc::Diagnostic &d) {
  std::cerr << "помилка:" << d.pos.line << ':' << d.pos.column << ": "
            << (d.message.empty() ? calc::error_message(d.code) : d.message)
            << " [" << static_cast<int>(d.code) << "]\n";
}

/// Друкує довідку з використання.
void print_usage(std::ostream &out, std::string_view prog) {
  out << "Використання: " << prog << " [опції] [файл]\n"
      << "\n"
      << "Без файла і опцій у терміналі — інтерактивний режим (REPL).\n"
      << "Якщо stdin не TTY (пайп) — читає всю програму зі stdin.\n"
      << "\n"
      << "Опції:\n"
      << "  -i, --interactive  примусово REPL (навіть якщо є файл — ігнорує)\n"
      << "  -S, --dump         лише компілювати і вивести байткод / пули\n"
      << "  --dump-run         як -S, плюс тихий прогін ВМ: тип і значення "
         "змінних\n"
      << "  --dump-temps       у дампі показати службові слоти for-in "
         "(__it/__i/__e)\n"
      << "  -h, --help         ця довідка\n"
      << "\n"
      << "Приклади:\n"
      << "  " << prog << "\n"
      << "  " << prog << " program.calc\n"
      << "  " << prog << " -S program.calc\n"
      << "  " << prog << " --dump-run program.calc\n"
      << "  " << prog << " -S --dump-temps program.calc\n";
}

struct Options {
  bool dump{false};
  bool dump_run{false};   ///< дамп + тихий прогін для типу/значення змінних
  bool dump_temps{false}; ///< показувати __it/__i/__e у секції змінних
  bool interactive{false};
  bool help{false};
  std::string input_path; ///< порожньо = stdin / REPL
};

/// Розбір аргументів командного рядка.
[[nodiscard]] std::optional<Options> parse_args(int argc, char **argv) {
  Options opt;
  const std::string_view prog = (argc > 0 && argv[0]) ? argv[0] : "calc";

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      opt.help = true;
      continue;
    }
    if (arg == "-i" || arg == "--interactive") {
      opt.interactive = true;
      continue;
    }
    if (arg == "-S" || arg == "--dump") {
      opt.dump = true;
      continue;
    }
    if (arg == "--dump-run") {
      opt.dump_run = true;
      continue;
    }
    if (arg == "--dump-temps") {
      opt.dump_temps = true;
      continue;
    }
    if (arg.starts_with('-')) {
      std::cerr << "Невідома опція: " << arg << '\n';
      print_usage(std::cerr, prog);
      return std::nullopt;
    }
    if (!opt.input_path.empty()) {
      std::cerr << "Занадто багато аргументів-файлів\n";
      print_usage(std::cerr, prog);
      return std::nullopt;
    }
    opt.input_path = std::string(arg);
  }
  return opt;
}

} // namespace

int main(int argc, char **argv) {
  const auto parsed = parse_args(argc, argv);
  if (!parsed) {
    return 1;
  }
  const Options &opt = *parsed;
  const std::string_view prog = (argc > 0 && argv[0]) ? argv[0] : "calc";

  if (opt.help) {
    print_usage(std::cout, prog);
    return 0;
  }

  const bool want_dump = opt.dump || opt.dump_run;
  if (opt.dump_temps && !want_dump) {
    std::cerr << "--dump-temps лише разом із -S/--dump або --dump-run\n";
    return 1;
  }

  // Інтерактив: явно -i, або немає файла/дампу і stdin — термінал.
  const bool tty = isatty(STDIN_FILENO) == 1;
  if (opt.interactive || (!want_dump && opt.input_path.empty() && tty)) {
    return calc::run_repl();
  }

  std::string source;
  if (!opt.input_path.empty()) {
    std::ifstream file(opt.input_path);
    if (!file) {
      std::cerr << "Не вдалося відкрити файл: " << opt.input_path << '\n';
      return 1;
    }
    source = read_all(file);
  } else {
    source = read_all(std::cin);
  }

  if (want_dump) {
    auto compiled = calc::compile_source(source);
    if (!compiled) {
      report(compiled.error());
      return 1;
    }
    if (opt.dump_run) {
      std::ostringstream discarded;
      auto ran = calc::execute_with_locals(*compiled, discarded);
      if (!ran) {
        report(ran.error());
        return 1;
      }
      calc::dump_chunk(*compiled, std::cout, ran->locals, opt.dump_temps);
    } else {
      calc::dump_chunk(*compiled, std::cout, {}, opt.dump_temps);
    }
    return 0;
  }

  auto result = calc::run(source, std::cout);
  if (!result) {
    report(result.error());
    return 1;
  }
  return 0;
}
