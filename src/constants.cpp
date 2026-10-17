// Таблиця вбудованих констант (SI, де застосовно; CODATA-орієнтовні значення).
#include "calc/constants.hpp"

#include <cmath>
#include <numbers>
#include <span>

namespace calc {
namespace {

struct ConstEntry {
  std::string_view name;
  Value value;
};

[[nodiscard]] std::span<const ConstEntry> constant_table() {
  using std::numbers::e_v;
  using std::numbers::phi_v;
  using std::numbers::pi_v;

  static const Real pi = pi_v<Real>;
  static const Real e = e_v<Real>;
  static const Real phi = phi_v<Real>;
  static const Real tau = Real{2} * pi;
  static const Real ln2 = std::log(Real{2});
  static const Real ln10 = std::log(Real{10});
  static const Real sqrt2 = std::sqrt(Real{2});
  static const Real sqrt3 = std::sqrt(Real{3});

  static const Real c = Real{299792458};            // м/с (exact)
  static const Real G = Real{6.67430e-11L};         // м³/(кг·с²)
  static const Real h = Real{6.62607015e-34L};      // Дж·с (exact)
  static const Real hbar = h / tau;                 // Дж·с
  static const Real qe = Real{1.602176634e-19L};    // Кл (exact)
  static const Real me = Real{9.1093837015e-31L};   // кг
  static const Real mp = Real{1.67262192369e-27L};  // кг
  static const Real mn = Real{1.67492749804e-27L};  // кг
  static const Real u = Real{1.66053906660e-27L};   // кг (а.о.м.)
  static const Real NA = Real{6.02214076e23L};      // 1/моль (exact)
  static const Real kB = Real{1.380649e-23L};       // Дж/К (exact)
  static const Real Rgas = Real{8.314462618L};      // Дж/(моль·К)
  static const Real Farad = NA * qe;                // Кл/моль
  static const Real eps0 = Real{8.8541878128e-12L}; // Ф/м
  static const Real mu0 = Real{1.25663706212e-6L};  // Гн/м
  static const Real alpha = Real{7.2973525693e-3L};
  static const Real sigma = Real{5.670374419e-8L}; // Вт/(м²·К⁴)
  static const Real g = Real{9.80665L};            // м/с²
  static const Real atm = Real{101325};            // Па (exact)
  static const Real celsius0 = Real{273.15L};      // K
  static const Real Vm = Real{22.41396954e-3L};    // м³/моль
  static const Real Ry = Real{2.1798723611035e-18L};
  static const Real a0 = Real{5.29177210903e-11L};

  static const ConstEntry kTable[] = {
      // математика
      {"pi", Value{pi}},
      {"e", Value{e}},
      {"tau", Value{tau}},
      {"phi", Value{phi}},
      {"sqrt2", Value{sqrt2}},
      {"sqrt3", Value{sqrt3}},
      {"ln2", Value{ln2}},
      {"ln10", Value{ln10}},
      {"i", Value{Complex{Real{0}, Real{1}}}},

      // фізика
      {"c", Value{c}},
      {"G", Value{G}},
      {"h", Value{h}},
      {"hbar", Value{hbar}},
      {"qe", Value{qe}},
      {"me", Value{me}},
      {"mp", Value{mp}},
      {"mn", Value{mn}},
      {"u", Value{u}},
      {"eps0", Value{eps0}},
      {"mu0", Value{mu0}},
      {"alpha", Value{alpha}},
      {"sigma", Value{sigma}},
      {"g", Value{g}},
      {"atm", Value{atm}},
      {"Ry", Value{Ry}},
      {"a0", Value{a0}},

      // хімія / термодинаміка
      {"NA", Value{NA}},
      {"kB", Value{kB}},
      {"k", Value{kB}},
      {"R", Value{Rgas}},
      {"F", Value{Farad}},
      {"Vm", Value{Vm}},
      {"celsius0", Value{celsius0}},
  };
  return kTable;
}

} // namespace

std::optional<Value> find_constant(std::string_view name) {
  for (const auto &e : constant_table()) {
    if (e.name == name) {
      return e.value;
    }
  }
  return std::nullopt;
}

bool is_constant_name(std::string_view name) {
  return find_constant(name).has_value();
}

} // namespace calc
