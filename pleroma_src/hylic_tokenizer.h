#pragma once

#include "common.h"
#include "hylic_compex.h"
#include <list>
#include <string>
#include <vector>

enum class TokenType {
  Newline,
  Tab,
  EndOfFile,

  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,

  Actor,

  Plus,
  Minus,
  Star,
  Slash,

  Equals,
  EqualsEquals,
  GreaterThan,
  GreaterThanEqual,
  LessThan,
  LessThanEqual,
  NotEqual,
  Not,

  Pound,
  LeftBracket,
  RightBracket,
  Dollar,
  Bang,
  Dot,
  Comma,
  Colon,
  Breakthrough,

  Import,
  Match,
  True,
  False,
  Function,
  Pure,
  For,
  While,
  Return,
  Fallthrough,

  PromiseType,

  LocVar,
  FarVar,
  AlnVar,

  Message,

  // Literals
  Number,
  String,
  Character,
  Symbol,

  ModUse,
  IndexStart,
  IndexEnd,
  Self,
  Comment
};

struct Token {
  TokenType type;
  std::string lexeme;

  int start_char = 0;
  int end_char = 0;

  int line_n = 0;
};

struct TokenStream {
  std::list<Token *> tokens;
  std::list<Token *>::iterator current;

  std::string filename;
  int line_number = 0;
  int char_number = 0;

  Token *peek();
  Token *peek_forward();
  void go_back(int n);
  Token *get();
  void reset();

  void expect(TokenType t);
  void expect_or(std::vector<TokenType>);
  void expect_eos();

  Token *check(TokenType t);
  Token *accept(TokenType t);
  void add_token(TokenType t, std::string lexeme, int start_char, int end_char, int line_n);
  std::vector<Token *> accept_all(std::vector<TokenType> toks);
};

class TokenizerException : public CompileException {
public:
  TokenizerException(std::string file, u32 line_n, u32 char_n,
                   std::string msg)
    : CompileException(file, line_n, char_n, msg) {}
};
TokenStream* tokenize_file(std::string filepath);

const char *token_type_to_string(TokenType t);
