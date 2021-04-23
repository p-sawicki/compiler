#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "parse_tree.h"
#include <fstream>

struct ParserError : std::runtime_error {
public:
  ParserError(const std::string &err);
};

class Parser {
  Lexer &lexer;
  Token peek;
  int lineNumber;

  void next();
  void error(const std::string &msg) const;
  void warning(const std::string &msg) const;

  void match(Tag tag, const std::string &errMsg);

  stmt_ptr variableDefiniton();
  stmt_ptr functionDefinition();
  stmt_ptr statement();
  stmt_ptr conditionalStatement();
  stmt_ptr block();
  stmt_ptr assignment();
  expr_ptr expression();
  expr_ptr term();
  expr_ptr factor();
  expr_ptr unary();
  expr_ptr functionCall();
  expr_ptr conditional();
  expr_ptr conjunction();
  expr_ptr negation();
  expr_ptr relation();

public:
  const static std::string NO_SEMICOLON, NO_COLON, NO_CLOSING_BRACKET,
      NO_CURLY_BRACKET, NO_CLOSING_CURLY_BRACKET;

  Parser(Lexer &lexer_);
  stmt_ptr parseNext();
  void parse();
};

#endif