#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class Tag {
  TYPE,
  INT,
  DOUBLE,
  I,
  STRING,
  ID,
  ASSIGN,
  EQ,
  NEQ,
  LT,
  LE,
  GE,
  GT,
  FUN,
  MAIN,
  OR,
  AND,
  NOT,
  IF,
  ELSE,
  WHILE,
  RETURN,
  RE,
  IM,
  SEMICOLON,
  COLON,
  COMMA,
  OPEN_CURLY,
  CLOSE_CURLY,
  OPEN_BRACKET,
  CLOSE_BRACKET,
  VERTICAL,
  PLUS,
  MINUS,
  TIMES,
  DIVIDE,
  END
};

// Single-character tokens
const static std::unordered_map<char, Tag> operators{{'+', Tag::PLUS},
                                                     {'-', Tag::MINUS},
                                                     {'*', Tag::TIMES},
                                                     {'/', Tag::DIVIDE},
                                                     {'=', Tag::ASSIGN},
                                                     {'<', Tag::LT},
                                                     {'>', Tag::GT},
                                                     {':', Tag::COLON},
                                                     {';', Tag::SEMICOLON},
                                                     {'{', Tag::OPEN_CURLY},
                                                     {'}', Tag::CLOSE_CURLY},
                                                     {'(', Tag::OPEN_BRACKET},
                                                     {')', Tag::CLOSE_BRACKET},
                                                     {'|', Tag::VERTICAL},
                                                     {',', Tag::COMMA},
                                                     {EOF, Tag::END}};

enum class TypeID { INT, DOUBLE, COMPLEX, STRING, NONE };

struct Token {
  Tag tag;
  std::variant<int64_t, double, std::string, TypeID> value;
  int line;

  Token(Tag tag_, int line_);
  Token(int64_t val, int line_);
  Token(double val, int line_);
  Token(TypeID id, int line_);
  Token(Tag tag_, const std::string &val, int line_);

  int64_t getInt();
  double getDouble();
  std::string getString();
  TypeID getType();
};

#endif // SYMBOLS_H