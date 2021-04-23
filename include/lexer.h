#ifndef LEXER_H
#define LEXER_H

#include "symbols.h"

struct LexerError : std::runtime_error {
public:
  LexerError(const std::string &msg);
};

class Lexer {
  /**
   * Lexer caches the last processed token in order to correctly distinguish
   * between 'i' as a variable name and as the imaginary unit.
   * Only occurrences immediately following either an integer or a double
   * are classified as the imaginary unit.
   * Note: currently the complex number syntax requires both the real and
   * the imaginary part to be written explicitly. This means that a lone 'i'
   * will always be classified as a variable name.
   * The number 'i' needs to be written as '0 + 1i'.
   **/
  Tag previous;

  // Next character
  char peek;

  // Input stream
  std::istream &stream;

  // Keywords
  std::unordered_map<std::string, Token> reserved;

  void reserve(const std::vector<std::pair<std::string, Tag>> &keywords);
  void reserve(const std::vector<std::pair<std::string, TypeID>> &types);

  // Puts next character into peek
  void readNext();

  // Puts next character into peek and return whether is matches the argument
  bool readNext(char next);

  void error(char token);

  /**
   *  Helper functions that put a token into reserved and update previous.
   *  Should be used like this: return ret(Token(...));
   * */
  Token ret(Token token);
  Token ret(Tag tag);

  // Skips whitespace
  void whitespace();

  // Handle tokens starting with "="
  Token equals();

  // Handle tokens starting with "!"
  Token notEquals();

  // Handle tokens starting with "<"
  Token lessThan();

  // Handle tokens starting with ">"
  Token greaterThan();

  // Handle tokens starting with '"'
  Token quotation();

  // Handle tokens starting with a digit
  Token digit();

  // Handle tokens starting with a letter or underscore
  Token alpha();

public:
  int line;
  Lexer(std::istream &stream_ = std::cin);

  Token getNextToken();
};

#endif // LEXER_H