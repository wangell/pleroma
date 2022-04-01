#include "hylic.h"
#include "hylic_tokenizer.h"
#include <cassert>

Token* TokenStream::peek() { return *(current); }

Token* TokenStream::peek_forward() {
  current++;
  auto c = current;
  current--;
  return *c;
}

void TokenStream::go_back(int n) {
  for (int q = 0; q < n; ++q) {
    current--;
  }
}

Token* TokenStream::get() {
  auto c = current;
  current++;
  return *c;
}

void TokenStream::reset() {
  current = tokens.begin();
  line_number = 0;
  char_number = 0;
}

void add_token(TokenType t, std::string lexeme) {
  Token *tok = new Token({t, lexeme});
  tokenstream.tokens.push_back(tok);
}

void tokenize_file(FILE *f) {
  wchar_t c;

  while ((c = fgetwc(f)) != EOF) {
    if (c == '~') {
      add_token(TokenType::Import, "~");
    } else if (c == '?') {
      add_token(TokenType::Match, "?");
    } else if (c == '|') {
      add_token(TokenType::For, "|");
    } else if (c == '=') {
      add_token(TokenType::Equals, "=");
    } else if (c == '^') {
      add_token(TokenType::Not, "^");
    } else if (c == '>') {
      if ((c = fgetwc(f)) == '=') {
        add_token(TokenType::GreaterThanEqual, ">=");
      } else {
        ungetwc(c, f);
        add_token(TokenType::GreaterThan, ">");
      }
    } else if (c == '<') {
      if ((c = fgetwc(f)) == '=') {
        add_token(TokenType::LessThanEqual, "<=");
      } else {
        ungetwc(c, f);
        add_token(TokenType::LessThan, "<");
      }
    } else if (isalpha(c)) {
      std::string sym;
      sym.push_back(c);
      while ((c = fgetwc(f))) {
        if (!isalpha(c) && !isdigit(c)) {
            ungetwc(c, f);
            break;
          } else {
            sym.push_back(c);
          }
      }

      if (sym == "whl") {
        add_token(TokenType::While, sym);
      } else if (sym == "loc") {
        add_token(TokenType::LocVar, sym);
      } else if (sym == "far") {
        add_token(TokenType::FarVar, sym);
      } else if (sym == "aln") {
        add_token(TokenType::AlnVar, sym);
      } else {
        add_token(TokenType::Symbol, sym);
      }
    } else if (c == U'δ') {
      add_token(TokenType::Function, "δ");
    } else if (c == U'λ') {
      add_token(TokenType::Pure, "λ");
    } else if (c == U'↵') {
      add_token(TokenType::Return, "↵");
    } else if (c == '+') {
      add_token(TokenType::Plus, "+");
    } else if (c == '-') {
      add_token(TokenType::Minus, "-");
    } else if (c == '"') {
      std::string sym;
      while ((c = fgetwc(f)) != '"') {
        sym.push_back(c);
      }
      add_token(TokenType::String, sym);
    } else if (c == '*') {
      add_token(TokenType::Star, "*");
    } else if (c == U'ε') {
      add_token(TokenType::Actor, "ε");
    } else if (c == '/') {
      add_token(TokenType::Slash, "/");
    } else if (c == '(') {
      add_token(TokenType::LeftParen, "(");
    } else if (c == ')') {
      add_token(TokenType::RightParen, ")");
    } else if (c == '[') {
      add_token(TokenType::LeftBracket, "[");
    } else if (c == ']') {
      add_token(TokenType::RightBracket, "]");
    } else if (c == '{') {
      add_token(TokenType::LeftBrace, "{");
    } else if (c == '}') {
      add_token(TokenType::RightBrace, "}");
    } else if (c == '@') {
      add_token(TokenType::PromiseType, "@");
    } else if (c == ':') {
      add_token(TokenType::Colon, "(");
    } else if (c == '!') {
      add_token(TokenType::Message, "!");
    } else if (c == '\n') {
      add_token(TokenType::Newline, "\n");
    } else if (c == ',') {
      add_token(TokenType::Comma, ",");
    } else if (c == '&') {
      // FIXME combine with dot and make it a separate operator in future
      add_token(TokenType::Breakthrough, "&");
    } else if (c == '.') {
      add_token(TokenType::Dot, ".");
    } else if (c == '_') {
      add_token(TokenType::Fallthrough, "_");
    } else if (c == '#') {
      c = fgetwc(f);
      if (c == 't') {
        add_token(TokenType::True, "#t");
      } else if (c == 'f') {
        add_token(TokenType::False, "#f");
      } else {
        printf("Invalid boolean literal.\n");
        assert(false);
      }
    } else if (c == '\t') {
      add_token(TokenType::Tab, "\t");
    } else if (isdigit(c)) {
      std::string n;
      n.push_back(c);
      while (isdigit(c = fgetwc(f))) {
        n.push_back(c);
      }
      ungetwc(c, f);
      add_token(TokenType::Number, n);
    } else if (c == ' ') {
      // ignore
    } else {
      printf("Invalid character: %c\n", c);
      assert(false);
    }
  }

  tokenstream.current = tokenstream.tokens.begin();
}

const char *token_type_to_string(TokenType t) {
  switch (t) {
  case TokenType::Newline:
    return "Newline";
    break;
  case TokenType::Tab:
    return "Tab";
    break;
  case TokenType::Symbol:
    return "Symbol";
    break;
  case TokenType::LeftParen:
    return "Left Paren";
    break;
  case TokenType::RightParen:
    return "Right Paren";
    break;
  case TokenType::PromiseType:
    return "Promise";
    break;
  case TokenType::Actor:
    return "Entity";
    break;
  case TokenType::LocVar:
    return "LocVar";
    break;
  case TokenType::FarVar:
    return "FarVar";
    break;
  case TokenType::AlnVar:
    return "AlnVar";
    break;
  case TokenType::Message:
    return "Message";
    break;
  case TokenType::Return:
    return "Return";
    break;
  case TokenType::While:
    return "While";
    break;
  case TokenType::True:
    return "True";
    break;
  case TokenType::False:
    return "False";
    break;
  case TokenType::Function:
    return "Function";
    break;
  case TokenType::For:
    return "For";
    break;
  case TokenType::Fallthrough:
    return "Fallthrough";
    break;
  case TokenType::Colon:
    return "Colon";
    break;
  case TokenType::Dot:
    return "Dot";
    break;
  }

  return "Unimplemented";
}
