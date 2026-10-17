// Парсер: рекурсивний спуск токенів у AST.
#include "calc/parser.hpp"

#include <utility>

namespace calc {
namespace {

class Parser {
public:
  explicit Parser(const std::vector<Token> &tokens) : tokens_(tokens) {}

  [[nodiscard]] ParseResult parse_program() {
    Program program;
    while (!check(TokenKind::End)) {
      auto stmt = parse_statement();
      if (!stmt) {
        return std::unexpected(stmt.error());
      }
      program.statements.push_back(std::move(*stmt));
    }
    return program;
  }

private:
  const std::vector<Token> &tokens_;
  std::size_t current_{0};

  [[nodiscard]] const Token &peek() const { return tokens_[current_]; }

  [[nodiscard]] const Token &previous() const { return tokens_[current_ - 1]; }

  [[nodiscard]] bool check(TokenKind kind) const { return peek().kind == kind; }

  [[nodiscard]] bool is_at_end() const { return check(TokenKind::End); }

  const Token &advance() {
    if (!is_at_end()) {
      ++current_;
    }
    return previous();
  }

  [[nodiscard]] bool match(TokenKind kind) {
    if (check(kind)) {
      advance();
      return true;
    }
    return false;
  }

  [[nodiscard]] Diagnostic error(ErrorCode code, SourcePos pos,
                                 std::string message) const {
    return Diagnostic{code, pos, std::move(message)};
  }

  [[nodiscard]] std::expected<Token, Diagnostic>
  consume(TokenKind kind, ErrorCode code, std::string message) {
    if (check(kind)) {
      return advance();
    }
    return std::unexpected(error(code, peek().pos, std::move(message)));
  }

  /// Оператор: let | assign | print | if | while | for | fun | return |
  /// break | continue | `{`.
  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_statement() {
    if (match(TokenKind::Let)) {
      return parse_let(/*require_semi=*/true);
    }
    if (match(TokenKind::Print)) {
      return parse_print();
    }
    if (match(TokenKind::If)) {
      return parse_if();
    }
    if (match(TokenKind::While)) {
      return parse_while();
    }
    if (match(TokenKind::For)) {
      return parse_for();
    }
    if (match(TokenKind::Fun)) {
      return parse_fun();
    }
    if (match(TokenKind::Return)) {
      return parse_return();
    }
    if (match(TokenKind::Break)) {
      const SourcePos pos = previous().pos;
      auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                          "Очікується ';'");
      if (!semi) {
        return std::unexpected(semi.error());
      }
      return make_stmt(BreakStmt{pos});
    }
    if (match(TokenKind::Continue)) {
      const SourcePos pos = previous().pos;
      auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                          "Очікується ';'");
      if (!semi) {
        return std::unexpected(semi.error());
      }
      return make_stmt(ContinueStmt{pos});
    }
    if (match(TokenKind::LBrace)) {
      return parse_block();
    }
    if (looks_like_assignment()) {
      return parse_assign(/*require_semi=*/true);
    }
    // Вираз-оператор (виклик тощо), інакше — помилка.
    {
      const SourcePos pos = peek().pos;
      auto expr = parse_expression();
      if (!expr) {
        return std::unexpected(
            error(ErrorCode::UnexpectedToken, pos, "Очікується оператор"));
      }
      auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                          "Очікується ';'");
      if (!semi) {
        return std::unexpected(semi.error());
      }
      return make_stmt(ExprStmt{std::move(*expr), pos});
    }
  }

  /// Чи далі йде присвоєння: ident ([expr]| .ident)* =
  [[nodiscard]] bool looks_like_assignment() const {
    if (!check(TokenKind::Identifier)) {
      return false;
    }
    std::size_t i = current_ + 1;
    while (i < tokens_.size()) {
      if (tokens_[i].kind == TokenKind::LBracket) {
        int depth = 1;
        ++i;
        while (i < tokens_.size() && depth > 0) {
          if (tokens_[i].kind == TokenKind::LBracket) {
            ++depth;
          } else if (tokens_[i].kind == TokenKind::RBracket) {
            --depth;
          }
          ++i;
        }
        continue;
      }
      if (tokens_[i].kind == TokenKind::Dot && i + 1 < tokens_.size() &&
          tokens_[i + 1].kind == TokenKind::Identifier) {
        i += 2;
        continue;
      }
      break;
    }
    return i < tokens_.size() && tokens_[i].kind == TokenKind::Equal;
  }

  [[nodiscard]] std::expected<StmtPtr, Diagnostic>
  parse_let(bool require_semi) {
    const SourcePos pos = previous().pos;
    LetStmt stmt;
    stmt.pos = pos;

    do {
      auto name_tok = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                              "Очікується імʼя змінної");
      if (!name_tok) {
        return std::unexpected(name_tok.error());
      }

      if (!match(TokenKind::Equal)) {
        return std::unexpected(error(ErrorCode::MissingInitializer,
                                     name_tok->pos,
                                     "Змінна має бути ініціалізована"));
      }

      auto init = parse_expression();
      if (!init) {
        return std::unexpected(init.error());
      }

      LetBinding binding;
      binding.name = std::string(name_tok->text);
      binding.name_pos = name_tok->pos;
      binding.init = std::move(*init);
      stmt.bindings.push_back(std::move(binding));
    } while (match(TokenKind::Comma));

    if (require_semi) {
      auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                          "Очікується ';'");
      if (!semi) {
        return std::unexpected(semi.error());
      }
    }
    return make_stmt(std::move(stmt));
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_lvalue() {
    auto name_tok = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                            "Очікується імʼя");
    if (!name_tok) {
      return std::unexpected(name_tok.error());
    }
    auto node = std::make_unique<Expr>();
    node->node = VarExpr{std::string(name_tok->text), name_tok->pos};
    ExprPtr expr = std::move(node);
    while (true) {
      if (match(TokenKind::LBracket)) {
        const SourcePos pos = previous().pos;
        auto index = parse_expression();
        if (!index) {
          return std::unexpected(index.error());
        }
        auto rb = consume(TokenKind::RBracket, ErrorCode::ExpectedToken,
                          "Очікується ']'");
        if (!rb) {
          return std::unexpected(rb.error());
        }
        auto n = std::make_unique<Expr>();
        n->node = IndexExpr{std::move(expr), std::move(*index), pos};
        expr = std::move(n);
        continue;
      }
      if (match(TokenKind::Dot)) {
        const SourcePos pos = previous().pos;
        auto field = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                             "Очікується імʼя поля");
        if (!field) {
          return std::unexpected(field.error());
        }
        auto n = std::make_unique<Expr>();
        n->node = FieldExpr{std::move(expr), std::string(field->text),
                            field->pos, pos};
        expr = std::move(n);
        continue;
      }
      break;
    }
    return expr;
  }

  [[nodiscard]] std::expected<StmtPtr, Diagnostic>
  parse_assign(bool require_semi) {
    auto target = parse_lvalue();
    if (!target) {
      return std::unexpected(target.error());
    }
    auto eq =
        consume(TokenKind::Equal, ErrorCode::ExpectedToken, "Очікується '='");
    if (!eq) {
      return std::unexpected(eq.error());
    }
    auto value = parse_expression();
    if (!value) {
      return std::unexpected(value.error());
    }
    if (require_semi) {
      auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                          "Очікується ';'");
      if (!semi) {
        return std::unexpected(semi.error());
      }
    }
    return make_stmt(
        AssignStmt{std::move(*target), std::move(*value), eq->pos});
  }

  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_print() {
    const SourcePos pos = previous().pos;
    auto lparen =
        consume(TokenKind::LParen, ErrorCode::ExpectedToken, "Очікується '('");
    if (!lparen) {
      return std::unexpected(lparen.error());
    }

    PrintStmt stmt;
    stmt.pos = pos;

    if (check(TokenKind::String)) {
      const Token &fmt = advance();
      stmt.has_format = true;
      stmt.format = fmt.string_value;
      stmt.format_pos = fmt.pos;
      while (match(TokenKind::Comma)) {
        auto arg = parse_expression();
        if (!arg) {
          return std::unexpected(arg.error());
        }
        stmt.args.push_back(std::move(*arg));
      }
    } else {
      auto arg = parse_expression();
      if (!arg) {
        return std::unexpected(arg.error());
      }
      stmt.args.push_back(std::move(*arg));
    }

    auto rparen =
        consume(TokenKind::RParen, ErrorCode::ExpectedToken, "Очікується ')'");
    if (!rparen) {
      return std::unexpected(rparen.error());
    }
    auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                        "Очікується ';'");
    if (!semi) {
      return std::unexpected(semi.error());
    }
    return make_stmt(std::move(stmt));
  }

  /// if (cond) stmt [else stmt] — else чіпляється до найближчого if.
  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_if() {
    const SourcePos pos = previous().pos;
    auto lparen =
        consume(TokenKind::LParen, ErrorCode::ExpectedToken, "Очікується '('");
    if (!lparen) {
      return std::unexpected(lparen.error());
    }
    auto cond = parse_expression();
    if (!cond) {
      return std::unexpected(cond.error());
    }
    auto rparen =
        consume(TokenKind::RParen, ErrorCode::ExpectedToken, "Очікується ')'");
    if (!rparen) {
      return std::unexpected(rparen.error());
    }

    auto then_branch = parse_statement();
    if (!then_branch) {
      return std::unexpected(then_branch.error());
    }

    StmtPtr else_branch;
    if (match(TokenKind::Else)) {
      auto e = parse_statement();
      if (!e) {
        return std::unexpected(e.error());
      }
      else_branch = std::move(*e);
    }

    return make_stmt(IfStmt{std::move(*cond), std::move(*then_branch),
                            std::move(else_branch), pos});
  }

  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_while() {
    const SourcePos pos = previous().pos;
    auto lparen =
        consume(TokenKind::LParen, ErrorCode::ExpectedToken, "Очікується '('");
    if (!lparen) {
      return std::unexpected(lparen.error());
    }
    auto cond = parse_expression();
    if (!cond) {
      return std::unexpected(cond.error());
    }
    auto rparen =
        consume(TokenKind::RParen, ErrorCode::ExpectedToken, "Очікується ')'");
    if (!rparen) {
      return std::unexpected(rparen.error());
    }
    auto body = parse_statement();
    if (!body) {
      return std::unexpected(body.error());
    }
    return make_stmt(WhileStmt{std::move(*cond), std::move(*body), pos});
  }

  [[nodiscard]] bool looks_like_for_in() const {
    if (!check(TokenKind::Identifier)) {
      return false;
    }
    std::size_t i = current_ + 1;
    if (i < tokens_.size() && tokens_[i].kind == TokenKind::In) {
      return true;
    }
    if (i + 2 < tokens_.size() && tokens_[i].kind == TokenKind::Comma &&
        tokens_[i + 1].kind == TokenKind::Identifier &&
        tokens_[i + 2].kind == TokenKind::In) {
      return true;
    }
    return false;
  }

  /// for (x in xs) / for (a, b in xs) / for (init; cond; step)
  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_for() {
    const SourcePos pos = previous().pos;
    auto lparen =
        consume(TokenKind::LParen, ErrorCode::ExpectedToken, "Очікується '('");
    if (!lparen) {
      return std::unexpected(lparen.error());
    }

    if (looks_like_for_in()) {
      auto first = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                           "Очікується імʼя");
      if (!first) {
        return std::unexpected(first.error());
      }
      std::string second;
      SourcePos second_pos{};
      if (match(TokenKind::Comma)) {
        auto sec = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                           "Очікується імʼя");
        if (!sec) {
          return std::unexpected(sec.error());
        }
        second = std::string(sec->text);
        second_pos = sec->pos;
      }
      auto in_tok =
          consume(TokenKind::In, ErrorCode::ExpectedToken, "Очікується 'in'");
      if (!in_tok) {
        return std::unexpected(in_tok.error());
      }
      auto iterable = parse_expression();
      if (!iterable) {
        return std::unexpected(iterable.error());
      }
      auto rparen = consume(TokenKind::RParen, ErrorCode::ExpectedToken,
                            "Очікується ')'");
      if (!rparen) {
        return std::unexpected(rparen.error());
      }
      auto body = parse_statement();
      if (!body) {
        return std::unexpected(body.error());
      }
      return make_stmt(ForInStmt{std::string(first->text), first->pos,
                                 std::move(second), second_pos,
                                 std::move(*iterable), std::move(*body), pos});
    }

    StmtPtr init;
    if (match(TokenKind::Semicolon)) {
      // порожній init
    } else if (match(TokenKind::Let)) {
      auto let = parse_let(/*require_semi=*/true);
      if (!let) {
        return std::unexpected(let.error());
      }
      init = std::move(*let);
    } else if (looks_like_assignment()) {
      auto asg = parse_assign(/*require_semi=*/true);
      if (!asg) {
        return std::unexpected(asg.error());
      }
      init = std::move(*asg);
    } else {
      return std::unexpected(error(ErrorCode::UnexpectedToken, peek().pos,
                                   "Очікується init for або ';'"));
    }

    ExprPtr condition;
    if (!check(TokenKind::Semicolon)) {
      auto cond = parse_expression();
      if (!cond) {
        return std::unexpected(cond.error());
      }
      condition = std::move(*cond);
    }
    auto semi2 = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                         "Очікується ';'");
    if (!semi2) {
      return std::unexpected(semi2.error());
    }

    StmtPtr step;
    if (!check(TokenKind::RParen)) {
      if (!looks_like_assignment()) {
        return std::unexpected(error(ErrorCode::UnexpectedToken, peek().pos,
                                     "Очікується присвоєння у step for"));
      }
      auto asg = parse_assign(/*require_semi=*/false);
      if (!asg) {
        return std::unexpected(asg.error());
      }
      step = std::move(*asg);
    }

    auto rparen =
        consume(TokenKind::RParen, ErrorCode::ExpectedToken, "Очікується ')'");
    if (!rparen) {
      return std::unexpected(rparen.error());
    }
    auto body = parse_statement();
    if (!body) {
      return std::unexpected(body.error());
    }
    return make_stmt(ForStmt{std::move(init), std::move(condition),
                             std::move(step), std::move(*body), pos});
  }

  /// fun name(params) stmt
  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_fun() {
    const SourcePos pos = previous().pos;
    auto name_tok = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                            "Очікується імʼя функції");
    if (!name_tok) {
      return std::unexpected(name_tok.error());
    }
    auto lparen =
        consume(TokenKind::LParen, ErrorCode::ExpectedToken, "Очікується '('");
    if (!lparen) {
      return std::unexpected(lparen.error());
    }

    std::vector<FunParam> params;
    if (!check(TokenKind::RParen)) {
      do {
        auto p = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                         "Очікується імʼя параметра");
        if (!p) {
          return std::unexpected(p.error());
        }
        params.push_back(FunParam{std::string(p->text), p->pos});
      } while (match(TokenKind::Comma));
    }
    auto rparen =
        consume(TokenKind::RParen, ErrorCode::ExpectedToken, "Очікується ')'");
    if (!rparen) {
      return std::unexpected(rparen.error());
    }
    auto body = parse_statement();
    if (!body) {
      return std::unexpected(body.error());
    }
    return make_stmt(FunStmt{std::string(name_tok->text), name_tok->pos,
                             std::move(params), std::move(*body), pos});
  }

  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_return() {
    const SourcePos pos = previous().pos;
    ExprPtr value;
    if (!check(TokenKind::Semicolon)) {
      auto v = parse_expression();
      if (!v) {
        return std::unexpected(v.error());
      }
      value = std::move(*v);
    }
    auto semi = consume(TokenKind::Semicolon, ErrorCode::ExpectedToken,
                        "Очікується ';'");
    if (!semi) {
      return std::unexpected(semi.error());
    }
    return make_stmt(ReturnStmt{std::move(value), pos});
  }

  /// Блок `{ stmt* }` — вже зʼїли `{`.
  [[nodiscard]] std::expected<StmtPtr, Diagnostic> parse_block() {
    const SourcePos pos = previous().pos;
    BlockStmt block;
    block.pos = pos;
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
      auto stmt = parse_statement();
      if (!stmt) {
        return std::unexpected(stmt.error());
      }
      block.statements.push_back(std::move(*stmt));
    }
    auto rbrace =
        consume(TokenKind::RBrace, ErrorCode::ExpectedToken, "Очікується '}'");
    if (!rbrace) {
      return std::unexpected(rbrace.error());
    }
    return make_stmt(std::move(block));
  }

  /// Пріоритет (від нижчого): or → and → ==/!= → порівняння → +/− → */% →
  /// унарні → ** (правоасоц.) → primary.
  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_expression() {
    return parse_or();
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic>
  parse_binary_left(auto parse_higher, std::initializer_list<TokenKind> ops) {
    auto left = parse_higher();
    if (!left) {
      return left;
    }
    while (true) {
      bool matched = false;
      for (TokenKind k : ops) {
        if (check(k)) {
          matched = true;
          break;
        }
      }
      if (!matched) {
        break;
      }
      const Token op = advance();
      auto right = parse_higher();
      if (!right) {
        return right;
      }
      auto node = std::make_unique<Expr>();
      node->node =
          BinaryExpr{op.kind, std::move(*left), std::move(*right), op.pos};
      left = std::move(node);
    }
    return left;
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_or() {
    return parse_binary_left([this] { return parse_and(); }, {TokenKind::Or});
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_and() {
    return parse_binary_left([this] { return parse_equality(); },
                             {TokenKind::And});
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_equality() {
    return parse_binary_left([this] { return parse_comparison(); },
                             {TokenKind::EqEq, TokenKind::NotEq});
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_comparison() {
    return parse_binary_left([this] { return parse_additive(); },
                             {TokenKind::Less, TokenKind::LessEq,
                              TokenKind::Greater, TokenKind::GreaterEq});
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_additive() {
    return parse_binary_left([this] { return parse_multiplicative(); },
                             {TokenKind::Plus, TokenKind::Minus});
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_multiplicative() {
    return parse_binary_left(
        [this] { return parse_unary(); },
        {TokenKind::Star, TokenKind::Slash, TokenKind::Percent});
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_unary() {
    if (check(TokenKind::Plus) || check(TokenKind::Minus) ||
        check(TokenKind::Not)) {
      const Token op = advance();
      auto operand = parse_unary();
      if (!operand) {
        return operand;
      }
      auto node = std::make_unique<Expr>();
      node->node = UnaryExpr{op.kind, std::move(*operand), op.pos};
      return node;
    }
    return parse_power();
  }

  /// Степінь правоасоціативна: 2 ** 3 ** 2 → 2 ** (3 ** 2).
  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_power() {
    auto left = parse_postfix();
    if (!left) {
      return left;
    }
    if (match(TokenKind::StarStar)) {
      const Token op = previous();
      auto right = parse_unary(); // унарний мінус у показнику: 2 ** -1
      if (!right) {
        return right;
      }
      auto node = std::make_unique<Expr>();
      node->node =
          BinaryExpr{op.kind, std::move(*left), std::move(*right), op.pos};
      return node;
    }
    return left;
  }

  /// postfix: primary ([index] | .field)*
  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_postfix() {
    auto expr = parse_primary();
    if (!expr) {
      return expr;
    }
    while (true) {
      if (match(TokenKind::LBracket)) {
        const SourcePos pos = previous().pos;
        auto index = parse_expression();
        if (!index) {
          return std::unexpected(index.error());
        }
        auto rb = consume(TokenKind::RBracket, ErrorCode::ExpectedToken,
                          "Очікується ']'");
        if (!rb) {
          return std::unexpected(rb.error());
        }
        auto n = std::make_unique<Expr>();
        n->node = IndexExpr{std::move(*expr), std::move(*index), pos};
        expr = std::move(n);
        continue;
      }
      if (match(TokenKind::Dot)) {
        const SourcePos pos = previous().pos;
        auto field = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                             "Очікується імʼя поля");
        if (!field) {
          return std::unexpected(field.error());
        }
        auto n = std::make_unique<Expr>();
        n->node = FieldExpr{std::move(*expr), std::string(field->text),
                            field->pos, pos};
        expr = std::move(n);
        continue;
      }
      break;
    }
    return expr;
  }

  [[nodiscard]] std::expected<ExprPtr, Diagnostic> parse_primary() {
    if (match(TokenKind::True)) {
      auto node = std::make_unique<Expr>();
      node->node = LiteralExpr{Value{Boolean{true}}, previous().pos};
      return node;
    }
    if (match(TokenKind::False)) {
      auto node = std::make_unique<Expr>();
      node->node = LiteralExpr{Value{Boolean{false}}, previous().pos};
      return node;
    }
    if (match(TokenKind::IntegerLit)) {
      auto node = std::make_unique<Expr>();
      node->node = LiteralExpr{Value{static_cast<Integer>(previous().number)},
                               previous().pos};
      return node;
    }
    if (match(TokenKind::RationalLit)) {
      auto made = make_rational(previous().rat_num, previous().rat_den);
      if (!made) {
        return std::unexpected(error(made.error(), previous().pos,
                                     "Некоректний раціональний літерал"));
      }
      auto node = std::make_unique<Expr>();
      node->node = LiteralExpr{*made, previous().pos};
      return node;
    }
    if (match(TokenKind::RealLit)) {
      auto node = std::make_unique<Expr>();
      node->node = LiteralExpr{Value{static_cast<Real>(previous().number)},
                               previous().pos};
      return node;
    }
    if (match(TokenKind::ImaginaryLit)) {
      auto node = std::make_unique<Expr>();
      node->node = LiteralExpr{
          Value{Complex{Real{0}, static_cast<Real>(previous().number)}},
          previous().pos};
      return node;
    }
    if (match(TokenKind::Identifier)) {
      const std::string name(previous().text);
      const SourcePos name_pos = previous().pos;
      if (match(TokenKind::LParen)) {
        std::vector<ExprPtr> args;
        if (!check(TokenKind::RParen)) {
          do {
            auto arg = parse_expression();
            if (!arg) {
              return std::unexpected(arg.error());
            }
            args.push_back(std::move(*arg));
          } while (match(TokenKind::Comma));
        }
        auto rparen = consume(TokenKind::RParen, ErrorCode::ExpectedToken,
                              "Очікується ')'");
        if (!rparen) {
          return std::unexpected(rparen.error());
        }
        auto node = std::make_unique<Expr>();
        node->node = CallExpr{name, name_pos, std::move(args), name_pos};
        return node;
      }
      auto node = std::make_unique<Expr>();
      node->node = VarExpr{name, name_pos};
      return node;
    }
    if (match(TokenKind::LBracket)) {
      const SourcePos pos = previous().pos;
      std::vector<ExprPtr> elements;
      if (!check(TokenKind::RBracket)) {
        do {
          auto el = parse_expression();
          if (!el) {
            return std::unexpected(el.error());
          }
          elements.push_back(std::move(*el));
        } while (match(TokenKind::Comma));
      }
      auto rb = consume(TokenKind::RBracket, ErrorCode::ExpectedToken,
                        "Очікується ']'");
      if (!rb) {
        return std::unexpected(rb.error());
      }
      auto node = std::make_unique<Expr>();
      node->node = ArrayExpr{std::move(elements), pos};
      return node;
    }
    if (match(TokenKind::LBrace)) {
      // У виразі `{` — літерал запису (не блок).
      const SourcePos pos = previous().pos;
      std::vector<RecordField> fields;
      if (!check(TokenKind::RBrace)) {
        do {
          auto name = consume(TokenKind::Identifier, ErrorCode::ExpectedToken,
                              "Очікується імʼя поля");
          if (!name) {
            return std::unexpected(name.error());
          }
          auto colon = consume(TokenKind::Colon, ErrorCode::ExpectedToken,
                               "Очікується ':'");
          if (!colon) {
            return std::unexpected(colon.error());
          }
          auto val = parse_expression();
          if (!val) {
            return std::unexpected(val.error());
          }
          fields.push_back(
              RecordField{std::string(name->text), name->pos, std::move(*val)});
        } while (match(TokenKind::Comma));
      }
      auto rb = consume(TokenKind::RBrace, ErrorCode::ExpectedToken,
                        "Очікується '}'");
      if (!rb) {
        return std::unexpected(rb.error());
      }
      auto node = std::make_unique<Expr>();
      node->node = RecordExpr{std::move(fields), pos};
      return node;
    }
    if (match(TokenKind::LParen)) {
      auto inner = parse_expression();
      if (!inner) {
        return inner;
      }
      auto rparen = consume(TokenKind::RParen, ErrorCode::ExpectedToken,
                            "Очікується ')'");
      if (!rparen) {
        return std::unexpected(rparen.error());
      }
      return inner;
    }
    if (check(TokenKind::String)) {
      return std::unexpected(error(ErrorCode::StringInExpression, peek().pos,
                                   "Рядок дозволений лише у print"));
    }
    return std::unexpected(
        error(ErrorCode::UnexpectedToken, peek().pos, "Очікується вираз"));
  }
};

} // namespace

ParseResult parse(const std::vector<Token> &tokens) {
  if (tokens.empty()) {
    return std::unexpected(
        Diagnostic{ErrorCode::EmptyInput, {}, "Порожній вхід"});
  }
  Parser parser(tokens);
  return parser.parse_program();
}

} // namespace calc
