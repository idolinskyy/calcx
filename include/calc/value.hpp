// Числові, логічні, масиви та записи.
#pragma once

#include "calc/error.hpp"

#include <complex>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace calc {

using Boolean = bool;
using Integer = std::int64_t;
using Real = long double;
using Complex = std::complex<Real>;
using String = std::string;

struct Rational {
  Integer num{0};
  Integer den{1};

  [[nodiscard]] friend bool operator==(const Rational&,
                                       const Rational&) = default;
};

struct ArrayData;
struct RecordData;

/// Масив за посиланням (shared): `let b = a` ділить дані з `a`.
struct Array {
  std::shared_ptr<ArrayData> data;
  Array();
  Array(const Array&) = default;
  Array(Array&&) noexcept = default;
  Array& operator=(const Array&) = default;
  Array& operator=(Array&&) noexcept = default;
  ~Array();
};

/// Запис/обʼєкт за посиланням: `{ x: 1, y: 2 }`.
struct Record {
  std::shared_ptr<RecordData> data;
  Record();
  Record(const Record&) = default;
  Record(Record&&) noexcept = default;
  Record& operator=(const Record&) = default;
  Record& operator=(Record&&) noexcept = default;
  ~Record();
};

using Value = std::variant<Boolean, Integer, Rational, Real, Complex, Array,
                           Record, String>;

using ArithResult = std::expected<Value, ErrorCode>;

[[nodiscard]] Array make_array(std::vector<Value> items);
[[nodiscard]] std::vector<Value>& array_items(Array& a);
[[nodiscard]] const std::vector<Value>& array_items(const Array& a);

[[nodiscard]] Record make_record(std::map<std::string, Value> fields);
[[nodiscard]] std::map<std::string, Value>& record_fields(Record& r);
[[nodiscard]] const std::map<std::string, Value>& record_fields(const Record& r);

/// Підготовка ітерації: mode 1 → елементи/ключі; mode 2 → пари [i,v] / [k,v].
[[nodiscard]] ArithResult make_iter_items(const Value& coll, std::uint8_t mode);

[[nodiscard]] ArithResult make_rational(Integer num, Integer den);

[[nodiscard]] bool is_boolean(const Value& v);
[[nodiscard]] bool is_integer(const Value& v);
[[nodiscard]] bool is_rational(const Value& v);
[[nodiscard]] bool is_real(const Value& v);
[[nodiscard]] bool is_complex(const Value& v);
[[nodiscard]] bool is_array(const Value& v);
[[nodiscard]] bool is_record(const Value& v);
[[nodiscard]] bool is_string(const Value& v);

[[nodiscard]] std::string_view value_type_name(const Value& v);

[[nodiscard]] ArithResult negate(const Value& v);
[[nodiscard]] ArithResult add(const Value& a, const Value& b);
[[nodiscard]] ArithResult sub(const Value& a, const Value& b);
[[nodiscard]] ArithResult mul(const Value& a, const Value& b);
[[nodiscard]] ArithResult div(const Value& a, const Value& b);
[[nodiscard]] ArithResult mod(const Value& a, const Value& b);
[[nodiscard]] ArithResult pow_op(const Value& base, const Value& exp);

[[nodiscard]] ArithResult eq(const Value& a, const Value& b);
[[nodiscard]] ArithResult ne(const Value& a, const Value& b);
[[nodiscard]] ArithResult lt(const Value& a, const Value& b);
[[nodiscard]] ArithResult le(const Value& a, const Value& b);
[[nodiscard]] ArithResult gt(const Value& a, const Value& b);
[[nodiscard]] ArithResult ge(const Value& a, const Value& b);

[[nodiscard]] ArithResult logical_not(const Value& v);
[[nodiscard]] ArithResult logical_and(const Value& a, const Value& b);
[[nodiscard]] ArithResult logical_or(const Value& a, const Value& b);

[[nodiscard]] ArithResult array_get(const Value& arr, const Value& index);
[[nodiscard]] ArithResult array_set(Value& arr, const Value& index, Value value);
[[nodiscard]] ArithResult record_get(const Value& rec, std::string_view field);
[[nodiscard]] ArithResult record_set(Value& rec, std::string_view field,
                                     Value value);

[[nodiscard]] std::string format_value(const Value& v);
void print_value(std::ostream& os, const Value& v);

}  // namespace calc
