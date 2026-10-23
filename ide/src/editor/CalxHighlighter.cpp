#include "editor/CalxHighlighter.h"

#include "calc/builtin.hpp"
#include "calc/constants.hpp"
#include "calc/lexer.hpp"
#include "calc/token.hpp"

#include <QColor>

#include <algorithm>
#include <string>

namespace {

/// Байтовий зсув у UTF-8 → індекс у QString (UTF-16).
[[nodiscard]] int qIndexFromUtf8(const std::string &utf8,
                                 std::size_t byte_off) {
  if (byte_off == 0) {
    return 0;
  }
  if (byte_off > utf8.size()) {
    byte_off = utf8.size();
  }
  return static_cast<int>(
      QString::fromUtf8(utf8.data(), static_cast<qsizetype>(byte_off)).size());
}

[[nodiscard]] int qLenFromUtf8(const std::string &utf8, std::size_t byte_off,
                               std::size_t byte_len) {
  if (byte_off >= utf8.size()) {
    return 0;
  }
  const std::size_t n = std::min(byte_len, utf8.size() - byte_off);
  return static_cast<int>(
      QString::fromUtf8(utf8.data() + byte_off, static_cast<qsizetype>(n))
          .size());
}

} // namespace

CalxHighlighter::CalxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  applyFormats();
}

void CalxHighlighter::setDark(bool dark) {
  if (dark_ == dark) {
    return;
  }
  dark_ = dark;
  applyFormats();
  rehighlight();
}

void CalxHighlighter::applyFormats() {
  if (dark_) {
    keywordFmt_.setForeground(QColor(QStringLiteral("#79c0ff")));
    keywordFmt_.setFontWeight(QFont::Bold);
    literalFmt_.setForeground(QColor(QStringLiteral("#ffa657")));
    stringFmt_.setForeground(QColor(QStringLiteral("#a5d6ff")));
    builtinFmt_.setForeground(QColor(QStringLiteral("#7ee787")));
    constantFmt_.setForeground(QColor(QStringLiteral("#79c0ff")));
    commentFmt_.setForeground(QColor(QStringLiteral("#8b949e")));
    operatorFmt_.setForeground(QColor(QStringLiteral("#ff7b72")));
    punctFmt_.setForeground(QColor(QStringLiteral("#c9d1d9")));
  } else {
    keywordFmt_.setForeground(QColor(QStringLiteral("#0550ae")));
    keywordFmt_.setFontWeight(QFont::Bold);
    literalFmt_.setForeground(QColor(QStringLiteral("#953800")));
    stringFmt_.setForeground(QColor(QStringLiteral("#0a3069")));
    builtinFmt_.setForeground(QColor(QStringLiteral("#116329")));
    constantFmt_.setForeground(QColor(QStringLiteral("#0550ae")));
    commentFmt_.setForeground(QColor(QStringLiteral("#6e7781")));
    operatorFmt_.setForeground(QColor(QStringLiteral("#cf222e")));
    punctFmt_.setForeground(QColor(QStringLiteral("#24292f")));
  }
}

void CalxHighlighter::highlightBlock(const QString &text) {
  // Лексер працює з UTF-8 байтами; setFormat — з індексами QString.
  const std::string src = text.toUtf8().toStdString();
  auto lexed = calc::tokenize(src);

  for (const auto &tok : lexed.tokens) {
    if (tok.kind == calc::TokenKind::End || tok.text.empty()) {
      continue;
    }
    const int from = qIndexFromUtf8(src, tok.pos.offset);
    const int len = qLenFromUtf8(src, tok.pos.offset, tok.text.size());
    if (from < 0 || len <= 0 || from + len > text.size()) {
      continue;
    }

    QTextCharFormat fmt;
    switch (tok.kind) {
    case calc::TokenKind::Let:
    case calc::TokenKind::Print:
    case calc::TokenKind::If:
    case calc::TokenKind::Else:
    case calc::TokenKind::While:
    case calc::TokenKind::For:
    case calc::TokenKind::In:
    case calc::TokenKind::Break:
    case calc::TokenKind::Continue:
    case calc::TokenKind::Fun:
    case calc::TokenKind::Return:
    case calc::TokenKind::And:
    case calc::TokenKind::Or:
    case calc::TokenKind::Not:
      fmt = keywordFmt_;
      break;
    case calc::TokenKind::True:
    case calc::TokenKind::False:
    case calc::TokenKind::IntegerLit:
    case calc::TokenKind::RationalLit:
    case calc::TokenKind::RealLit:
    case calc::TokenKind::ImaginaryLit:
      fmt = literalFmt_;
      break;
    case calc::TokenKind::String:
      fmt = stringFmt_;
      break;
    case calc::TokenKind::Identifier:
      if (calc::find_builtin(tok.text)) {
        fmt = builtinFmt_;
      } else if (calc::is_constant_name(tok.text)) {
        fmt = constantFmt_;
      } else {
        continue;
      }
      break;
    case calc::TokenKind::Plus:
    case calc::TokenKind::Minus:
    case calc::TokenKind::Star:
    case calc::TokenKind::StarStar:
    case calc::TokenKind::Slash:
    case calc::TokenKind::Percent:
    case calc::TokenKind::Equal:
    case calc::TokenKind::EqEq:
    case calc::TokenKind::NotEq:
    case calc::TokenKind::Less:
    case calc::TokenKind::LessEq:
    case calc::TokenKind::Greater:
    case calc::TokenKind::GreaterEq:
      fmt = operatorFmt_;
      break;
    case calc::TokenKind::LParen:
    case calc::TokenKind::RParen:
    case calc::TokenKind::LBrace:
    case calc::TokenKind::RBrace:
    case calc::TokenKind::LBracket:
    case calc::TokenKind::RBracket:
    case calc::TokenKind::Comma:
    case calc::TokenKind::Semicolon:
    case calc::TokenKind::Dot:
    case calc::TokenKind::Colon:
      fmt = punctFmt_;
      break;
    default:
      continue;
    }
    setFormat(from, len, fmt);
  }

  bool in_string = false;
  bool escape = false;
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text[i];
    if (in_string) {
      if (escape) {
        escape = false;
      } else if (c == QLatin1Char('\\')) {
        escape = true;
      } else if (c == QLatin1Char('"')) {
        in_string = false;
      }
      continue;
    }
    if (c == QLatin1Char('"')) {
      in_string = true;
      continue;
    }
    if (c == QLatin1Char('/') && i + 1 < text.size() &&
        text[i + 1] == QLatin1Char('/')) {
      setFormat(i, static_cast<int>(text.size() - i), commentFmt_);
      break;
    }
  }
}
