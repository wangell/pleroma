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

    throw ParserException(filename, line_number, char_number, "Reached end of tokenstream while looking for " + tokens_allowed);
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
    throw ParserException(filename, line_number, char_number, "Expected token type: " + std::string(token_type_to_string(t)) + " but got " + std::string(token_type_to_string(curr->type)) + "(" + std::to_string((int)curr->type) + ")");
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

void TokenStream::add_token(TokenType t, std::string lexeme, int start_char, int end_char, int line_n) {
  Token *tok = new Token({t, lexeme});
  tokens.push_back(tok);

  tok->start_char = start_char;
  tok->end_char = end_char;
  tok->line_n = line_n;
}

TokenStream* tokenize_file(std::string filepath) {
  TokenStream *tokenstream = new TokenStream;
  tokenstream->filename = filepath;

  FILE *f = fopen(filepath.c_str(), "r");
  if (!f) {
    printf("Error opening module: %s\n", filepath.c_str());
    exit(1);
  }

  int line_n = 0;
  int char_n = 0;

  int start_char = 0;
  int end_char = 0;

  wchar_t c;

  while ((c = fgetwc(f)) != EOF) {
    start_char = char_n;
    char_n += 1;
    if (c == '~') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Import, "~", start_char, end_char, line_n);
    } else if (c == '?') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Match, "?", start_char, end_char, line_n);
    } else if (c == '|') {
      end_char = char_n;
      tokenstream->add_token(TokenType::For, "|", start_char, end_char, line_n);
    } else if (c == '=') {
      if ((c = fgetwc(f)) == '=') {
        char_n += 1;
        end_char = char_n;
        tokenstream->add_token(TokenType::EqualsEquals, "==", start_char, end_char, line_n);
      } else {
        ungetwc(c, f);
        end_char = char_n;
        tokenstream->add_token(TokenType::Equals, "=", start_char, end_char, line_n);
      }
    } else if (c == '^') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Not, "^", start_char, end_char, line_n);
    } else if (c == '>') {
      if ((c = fgetwc(f)) == '=') {
        char_n += 1;
        end_char = char_n;
        tokenstream->add_token(TokenType::GreaterThanEqual, ">=", start_char, end_char, line_n);
      } else {
        ungetwc(c, f);
        end_char = char_n;
        tokenstream->add_token(TokenType::GreaterThan, ">", start_char, end_char, line_n);
      }
    } else if (c == '<') {
      if ((c = fgetwc(f)) == '=') {
        char_n += 1;
        end_char = char_n;
        tokenstream->add_token(TokenType::LessThanEqual, "<=", start_char, end_char, line_n);
      } else {
        ungetwc(c, f);
        end_char = char_n;
        tokenstream->add_token(TokenType::LessThan, "<", start_char, end_char, line_n);
      }
    } else if (isalpha(c)) {
      std::string sym;
      sym.push_back(c);
      while ((c = fgetwc(f))) {
        char_n += 1;
        if (!isalpha(c) && !isdigit(c) && c != '-') {
            ungetwc(c, f);
            char_n -= 1;
            break;
          } else {
            sym.push_back(c);
          }
      }

      if (sym == "whl") {
        end_char = char_n;
        tokenstream->add_token(TokenType::While, sym, start_char, end_char, line_n);
      } else if (sym == "loc") {
        end_char = char_n;
        tokenstream->add_token(TokenType::LocVar, sym, start_char, end_char, line_n);
      } else if (sym == "far") {
        end_char = char_n;
        tokenstream->add_token(TokenType::FarVar, sym, start_char, end_char, line_n);
      } else if (sym == "aln") {
        end_char = char_n;
        tokenstream->add_token(TokenType::AlnVar, sym, start_char, end_char, line_n);
      } else if (sym == "self") {
        end_char = char_n;
        tokenstream->add_token(TokenType::Self, sym, start_char, end_char, line_n);
      } else {
        end_char = char_n;
        tokenstream->add_token(TokenType::Symbol, sym, start_char, end_char, line_n);
      }
    } else if (c == U'δ') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Function, "δ", start_char, end_char, line_n);
    } else if (c == U'λ') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Pure, "λ", start_char, end_char, line_n);
    } else if (c == U'↵') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Return, "↵", start_char, end_char, line_n);
    } else if (c == '+') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Plus, "+", start_char, end_char, line_n);
    } else if (c == '-') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Minus, "-", start_char, end_char, line_n);
    } else if (c == '"') {
      std::string sym;
      while ((c = fgetwc(f)) != '"') {
        char_n += 1;

        // Escaped characters
        if (c == '\\') {
          c = fgetwc(f);
          char_n += 1;

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
      end_char = char_n;
      tokenstream->add_token(TokenType::String, sym, start_char, end_char, line_n);
    } else if (c == '*') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Star, "*", start_char, end_char, line_n);
    } else if (c == '*') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Star, "*", start_char, end_char, line_n);
    } else if (c == U'ε') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Actor, "ε", start_char, end_char, line_n);
    } else if (c == U'$') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Dollar, "$", start_char, end_char, line_n);
    } else if (c == '/') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Slash, "/", start_char, end_char, line_n);
    } else if (c == U'⌜') {
      end_char = char_n;
      tokenstream->add_token(TokenType::IndexStart, "⌜", start_char, end_char, line_n);
    } else if (c == U'⌟') {
      end_char = char_n;
      tokenstream->add_token(TokenType::IndexEnd, "⌟", start_char, end_char, line_n);
    } else if (c == '(') {
      end_char = char_n;
      tokenstream->add_token(TokenType::LeftParen, "(", start_char, end_char, line_n);
    } else if (c == ')') {
      end_char = char_n;
      tokenstream->add_token(TokenType::RightParen, ")", start_char, end_char, line_n);
    } else if (c == '[') {
      end_char = char_n;
      tokenstream->add_token(TokenType::LeftBracket, "[", start_char, end_char, line_n);
    } else if (c == ']') {
      end_char = char_n;
      tokenstream->add_token(TokenType::RightBracket, "]", start_char, end_char, line_n);
    } else if (c == '{') {
      end_char = char_n;
      tokenstream->add_token(TokenType::LeftBrace, "{", start_char, end_char, line_n);
    } else if (c == '}') {
      end_char = char_n;
      tokenstream->add_token(TokenType::RightBrace, "}", start_char, end_char, line_n);
    } else if (c == '@') {
      end_char = char_n;
      tokenstream->add_token(TokenType::PromiseType, "@", start_char, end_char, line_n);
    } else if (c == ':') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Colon, "(", start_char, end_char, line_n);
    } else if (c == '!') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Message, "!", start_char, end_char, line_n);
    } else if (c == '\n') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Newline, "\n", start_char, end_char, line_n);
      line_n++;
      char_n = 0;
    } else if (c == ',') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Comma, ",", start_char, end_char, line_n);
    } else if (c == '&') {
      // FIXME combine with dot and make it a separate operator in future
      end_char = char_n;
      tokenstream->add_token(TokenType::Breakthrough, "&", start_char, end_char, line_n);
    } else if (c == U'►') {
      end_char = char_n;
      tokenstream->add_token(TokenType::ModUse, "►", start_char, end_char, line_n);
    } else if (c == '.') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Dot, ".", start_char, end_char, line_n);
    } else if (c == '_') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Fallthrough, "_", start_char, end_char, line_n);
    } else if (c == '#') {
      c = fgetwc(f);
      char_n += 1;
      if (c == 't') {
        end_char = char_n;
        tokenstream->add_token(TokenType::True, "#t", start_char, end_char, line_n);
      } else if (c == 'f') {
        end_char = char_n;
        tokenstream->add_token(TokenType::False, "#f", start_char, end_char, line_n);
      } else {
        printf("Invalid boolean literal.\n");
        assert(false);
      }
    } else if (c == '\t') {
      end_char = char_n;
      tokenstream->add_token(TokenType::Tab, "\t", start_char, end_char, line_n);
    } else if (isdigit(c)) {
      std::string n;
      n.push_back(c);
      while (isdigit(c = fgetwc(f))) {
        char_n += 1;
        n.push_back(c);
      }
      ungetwc(c, f);
      end_char = char_n;
      tokenstream->add_token(TokenType::Number, n, start_char, end_char, line_n);
    } else if (c == ' ') {
      // ignore
    } else {
      throw TokenizerException(tokenstream->filename, line_n, char_n, "Invalid character: " + std::to_string(c));
    }
  }

  end_char = char_n;
  tokenstream->add_token(TokenType::EndOfFile, "EOF", start_char, end_char, line_n);

  tokenstream->current = tokenstream->tokens.begin();

  return tokenstream;
}

const char *token_type_to_string(TokenType t) {
  switch (t) {
  case TokenType::Newline: return "Newline";
  case TokenType::Tab: return "Tab";
  case TokenType::Symbol: return "Symbol";
  case TokenType::LeftParen: return "Left Paren";
  case TokenType::RightParen: return "Right Paren";
  case TokenType::PromiseType: return "Promise";
  case TokenType::Actor: return "Entity";
  case TokenType::LocVar: return "LocVar";
  case TokenType::FarVar: return "FarVar";
  case TokenType::AlnVar: return "AlnVar";
  case TokenType::Message: return "Message";
  case TokenType::Return: return "Return";
  case TokenType::While: return "While";
  case TokenType::True: return "True";
  case TokenType::False: return "False";
  case TokenType::Function: return "Function";
  case TokenType::For: return "For";
  case TokenType::Fallthrough: return "Fallthrough";
  case TokenType::Colon: return "Colon";
  case TokenType::Dot: return "Dot";
  case TokenType::Plus: return "Plus";
  case TokenType::ModUse: return "ModUse";
  case TokenType::EndOfFile: return "EOF";
  case TokenType::IndexStart: return "IndexStart";
  case TokenType::IndexEnd: return "IndexEnd";
  case TokenType::Breakthrough: return "Breakthrough";
  case TokenType::Import: return "Import";
  }

  return "Unimplemented";
}
