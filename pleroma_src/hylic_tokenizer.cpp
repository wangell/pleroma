#include "hylic.h"
#include "hylic_tokenizer.h"
#include <cassert>
#include <exception>
#include <string>
#include <vector>

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
    if ((*current)->type == TokenType::Newline) {
      line_number--;
    }
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

void TokenStream::expect_or(std::vector<TokenType> toks) {
  if (current == tokens.end()) {
    std::string tokens_allowed = "";
    for (auto j : toks) {
      tokens_allowed += "|" + std::string(token_type_to_string(j));
    }

    throw TokenizerException(filename, line_number, char_number, "Reached end of tokenstream while looking for " + tokens_allowed);
    //throw TokenizerException("Reached end of tokenstream while looking for %s\n", tokens_allowed.c_str());
    assert(false);
    exit(1);
  }

  auto curr = get();

  bool matched_tok = false;

  for (auto k : toks) {
    if (k != curr->type) {
      matched_tok = true;
    }
  }

  if (!matched_tok) {
    std::string tokens_allowed = "";
    for (auto j : toks) {
      tokens_allowed += "|" + std::string(token_type_to_string(j));
    }
    printf("Expected token type: %s but got %s(%d), at line %d\n", tokens_allowed.c_str(), token_type_to_string(curr->type), curr->type, line_number);
    exit(1);
  }

  if (curr->type == TokenType::Newline)
    line_number++;
}

void TokenStream::expect_eos() {
  expect_or({ TokenType::Newline, TokenType::EndOfFile });
}

void TokenStream::expect(TokenType t) {
  if (current == tokens.end()) {
    printf("Reached end of tokenstream while looking for %s\n", token_type_to_string(t));
    assert(false);
    exit(1);
  }

  auto curr = get();
  if (curr->type != t) {
    printf("Expected token type: %s but got %s(%d), at line %d\n", token_type_to_string(t), token_type_to_string(curr->type), curr->type, line_number);
    throw TokenizerException(filename, line_number, char_number, "Expected token type: " + std::string(token_type_to_string(t)) + " but got " + std::string(token_type_to_string(curr->type)) + "(" + std::to_string((int)curr->type) + ")");
  }

  if (curr->type == TokenType::Newline)
    line_number++;
}

Token *TokenStream::check(TokenType t) {
  if (current == tokens.end()) {
    return nullptr;
  }
  auto g = get();
  current--;
  if (g->type == t) {
    return g;
  } else {
    return nullptr;
  }
}

std::vector<Token *> TokenStream::accept_all(std::vector<TokenType> toks) {
  assert(toks.size() > 0);

  // FIXME need to check for toks.length until end
  if (current == tokens.end()) {
    return {};
  }

  std::vector<Token *> return_toks;

  for (int k = 0; k < toks.size(); ++k) {
    auto g = get();
    if (toks[k] == g->type) {
      return_toks.push_back(g);
    } else {
      break;
    }
  }

  if (toks.size() != return_toks.size()) {
    for (int k = 0; k < return_toks.size(); ++k) {
      current--;
    }
    return {};
  }

  return return_toks;
}

Token *TokenStream::accept(TokenType t) {
  if (current == tokens.end()) {
    assert(false);
    return nullptr;
  }
  auto g = get();
  if (g->type == t) {

    if (g->type == TokenType::Newline)
      line_number++;

    return g;
  } else {
    current--;
    return nullptr;
  }
}

void TokenStream::add_token(TokenType t, std::string lexeme) {
  Token *tok = new Token({t, lexeme});
  tokens.push_back(tok);
}

TokenStream* tokenize_file(std::string filepath) {
  TokenStream *tokenstream = new TokenStream;
  tokenstream->filename = filepath;

  FILE *f = fopen(filepath.c_str(), "r");
  if (!f) {
    printf("Error opening module: %s\n", filepath.c_str());
    exit(1);
  }

  int line_n = 1;
  int char_n = 1;

  wchar_t c;

  while ((c = fgetwc(f)) != EOF) {
    char_n += 1;
    if (c == '~') {
      tokenstream->add_token(TokenType::Import, "~");
    } else if (c == '?') {
      tokenstream->add_token(TokenType::Match, "?");
    } else if (c == '|') {
      tokenstream->add_token(TokenType::For, "|");
    } else if (c == '=') {
      if ((c = fgetwc(f)) == '=') {
        tokenstream->add_token(TokenType::EqualsEquals, "==");
      } else {
        ungetwc(c, f);
        tokenstream->add_token(TokenType::Equals, "=");
      }
    } else if (c == '^') {
      tokenstream->add_token(TokenType::Not, "^");
    } else if (c == '>') {
      if ((c = fgetwc(f)) == '=') {
        tokenstream->add_token(TokenType::GreaterThanEqual, ">=");
      } else {
        ungetwc(c, f);
        tokenstream->add_token(TokenType::GreaterThan, ">");
      }
    } else if (c == '<') {
      if ((c = fgetwc(f)) == '=') {
        tokenstream->add_token(TokenType::LessThanEqual, "<=");
      } else {
        ungetwc(c, f);
        tokenstream->add_token(TokenType::LessThan, "<");
      }
    } else if (isalpha(c)) {
      std::string sym;
      sym.push_back(c);
      while ((c = fgetwc(f))) {
        if (!isalpha(c) && !isdigit(c) && c != '-') {
            ungetwc(c, f);
            break;
          } else {
            sym.push_back(c);
          }
      }

      if (sym == "whl") {
        tokenstream->add_token(TokenType::While, sym);
      } else if (sym == "loc") {
        tokenstream->add_token(TokenType::LocVar, sym);
      } else if (sym == "far") {
        tokenstream->add_token(TokenType::FarVar, sym);
      } else if (sym == "aln") {
        tokenstream->add_token(TokenType::AlnVar, sym);
      } else {
        tokenstream->add_token(TokenType::Symbol, sym);
      }
    } else if (c == U'δ') {
      tokenstream->add_token(TokenType::Function, "δ");
    } else if (c == U'λ') {
      tokenstream->add_token(TokenType::Pure, "λ");
    } else if (c == U'↵') {
      tokenstream->add_token(TokenType::Return, "↵");
    } else if (c == '+') {
      tokenstream->add_token(TokenType::Plus, "+");
    } else if (c == '-') {
      tokenstream->add_token(TokenType::Minus, "-");
    } else if (c == '"') {
      std::string sym;
      while ((c = fgetwc(f)) != '"') {
        if (c == '\\') {
          c = fgetwc(f);

          if (c == 'n') {
            sym.push_back('\n');
          } else if (c == 't') {
            sym.push_back('\t');
          } else if (c == '"') {
            sym.push_back('"');
          } else if (c == '\\') {
            sym.push_back('\\');
          } else {
            assert(false);
          }

        } else {
          sym.push_back(c);
        }
      }
      tokenstream->add_token(TokenType::String, sym);
    } else if (c == '*') {
      tokenstream->add_token(TokenType::Star, "*");
    } else if (c == '*') {
      tokenstream->add_token(TokenType::Star, "*");
    } else if (c == U'ε') {
      tokenstream->add_token(TokenType::Actor, "ε");
    } else if (c == U'$') {
      tokenstream->add_token(TokenType::Dollar, "$");
    } else if (c == '/') {
      tokenstream->add_token(TokenType::Slash, "/");
    } else if (c == U'⌜') {
      tokenstream->add_token(TokenType::IndexStart, "⌜");
    } else if (c == U'⌟') {
      tokenstream->add_token(TokenType::IndexEnd, "⌟");
    } else if (c == '(') {
      tokenstream->add_token(TokenType::LeftParen, "(");
    } else if (c == ')') {
      tokenstream->add_token(TokenType::RightParen, ")");
    } else if (c == '[') {
      tokenstream->add_token(TokenType::LeftBracket, "[");
    } else if (c == ']') {
      tokenstream->add_token(TokenType::RightBracket, "]");
    } else if (c == '{') {
      tokenstream->add_token(TokenType::LeftBrace, "{");
    } else if (c == '}') {
      tokenstream->add_token(TokenType::RightBrace, "}");
    } else if (c == '@') {
      tokenstream->add_token(TokenType::PromiseType, "@");
    } else if (c == ':') {
      tokenstream->add_token(TokenType::Colon, "(");
    } else if (c == '!') {
      tokenstream->add_token(TokenType::Message, "!");
    } else if (c == '\n') {
      tokenstream->add_token(TokenType::Newline, "\n");
      line_n++;
      char_n = 0;
    } else if (c == ',') {
      tokenstream->add_token(TokenType::Comma, ",");
    } else if (c == '&') {
      // FIXME combine with dot and make it a separate operator in future
      tokenstream->add_token(TokenType::Breakthrough, "&");
    } else if (c == U'►') {
      tokenstream->add_token(TokenType::ModUse, "►");
    } else if (c == '.') {
      tokenstream->add_token(TokenType::Dot, ".");
    } else if (c == '_') {
      tokenstream->add_token(TokenType::Fallthrough, "_");
    } else if (c == '#') {
      c = fgetwc(f);
      if (c == 't') {
        tokenstream->add_token(TokenType::True, "#t");
      } else if (c == 'f') {
        tokenstream->add_token(TokenType::False, "#f");
      } else {
        printf("Invalid boolean literal.\n");
        assert(false);
      }
    } else if (c == '\t') {
      tokenstream->add_token(TokenType::Tab, "\t");
    } else if (isdigit(c)) {
      std::string n;
      n.push_back(c);
      while (isdigit(c = fgetwc(f))) {
        n.push_back(c);
      }
      ungetwc(c, f);
      tokenstream->add_token(TokenType::Number, n);
    } else if (c == ' ') {
      // ignore
    } else {
      throw TokenizerException(tokenstream->filename, line_n, char_n, "Invalid character: " + std::to_string(c));
    }
  }

  tokenstream->add_token(TokenType::EndOfFile, "EOF");

  tokenstream->current = tokenstream->tokens.begin();

  return tokenstream;
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
  case TokenType::Plus:
    return "Plus";
    break;
  case TokenType::ModUse:
    return "ModUse";
    break;
  case TokenType::EndOfFile:
    return "EOF";
    break;
  case TokenType::IndexStart:
    return "IndexStart";
    break;
  case TokenType::IndexEnd:
    return "IndexEnd";
    break;
  case TokenType::Breakthrough:
    return "Breakthrough";
    break;
  case TokenType::Import:
    return "Import";
    break;
  }

  return "Unimplemented";
}
