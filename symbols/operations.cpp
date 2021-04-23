#include "parse_tree.h"

Operation::Operation(Token token) : Expression(std::move(token)) {}

BinaryOperation::BinaryOperation(expr_ptr lhs_, Token operator_, expr_ptr rhs_)
    : Operation(std::move(operator_)), lhs(std::move(lhs_)),
      rhs(std::move(rhs_)) {}

llvm::Value *BinaryOperation::multiplyComplex(llvm::Value *re1,
                                              llvm::Value *im1,
                                              llvm::Value *re2,
                                              llvm::Value *im2) {
  auto mul = Complex::mul(re1, im1, re2, im2);
  return Complex::get(mul.first, mul.second);
}

llvm::Value *BinaryOperation::divideComplex(llvm::Value *re1, llvm::Value *im1,
                                            llvm::Value *re2,
                                            llvm::Value *im2) {
  llvm::Value *conjugateIm = builder.CreateFMul(im2, MINUS_ONE_DOUBLE);
  auto mulTop = Complex::mul(re1, im1, re2, conjugateIm);
  auto mulBottom = Complex::mul(re2, im2, re2, conjugateIm);

  return Complex::get(builder.CreateFDiv(mulTop.first, mulBottom.first),
                      builder.CreateFDiv(mulTop.second, mulBottom.first));
}

llvm::Value *BinaryOperation::generate() {
  llvm::Value *L = lhs->generate(), *R = rhs->generate();
  llvm::Type *common = getMaxType(L->getType(), R->getType());
  L = expand(L, common);
  R = expand(R, common);
  if (common == intType) {
    switch (token.tag) {
    case Tag::PLUS:
      return builder.CreateAdd(L, R);
    case Tag::MINUS:
      return builder.CreateSub(L, R);
    case Tag::TIMES:
      return builder.CreateMul(L, R);
    case Tag::DIVIDE:
      return builder.CreateSDiv(L, R);
    default:
      error("Unsupported binary operator", token.line);
    }
  } else if (common == doubleType) {
    switch (token.tag) {
    case Tag::PLUS:
      return builder.CreateFAdd(L, R);
    case Tag::MINUS:
      return builder.CreateFSub(L, R);
    case Tag::TIMES:
      return builder.CreateFMul(L, R);
    case Tag::DIVIDE:
      return builder.CreateFDiv(L, R);
    default:
      error("Unsupported binary operator", token.line);
    }
  } else if (common == complexStruct) {
    auto left = Complex::getComponents(L), right = Complex::getComponents(R);

    switch (token.tag) {
    case Tag::PLUS:
      return Complex::get(builder.CreateFAdd(left.first, right.first),
                          builder.CreateFAdd(left.second, right.second));
    case Tag::MINUS:
      return Complex::get(builder.CreateFSub(left.first, right.first),
                          builder.CreateFSub(left.second, right.second));
    case Tag::TIMES:
      return multiplyComplex(left.first, left.second, right.first,
                             right.second);
    case Tag::DIVIDE:
      return divideComplex(left.first, left.second, right.first, right.second);
    default:
      error("Unsupported binary operator", token.line);
    }
  }
  error("Unsupported types for binary operator", token.line);
  return nullptr;
}

UnaryOperation::UnaryOperation(Token operator_, expr_ptr expression_)
    : Operation(std::move(operator_)), expression(std::move(expression_)) {}

llvm::Value *UnaryOperation::generate() {
  llvm::Value *val = expression->generate();
  if (token.tag == Tag::MINUS) {
    if (val->getType() == intType) {
      return builder.CreateMul(val, MINUS_ONE_INT);
    } else if (val->getType() == doubleType) {
      return builder.CreateFMul(val, MINUS_ONE_DOUBLE);
    } else if (val->getType() == complexStruct) {
      auto comp = Complex::getComponents(val);
      return Complex::get(builder.CreateFMul(comp.first, MINUS_ONE_DOUBLE),
                          builder.CreateFMul(comp.second, MINUS_ONE_DOUBLE));
    } else {
      error("Unsupported type for unary operator", token.line);
    }
  }
  return val;
}

Constant::Constant(Token token, TypeID type_)
    : Expression(std::move(token)), type(type_) {}

llvm::Value *Constant::generate() {
  if (type == TypeID::DOUBLE) {
    return llvm::ConstantFP::get(context, llvm::APFloat(token.getDouble()));
  }
  if (type == TypeID::INT) {
    return llvm::ConstantInt::get(context,
                                  llvm::APInt(64, token.getInt(), true));
  }
  if (type == TypeID::STRING) {
    return builder.CreateGlobalStringPtr(llvm::StringRef(token.getString()), "",
                                         0U, module.get());
  }
  error("Unsupported constant", token.line);
  return nullptr;
}

LogicalOperation::LogicalOperation(expr_ptr lhs_, Token operator_,
                                   expr_ptr rhs_)
    : Expression(std::move(operator_)), lhs(std::move(lhs_)),
      rhs(std::move(rhs_)) {}

Disjunction::Disjunction(expr_ptr lhs_, Token operator_, expr_ptr rhs_)
    : LogicalOperation(std::move(lhs_), std::move(operator_), std::move(rhs_)) {
}

llvm::Value *Disjunction::generate() {
  return builder.CreateOr(lhs->generate(), rhs->generate());
}

Conjunction::Conjunction(expr_ptr lhs_, Token operator_, expr_ptr rhs_)
    : LogicalOperation(std::move(lhs_), std::move(operator_), std::move(rhs_)) {
}

llvm::Value *Conjunction::generate() {
  return builder.CreateAnd(lhs->generate(), rhs->generate());
}

Negation::Negation(Token operator_, expr_ptr expression_)
    : Expression(std::move(operator_)), expression(std::move(expression_)) {}

llvm::Value *Negation::generate() {
  return builder.CreateNot(expression->generate());
}

Relation::Relation(expr_ptr lhs_, Token operator_, expr_ptr rhs_)
    : LogicalOperation(std::move(lhs_), std::move(operator_), std::move(rhs_)) {
}

llvm::Value *Relation::generate() {
  llvm::Value *L = lhs->generate(), *R = rhs->generate();
  llvm::Type *common = getMaxType(L->getType(), R->getType());
  expand(L, common);
  expand(R, common);

  if (common == intType) {
    switch (token.tag) {
    case Tag::LT:
      return builder.CreateICmpSLT(L, R);
    case Tag::LE:
      return builder.CreateICmpSLE(L, R);
    case Tag::EQ:
      return builder.CreateICmpEQ(L, R);
    case Tag::NEQ:
      return builder.CreateICmpNE(L, R);
    case Tag::GE:
      return builder.CreateICmpSGE(L, R);
    case Tag::GT:
      return builder.CreateICmpSGT(L, R);
    default:
      error("Unsupported relational operator", token.line);
    }
  } else if (common == doubleType) {
    switch (token.tag) {
    case Tag::LT:
      return builder.CreateFCmpOLT(L, R);
    case Tag::LE:
      return builder.CreateFCmpOLE(L, R);
    case Tag::EQ:
      return builder.CreateFCmpOEQ(L, R);
    case Tag::NEQ:
      return builder.CreateFCmpONE(L, R);
    case Tag::GE:
      return builder.CreateFCmpOGE(L, R);
    case Tag::GT:
      return builder.CreateFCmpOGT(L, R);
    default:
      error("Unsupported relational operator", token.line);
    }
  } else if (common == complexStruct) {
    auto left = Complex::getComponents(L), right = Complex::getComponents(R);
    switch (token.tag) {
    case Tag::LT:
      return builder.CreateFCmpOLT(left.first, right.first);
    case Tag::LE:
      return builder.CreateOr(
          builder.CreateFCmpOLT(left.first, right.first),
          builder.CreateAnd(builder.CreateFCmpOEQ(left.first, right.first),
                            builder.CreateFCmpOLE(left.second, right.second)));
    case Tag::EQ:
      return builder.CreateAnd(
          builder.CreateFCmpOEQ(left.first, right.first),
          builder.CreateFCmpOEQ(left.second, right.second));
    case Tag::NEQ:
      return builder.CreateOr(builder.CreateFCmpONE(left.first, right.first),
                              builder.CreateFCmpONE(left.second, right.second));
    case Tag::GE:
      return builder.CreateOr(
          builder.CreateFCmpOGT(left.first, right.first),
          builder.CreateAnd(builder.CreateFCmpOEQ(left.first, right.first),
                            builder.CreateFCmpOGE(left.second, right.second)));
    case Tag::GT:
      return builder.CreateFCmpOGT(left.first, right.first);
    default:
      error("Unsupported relational operator", token.line);
    }
  }
  error("Unsupported types for comparison operator", token.line);
  return nullptr;
}