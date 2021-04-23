#include "lexer.h"

LexerError::LexerError(const std::string &msg) : std::runtime_error(msg) {}

void Lexer::reserve(const std::vector<std::pair<std::string, Tag>> &keywords) {
  for (const auto &p : keywords) {
    reserved.insert({p.first, Token(p.second, p.first, -1)});
  }
}

void Lexer::reserve(const std::vector<std::pair<std::string, TypeID>> &types) {
  for (const auto &p : types) {
    reserved.insert({p.first, Token(p.second, -1)});
  }
}

Lexer::Lexer(std::istream &stream_) : line(1), stream(stream_) {
  reserve({{"if", Tag::IF},
           {"else", Tag::ELSE},
           {"while", Tag::WHILE},
           {"fun", Tag::FUN},
           {"main", Tag::MAIN},
           {"return", Tag::RETURN},
           {"Re", Tag::RE},
           {"Im", Tag::IM},
           {"and", Tag::AND},
           {"or", Tag::OR},
           {"not", Tag::NOT}});
  reserve({{"int", TypeID::INT},
           {"double", TypeID::DOUBLE},
           {"complex", TypeID::COMPLEX},
           {"string", TypeID::STRING}});

  readNext();
}

void Lexer::readNext() {
  if (stream.eof()) {
    peek = EOF;
  } else {
    if (!stream) {
      throw LexerError("Failure when reading stream.");
    }
    peek = stream.get();
  }
}

bool Lexer::readNext(char next) {
  readNext();
  if (peek != next) {
    return false;
  }
  peek = 0;
  return true;
}

void Lexer::error(char token) {
  throw LexerError("Invalid token " + std::string(1, token) + " at line " +
                   std::to_string(line) + ".");
}

inline Token Lexer::ret(Token token) {
  previous = token.tag;
  return token;
}

inline Token Lexer::ret(Tag tag) { return ret(Token(tag, line)); }

void Lexer::whitespace() {
  while (peek <= ' ' && peek != EOF) {
    if (peek == '\n') {
      ++line;
    }
    readNext();
  }
}

Token Lexer::equals() {
  if (readNext('=')) {
    return Token(Tag::EQ, line);
  } else {
    return Token(Tag::ASSIGN, line);
  }
}

Token Lexer::notEquals() {
  if (readNext('=')) {
    return Token(Tag::NEQ, line);
  } else {
    error('!');
    return Token(Tag::END, line);
  }
}

Token Lexer::lessThan() {
  if (readNext('=')) {
    return Token(Tag::LE, line);
  } else {
    return Token(Tag::LT, line);
  }
}

Token Lexer::greaterThan() {
  if (readNext('=')) {
    return Token(Tag::GE, line);
  } else {
    return Token(Tag::GT, line);
  }
}

Token Lexer::quotation() {
  std::string literal;
  int lineBegin = line;
  do {
    readNext();
    if (peek == '\n') {
      ++line;
    } else if (peek == '\\') {
      readNext();
      switch (peek) {
      case 'n':
        peek = 10; // \n
        break;

      case 't':
        peek = 9; // \t
        break;
      }
    }
    literal += peek;
  } while (peek != '"' && peek != EOF);

  if (peek == EOF) {
    throw LexerError("String literal at " + std::to_string(lineBegin) +
                     " not closed.");
  }

  literal.pop_back();
  peek = 0;
  return Token(Tag::STRING, literal, line);
}

Token Lexer::digit() {
  int64_t value = 0;
  double dvalue = 0.0;
  std::string printable;
  do {
    value = value * 10 + peek - '0';
    dvalue = dvalue * 10 + peek - '0';
    printable += peek;
    readNext();
  } while (isdigit(peek));

  if (peek != '.') {
    return Token(value, line);
  }

  printable += '.';
  double dividor = 10;
  readNext();
  while (isdigit(peek)) {
    dvalue += (peek - '0') / dividor;
    printable += peek;
    dividor *= 10;
    readNext();
  }
  return Token(dvalue, line);
}

Token Lexer::alpha() {
  std::string word;
  do {
    word += peek;
    readNext();
  } while (isalnum(peek) || peek == '_');

  // Case when 'i' is the imaginary unit - depending on the previous token.
  if (word == "i" && (previous == Tag::INT || previous == Tag::DOUBLE ||
                      previous == Tag::CLOSE_BRACKET || previous == Tag::ID ||
                      previous == Tag::VERTICAL)) {
    return Token(Tag::I, word, line);
  }

  if (reserved.find(word) != reserved.end()) {
    Token res = reserved.at(word);
    res.line = line;
    return res;
  }

  return Token(Tag::ID, word, line);
}

Token Lexer::getNextToken() {
  whitespace();

  if (isdigit(peek)) {
    return ret(digit());
  }

  if (isalpha(peek) || peek == '_') {
    return ret(alpha());
  }

  char curr = peek;
  peek = 0;
  switch (curr) {
  case '=':
    return ret(equals());

  case '!':
    return ret(notEquals());

  case '<':
    return ret(lessThan());

  case '>':
    return ret(greaterThan());

  case '"':
    return ret(quotation());

  default:
    if (operators.find(curr) != operators.end()) {
      return ret(operators.at(curr));
    }

    error(curr);
    return Token(Tag::END, line);
  }
}