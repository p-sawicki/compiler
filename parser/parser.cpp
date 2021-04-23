#include "parser.h"

ParserError::ParserError(const std::string &err) : std::runtime_error(err) {}

const std::string Parser::NO_SEMICOLON = "Missing semicolon ';'";
const std::string Parser::NO_COLON = "Missing colon ':'";
const std::string Parser::NO_CLOSING_BRACKET =
    "No match for opening bracket '('";
const std::string Parser::NO_CURLY_BRACKET = "Missing curly bracket '{'";
const std::string Parser::NO_CLOSING_CURLY_BRACKET =
    "No match for opening curly bracket '{'";

void Parser::next() {
  lineNumber = lexer.line;
  peek = lexer.getNextToken();
}

void Parser::error(const std::string &msg) const {
  throw ParserError("[ERROR] " + msg + " at line " +
                    std::to_string(lineNumber) + ".");
}

void Parser::warning(const std::string &msg) const {
  std::cout << "[WARNING] " << msg << " at line " << lineNumber << "\n";
}

void Parser::match(Tag tag, const std::string &errMsg) {
  if (peek.tag == tag) {
    next();
  } else {
    error(errMsg);
  }
}

stmt_ptr Parser::variableDefiniton() {
  Token type = std::move(peek);
  next();

  if (peek.tag != Tag::ID) {
    error("Expected an identifier");
  }
  Token name = std::move(peek);
  next();

  match(Tag::ASSIGN, "Variable " + name.getString() + " was not initialized");
  expr_ptr expr = expression();

  id_ptr id = std::make_unique<Identifier>(std::move(name), type.getType());
  match(Tag::SEMICOLON, NO_SEMICOLON);

  return std::make_unique<VariableDefinition>(std::move(id), std::move(expr));
}

stmt_ptr Parser::functionDefinition() {
  Token name = std::move(peek);
  next();

  match(Tag::COLON, NO_COLON);
  Token type = std::move(peek);
  next();

  match(Tag::OPEN_BRACKET,
        "Expected parameter list for function " + name.getString());
  std::vector<id_ptr> params;
  while (peek.tag != Tag::CLOSE_BRACKET) {
    Token paramName = std::move(peek);
    next();
    match(Tag::COLON, NO_COLON);

    params.push_back(
        std::make_unique<Identifier>(std::move(paramName), peek.getType()));
    next();
    if (peek.tag == Tag::COMMA) {
      next();
      if (peek.tag == Tag::CLOSE_BRACKET) {
        warning("Comma with no parameter after");
      }
    }
  }
  next(); // ')'

  if (peek.tag == Tag::SEMICOLON) {
    next();
    return std::make_unique<FunctionDeclaration>(std::move(name),
                                                 type.getType(), params);
  }
  return std::make_unique<FunctionDefinition>(std::move(name), type.getType(),
                                              block(), params);
}

stmt_ptr Parser::statement() {
  expr_ptr expr;
  stmt_ptr result;
  Token token = peek;
  switch (peek.tag) {
  case Tag::RETURN:
    next();
    expr = expression();
    match(Tag::SEMICOLON, NO_SEMICOLON);
    result =
        std::make_unique<ReturnStatement>(std::move(token), std::move(expr));
    return result;
  case Tag::IF:
  case Tag::WHILE:
    return conditionalStatement();
  case Tag::TYPE:
    return variableDefiniton();
  case Tag::ID:
    return assignment();
  default:
    error("Expected a statement");
  }
  return nullptr;
}

stmt_ptr Parser::conditionalStatement() {
  bool if_ = peek.tag == Tag::IF;
  Token token = peek;
  next();
  match(Tag::OPEN_BRACKET, "Expected a conditional in brackets");
  expr_ptr condition = conditional();
  match(Tag::CLOSE_BRACKET, NO_CLOSING_BRACKET);
  stmt_ptr body = block();
  if (if_) {
    stmt_ptr elseBlock = nullptr;
    if (peek.tag == Tag::ELSE) {
      next();
      elseBlock = block();
    }
    return std::make_unique<IfStatement>(std::move(token), std::move(condition),
                                         std::move(body), std::move(elseBlock));
  }
  return std::make_unique<WhileStatement>(
      std::move(token), std::move(condition), std::move(body));
}

stmt_ptr Parser::block() {
  if (peek.tag != Tag::OPEN_CURLY) {
    return statement();
  }

  Token token = peek;
  next(); // '{'
  std::vector<stmt_ptr> block;
  while (peek.tag != Tag::CLOSE_CURLY) {
    block.push_back(statement());
  }
  next(); // '}'
  return std::make_unique<Sequence>(std::move(token), block);
}

stmt_ptr Parser::assignment() {
  id_ptr name = std::make_unique<Identifier>(std::move(peek), TypeID::NONE);
  next();
  match(Tag::ASSIGN, "Expected an assignment");
  expr_ptr expr = expression();
  match(Tag::SEMICOLON, NO_SEMICOLON);
  return std::make_unique<Assignment>(std::move(name), std::move(expr));
}

expr_ptr Parser::expression() {
  expr_ptr lhs = term();
  while (peek.tag == Tag::PLUS || peek.tag == Tag::MINUS) {
    Token op = std::move(peek);
    next();
    lhs = std::make_unique<BinaryOperation>(std::move(lhs), std::move(op),
                                            term());
  }
  return lhs;
}

expr_ptr Parser::term() {
  expr_ptr lhs = factor();
  while (peek.tag == Tag::TIMES || peek.tag == Tag::DIVIDE) {
    Token op = std::move(peek);
    next();
    lhs = std::make_unique<BinaryOperation>(std::move(lhs), std::move(op),
                                            factor());
  }
  return lhs;
}

expr_ptr Parser::factor() {
  if (peek.tag == Tag::MINUS || peek.tag == Tag::PLUS) {
    Token op = std::move(peek);
    next();
    return std::make_unique<UnaryOperation>(std::move(op), unary());
  }
  return unary();
}

expr_ptr Parser::unary() {
  const Tag tag = peek.tag;
  Token token = peek;
  expr_ptr expr;
  switch (tag) {
  case Tag::INT:
    next();
    expr = std::make_unique<Constant>(std::move(token), TypeID::INT);
    break;
  case Tag::DOUBLE:
    next();
    expr = std::make_unique<Constant>(std::move(token), TypeID::DOUBLE);
    break;
  case Tag::STRING:
    next();
    expr = std::make_unique<Constant>(std::move(token), TypeID::STRING);
    break;
  case Tag::ID:
  case Tag::I:
  case Tag::RE:
  case Tag::IM:
    expr = functionCall();
    break;
  case Tag::OPEN_BRACKET:
    next();
    expr = expression();
    match(Tag::CLOSE_BRACKET, NO_CLOSING_BRACKET);
    break;
  case Tag::VERTICAL:
    next();
    expr = expression();
    match(Tag::VERTICAL, "No match for opening of absolute value '|'");
    expr = std::make_unique<AbsoluteValue>(std::move(token), std::move(expr));
    break;
  default:
    error("Unexpected syntax");
  }
  if (peek.tag == Tag::I) {
    expr = std::make_unique<Complex>(std::move(expr), std::move(peek));
    next();
  }
  return expr;
}

expr_ptr Parser::functionCall() {
  id_ptr res = std::make_unique<Identifier>(std::move(peek), TypeID::NONE);
  next();
  if (peek.tag != Tag::OPEN_BRACKET) {
    return res;
  }
  next();
  std::vector<expr_ptr> args;
  while (peek.tag != Tag::CLOSE_BRACKET) {
    args.push_back(expression());
    if (peek.tag == Tag::COMMA) {
      next();
      if (peek.tag == Tag::CLOSE_BRACKET) {
        warning("Comma with no argument after in call to " +
                res->token.getString());
      }
    }
  }
  next(); // ')'
  return std::make_unique<FunctionCall>(std::move(res->token), args);
}

expr_ptr Parser::conditional() {
  expr_ptr lhs = conjunction();
  while (peek.tag == Tag::OR) {
    Token op = std::move(peek);
    next();
    lhs = std::make_unique<Disjunction>(std::move(lhs), std::move(op),
                                        conjunction());
  }
  return lhs;
}

expr_ptr Parser::conjunction() {
  expr_ptr lhs = negation();
  while (peek.tag == Tag::AND) {
    Token op = std::move(peek);
    next();
    lhs = std::make_unique<Conjunction>(std::move(lhs), std::move(op),
                                        negation());
  }
  return lhs;
}

expr_ptr Parser::negation() {
  if (peek.tag == Tag::NOT) {
    Token op = std::move(peek);
    next();
    return std::make_unique<Negation>(std::move(op), relation());
  }
  return relation();
}

expr_ptr Parser::relation() {
  if (peek.tag == Tag::OPEN_BRACKET) {
    next();
    expr_ptr inside = conditional();
    match(Tag::CLOSE_BRACKET, NO_CLOSING_BRACKET);
    return inside;
  }
  expr_ptr lhs = expression();
  Token op = std::move(peek);
  next();
  expr_ptr rhs = expression();
  return std::make_unique<Relation>(std::move(lhs), std::move(op),
                                    std::move(rhs));
}

Parser::Parser(Lexer &lexer_) : lexer(lexer_), peek(Tag::END, -1) {
  Node::module = std::make_unique<llvm::Module>("", Node::context);
  Node::symbols = SymbolTable();
  next();
}

stmt_ptr Parser::parseNext() {
  switch (peek.tag) {
  case Tag::TYPE:
    return variableDefiniton();
  case Tag::FUN:
    next();
    return functionDefinition();
  default:
    error("Expected variable or function definition");
  }
  return nullptr;
}

void Parser::parse() {
  while (peek.tag != Tag::END) {
    stmt_ptr stmt = parseNext();
    stmt->generate();
  }
  Node::initGlobals();
}