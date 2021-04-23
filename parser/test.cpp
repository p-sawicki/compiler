#include "parser.h"
#include "gtest/gtest.h"

stmt_ptr parse(const std::string &input) {
  std::stringstream ss(input);
  Lexer lexer(ss);
  Parser parser(lexer);

  return parser.parseNext();
}

TEST(parser_test, arithmetic) {
  std::string in = "fun main : int() { \
    int a = -1 + 2 * 3; \
    a = (1 - 2) / |3|; \
    return 0; \
    }";
  stmt_ptr parseTree = parse(in);

  auto func = dynamic_cast<FunctionDefinition *>(parseTree.get());
  EXPECT_NE(func, nullptr);

  Sequence *block = dynamic_cast<Sequence *>(func->block.get());
  EXPECT_NE(block, nullptr);

  auto a = dynamic_cast<VariableDefinition *>(block->statements[0].get());
  auto b = dynamic_cast<Assignment *>(block->statements[1].get());
  EXPECT_NE(a, nullptr);
  EXPECT_NE(b, nullptr);

  auto ret = dynamic_cast<ReturnStatement *>(block->statements[2].get());
  EXPECT_NE(ret, nullptr);

  /**
   * Expected:
   * add
   *    unary -
   *        1
   *    mul
   *        2
   *        3
   * */

  auto topA = dynamic_cast<BinaryOperation *>(a->expression.get());
  auto leftA = dynamic_cast<UnaryOperation *>(topA->lhs.get());
  auto unaryExpr = dynamic_cast<Constant *>(leftA->expression.get());
  auto rightA = dynamic_cast<BinaryOperation *>(topA->rhs.get());

  EXPECT_NE(topA, nullptr);
  EXPECT_NE(leftA, nullptr);
  EXPECT_NE(rightA, nullptr);

  EXPECT_EQ(topA->token.tag, Tag::PLUS);
  EXPECT_EQ(unaryExpr->type, TypeID::INT);
  EXPECT_EQ(rightA->token.tag, Tag::TIMES);

  /**
   * Expected:
   * div
   *    sub
   *        1
   *        2
   *    abs
   *        3
   * */

  auto topB = dynamic_cast<BinaryOperation *>(b->expression.get());
  auto leftB = dynamic_cast<BinaryOperation *>(topB->lhs.get());
  auto rightB = dynamic_cast<AbsoluteValue *>(topB->rhs.get());

  EXPECT_NE(topB, nullptr);
  EXPECT_NE(leftB, nullptr);
  EXPECT_NE(rightB, nullptr);

  EXPECT_EQ(topB->token.tag, Tag::DIVIDE);
  EXPECT_EQ(leftB->token.tag, Tag::MINUS);
}

TEST(parser_test, logic) {
  std::string in("fun main :int () { \
    if (1 == 1 and 1 != 0 or not 1 < 0) { \
      return 0; \
    } \
    while (1 <= 1 and not (1 > 0 or 1 >= 0)) { \
      return -1; \
    } \
    return 2;\
  }");
  stmt_ptr parseTree = parse(in);

  auto func = dynamic_cast<FunctionDefinition *>(parseTree.get());
  auto block = dynamic_cast<Sequence *>(func->block.get());

  auto if_ = dynamic_cast<IfStatement *>(block->statements[0].get());
  auto while_ = dynamic_cast<WhileStatement *>(block->statements[1].get());

  EXPECT_NE(if_, nullptr);
  EXPECT_NE(while_, nullptr);

  /**
   * Expected:
   * or
   *    and
   *        rel ==
   *        rel !=
   *    not
   *        rel <
   * */

  auto ifTop = dynamic_cast<Disjunction *>(if_->condition.get());
  auto ifLeft = dynamic_cast<Conjunction *>(ifTop->lhs.get());
  auto ifLL = dynamic_cast<Relation *>(ifLeft->lhs.get());
  auto ifLR = dynamic_cast<Relation *>(ifLeft->rhs.get());
  auto ifRight = dynamic_cast<Negation *>(ifTop->rhs.get());
  auto negated = dynamic_cast<Relation *>(ifRight->expression.get());

  EXPECT_NE(ifTop, nullptr);
  EXPECT_NE(ifLeft, nullptr);
  EXPECT_NE(ifLL, nullptr);
  EXPECT_NE(ifLR, nullptr);
  EXPECT_NE(ifRight, nullptr);
  EXPECT_NE(negated, nullptr);

  EXPECT_EQ(ifLL->token.tag, Tag::EQ);
  EXPECT_EQ(ifLR->token.tag, Tag::NEQ);
  EXPECT_EQ(negated->token.tag, Tag::LT);

  /**
   * Expected:
   * and
   *    rel <=
   *    not
   *        or
   *            rel >
   *            rel >=
   * */

  auto whileTop = dynamic_cast<Conjunction *>(while_->condition.get());
  auto whileLeft = dynamic_cast<Relation *>(whileTop->lhs.get());
  auto whileRight = dynamic_cast<Negation *>(whileTop->rhs.get());
  auto wNegated = dynamic_cast<Disjunction *>(whileRight->expression.get());
  auto whileRL = dynamic_cast<Relation *>(wNegated->lhs.get());
  auto whileRR = dynamic_cast<Relation *>(wNegated->rhs.get());

  EXPECT_NE(whileTop, nullptr);
  EXPECT_NE(whileLeft, nullptr);
  EXPECT_NE(whileRight, nullptr);
  EXPECT_NE(wNegated, nullptr);
  EXPECT_NE(whileRL, nullptr);
  EXPECT_NE(whileRR, nullptr);

  EXPECT_EQ(whileLeft->token.tag, Tag::LE);
  EXPECT_EQ(whileRL->token.tag, Tag::GT);
  EXPECT_EQ(whileRR->token.tag, Tag::GE);
}

TEST(parser_test, missing_semicolon) {
  std::string in("fun main :int () {\
    int a = 0 \
    return a; \
  }");
  try {
    parse(in);
    FAIL();
  } catch (ParserError &err) {
    EXPECT_NE(std::string(err.what()).find(Parser::NO_SEMICOLON),
              std::string::npos);
  }
}

TEST(parser_test, missing_colon) {
  std::string in("fun main int () {}");
  try {
    parse(in);
    FAIL();
  } catch (ParserError &err) {
    EXPECT_NE(std::string(err.what()).find(Parser::NO_COLON),
              std::string::npos);
  }
}

TEST(codegen_test, no_return_stmt) {
  std::string in("fun main :int () int a = 1;");
  stmt_ptr stmt = parse(in);
  EXPECT_THROW(stmt->generate(), CodeGenError);
}

TEST(codegen_test, undeclared_variable) {
  std::string in("fun main :int () {\
    a = 1;\
    return a;\
  }");
  stmt_ptr stmt = parse(in);
  EXPECT_THROW(stmt->generate(), CodeGenError);
}

TEST(codegen_test, undeclared_function) {
  std::string in("fun main :int () {\
    int i = f(0); \
    return i; \
  }");
  stmt_ptr stmt = parse(in);
  EXPECT_THROW(stmt->generate(), CodeGenError);
}