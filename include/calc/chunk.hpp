// Плаский байткод, пул констант, прототипи функцій.
#pragma once

#include "calc/value.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace calc {

enum class OpCode : std::uint8_t {
  Halt = 0,
  LoadConst,  ///< u16 index у constants
  LoadLocal,  ///< u16 slot
  StoreLocal, ///< u16 slot
  Neg,
  Pos, ///< унарний + : перевірка, що вершина — число
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Pow,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  Not,
  And,
  Or,
  Print,       ///< друк вершини стека + '\n'
  PrintFmt,    ///< u16 format_index, u8 arg_count
  Jump,        ///< u16 абсолютна адреса
  JumpIfFalse, ///< зняти Boolean; якщо false — стрибок
  Call,        ///< u16 function_index, u8 arg_count
  Return,      ///< повернути вершину стека з кадру
  CallBuiltin, ///< u16 BuiltinId, u8 arg_count
  MakeArray,   ///< u16 count — зняти count елементів, push Array
  MakeRecord,  ///< u16 count; далі count×u16 індексів імен; значення на стеку
  LoadIndex,   ///< pop index, pop array → elem
  StoreIndex,  ///< pop value, pop index, pop array (мутація)
  LoadField,   ///< u16 name_index; pop record → field
  StoreField,  ///< u16 name_index; pop value, pop record
  Pop,         ///< відкинути вершину стека (ExprStmt)
  MakeIter,    ///< u8 mode (1|2): coll → масив елементів/пар для for-in
};

/// Прототип скомпільованої функції користувача.
struct FunctionProto {
  std::string name;
  std::uint16_t arity{0};
  std::uint16_t local_count{0};
  std::uint16_t entry{0}; ///< зміщення в code
  std::vector<std::string> local_names;
};

struct Chunk {
  std::vector<std::uint8_t> code;
  std::vector<Value> constants;
  std::vector<std::string> formats;
  std::vector<std::string> names; ///< імена полів записів
  std::vector<FunctionProto> functions;
  std::uint16_t local_count{0}; ///< локалі головного коду (кадр 0)
  std::vector<std::string> local_names;

  void emit(OpCode op);
  void emit_u16(OpCode op, std::uint16_t operand);
  void emit_print_fmt(std::uint16_t format_index, std::uint8_t arg_count);
  /// Call / CallBuiltin: op + u16 index + u8 argc.
  void emit_call(OpCode op, std::uint16_t index, std::uint8_t arg_count);
  void emit_u16_raw(std::uint16_t operand);

  /// Індекс у пулі або nullopt, якщо вже 65536 елементів (u16 вичерпано).
  [[nodiscard]] std::optional<std::uint16_t> add_constant(Value v);
  [[nodiscard]] std::optional<std::uint16_t> add_format(std::string fmt);
  [[nodiscard]] std::optional<std::uint16_t> add_name(std::string name);
};

void patch_u16(Chunk &chunk, std::size_t offset, std::uint16_t value);

} // namespace calc
