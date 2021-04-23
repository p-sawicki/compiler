#include "lexer.h"
#include "gtest/gtest.h"
#include <climits>

Token firstToken(const std::string &input) {
  std::stringstream stream(input);
  Lexer lexer(stream);
  return lexer.getNextToken();
}

void expectToken(Lexer &lexer, Tag tag) {
  Token token = lexer.getNextToken();
  EXPECT_EQ(token.tag, tag);
}

TEST(lexer_test, empty_stream) {
  Token token = firstToken("");
  EXPECT_EQ(token.tag, Tag::END);
}

TEST(lexer_test, integer) {
  Token token = firstToken("420");
  EXPECT_EQ(token.tag, Tag::INT);
  EXPECT_EQ(token.getInt(), 420l);
}

TEST(lexer_test, double_) {
  Token token = firstToken("420.42");
  EXPECT_EQ(token.tag, Tag::DOUBLE);
  EXPECT_DOUBLE_EQ(token.getDouble(), 420.42);
}

TEST(lexer_test, complex) {
  std::stringstream stream("420 + 4.2i");
  Lexer lexer(stream);

  expectToken(lexer, Tag::INT);
  expectToken(lexer, Tag::PLUS);
  expectToken(lexer, Tag::DOUBLE);
  expectToken(lexer, Tag::I);
}

TEST(lexer_test, relational_operators) {
  std::stringstream stream("\t==\t !=\t <\t <= > >=");
  Lexer lexer(stream);

  expectToken(lexer, Tag::EQ);
  expectToken(lexer, Tag::NEQ);
  expectToken(lexer, Tag::LT);
  expectToken(lexer, Tag::LE);
  expectToken(lexer, Tag::GT);
  expectToken(lexer, Tag::GE);
}

TEST(lexer_test, literal) {
  const std::string text = "Hello world!\n";
  Token token = firstToken("\"" + text + "\"");
  EXPECT_EQ(token.tag, Tag::STRING);
  EXPECT_EQ(token.getString(), text);

  EXPECT_THROW(firstToken("\"" + text), LexerError);
}

TEST(lexer_test, identifier) {
  const std::string name = "_variable123";
  Token token = firstToken(name);
  EXPECT_EQ(token.tag, Tag::ID);
  EXPECT_EQ(token.getString(), name);
}

TEST(lexer_test, keywords) {
  std::stringstream stream("\n\n\t   int double complex string fun \
        main or and not if while return Re Im");
  Lexer lexer(stream);

  for (int i = 0; i < 4; ++i) {
    expectToken(lexer, Tag::TYPE);
  }

  expectToken(lexer, Tag::FUN);
  expectToken(lexer, Tag::MAIN);
  expectToken(lexer, Tag::OR);
  expectToken(lexer, Tag::AND);
  expectToken(lexer, Tag::NOT);
  expectToken(lexer, Tag::IF);
  expectToken(lexer, Tag::WHILE);
  expectToken(lexer, Tag::RETURN);
  expectToken(lexer, Tag::RE);
  expectToken(lexer, Tag::IM);
}

TEST(lexer_test, assignment) {
  std::stringstream stream("int i = 0");
  Lexer lexer(stream);

  expectToken(lexer, Tag::TYPE);
  expectToken(lexer, Tag::ID);
  expectToken(lexer, Tag::ASSIGN);

  auto token = lexer.getNextToken();
  EXPECT_EQ(token.tag, Tag::INT);
  EXPECT_EQ(token.getInt(), 0l);
}

TEST(lexer_test, single_characters) {
  std::stringstream stream;
  for (char i = ' ' + 1; i < CHAR_MAX; ++i) {
    if (isprint(i) && i != '"') {
      stream << i << " ";
    }
  }
  Lexer lexer(stream);
  for (char i = ' ' + 1; i < CHAR_MAX; ++i) {
    if (isprint(i) && i != '"') {
      if (i == 'i') {
        expectToken(lexer, Tag::I);
      } else if (isalpha(i) || i == '_') {
        expectToken(lexer, Tag::ID);
      } else if (isdigit(i)) {
        expectToken(lexer, Tag::INT);
      } else if (operators.find(i) != operators.end()) {
        expectToken(lexer, operators.at(i));
      } else {
        EXPECT_THROW(lexer.getNextToken(), LexerError);
      }
    }
  }
}

TEST(lexer_test, case_sensitivity) {
  std::stringstream stream("Int dOuble re iM RETURN");
  Lexer lexer(stream);

  expectToken(lexer, Tag::ID);
  expectToken(lexer, Tag::ID);
  expectToken(lexer, Tag::ID);
  expectToken(lexer, Tag::ID);
  expectToken(lexer, Tag::ID);
}