#include "parse_tree.h"

CodeGenError::CodeGenError(const std::string &err) : std::runtime_error(err) {}

llvm::LLVMContext Node::context;
llvm::IRBuilder<> Node::builder(context);
std::unique_ptr<llvm::Module> Node::module;
SymbolTable Node::symbols;

llvm::Type *Node::intType = llvm::Type::getInt64Ty(context);
llvm::Type *Node::doubleType = llvm::Type::getDoubleTy(context);
llvm::Type *Node::boolType = llvm::Type::getInt1Ty(context);
llvm::Type *Node::stringType = llvm::Type::getInt8PtrTy(context);
llvm::StructType *Node::complexStruct =
    llvm::StructType::get(context, {doubleType, doubleType});

llvm::Constant *Node::TRUE =
    llvm::ConstantInt::get(boolType, llvm::APInt(1, 1, false));

llvm::Constant *Node::FALSE =
    llvm::ConstantInt::get(boolType, llvm::APInt(1, 0, false));

llvm::Constant *Node::MINUS_ONE_INT =
    llvm::ConstantInt::get(intType, llvm::APInt(64, -1, true));

llvm::Constant *Node::MINUS_ONE_DOUBLE =
    llvm::ConstantFP::get(doubleType, llvm::APFloat(-1.0));

llvm::Constant *Node::INT_ZERO =
    llvm::ConstantInt::get(intType, llvm::APInt(64, 0, true));
llvm::Constant *Node::DOUBLE_ZERO =
    llvm::ConstantFP::get(doubleType, llvm::APFloat(0.0));
llvm::Constant *Node::COMPLEX_ZERO =
    llvm::ConstantStruct::get(complexStruct, {DOUBLE_ZERO, DOUBLE_ZERO});
llvm::Constant *Node::STRING_ZERO = llvm::ConstantPointerNull::get(
    static_cast<llvm::PointerType *>(stringType));

Node::Node(Token token_) : token(std::move(token_)) {}

Node::~Node() = default;

void Node::error(const std::string &msg, int line) {
  std::string err = "[ERROR] " + msg + " at line " + std::to_string(line);
  throw CodeGenError(err);
}

Identifier *Node::getSymbol(const std::string &name) {
  Identifier *symbol = symbols.get(name);
  if (!symbol) {
    error("Undefined identifier " + name, token.line);
  }
  return symbol;
}

llvm::Type *Node::getType(TypeID type) {
  switch (type) {
  case TypeID::INT:
    return intType;
  case TypeID::DOUBLE:
    return doubleType;
  case TypeID::COMPLEX:
    return complexStruct;
  case TypeID::STRING:
    return stringType;
  default:
    error("Unsupported type", token.line);
    return nullptr;
  }
}

llvm::Type *Node::getMaxType(llvm::Type *a, llvm::Type *b) {
  if (a == stringType || b == stringType) {
    error("Error - strings cannot be converted to other types", token.line);
  }
  if (a == complexStruct || b == complexStruct) {
    return complexStruct;
  }
  if (a == doubleType || b == doubleType) {
    return doubleType;
  }
  return intType;
}

llvm::Value *Node::expand(llvm::Value *val, llvm::Type *to) {
  if (val->getType() == to) {
    return val;
  }
  if (val->getType() == Node::intType) {
    val = builder.CreateSIToFP(val, Node::doubleType);
  }
  if (to == doubleType) {
    return val;
  }
  if (to == complexStruct) {
    llvm::AllocaInst *alloc = builder.CreateAlloca(complexStruct, nullptr);
    builder.CreateStore(val, Complex::re(alloc));
    builder.CreateStore(DOUBLE_ZERO, Complex::im(alloc));
    return builder.CreateLoad(complexStruct, alloc);
  }
  error("Unsupported type conversion", token.line);
  return nullptr;
}

void Node::initGlobals() {
  llvm::Function *mainFunc = module->getFunction("main");
  if (!mainFunc || mainFunc->empty()) {
    throw CodeGenError("Missing main() function definiton");
  }

  builder.SetInsertPoint(&*mainFunc->getEntryBlock().begin());
  for (auto &global : SymbolTable::globals()) {
    const expr_ptr &expr = std::get<expr_ptr>(global);
    llvm::Value *expanded =
        expr->expand(expr->generate(), expr->getType(std::get<TypeID>(global)));

    builder.CreateStore(expanded, std::get<llvm::GlobalVariable *>(global));
  }
}

Expression::Expression(Token token) : Node(std::move(token)) {}

Identifier::Identifier(Token id, TypeID type_, llvm::Value *alloc_)
    : Expression(std::move(id)), type(type_), alloc(alloc_) {}

llvm::Value *Identifier::generate() {
  const std::string name = token.getString();
  Identifier *id = getSymbol(name);
  return builder.CreateLoad(getType(id->type), id->alloc, name);
}

FunctionCall::FunctionCall(Token name, std::vector<expr_ptr> &args)
    : Expression(std::move(name)), arguments(std::move(args)) {}

llvm::Value *FunctionCall::Re(llvm::Value *val) {
  if (val->getType() == intType || val->getType() == doubleType) {
    return val;
  }
  if (val->getType() == complexStruct) {
    auto comp = Complex::getComponents(val);
    return comp.first;
  }
  error("Unsupported type in call to Re()", token.line);
  return nullptr;
}

llvm::Value *FunctionCall::Im(llvm::Value *val) {
  if (val->getType() == intType) {
    return INT_ZERO;
  }
  if (val->getType() == doubleType) {
    return DOUBLE_ZERO;
  }
  if (val->getType() == complexStruct) {
    auto comp = Complex::getComponents(val);
    return comp.second;
  }
  error("Unsupported type in call to Im()", token.line);
  return nullptr;
}

llvm::Value *FunctionCall::generate() {
  const std::string name = token.getString();

  if (token.tag == Tag::RE) {
    if (arguments.size() != 1) {
      error("Incorrect number of parameters in call to Re()", token.line);
    }
    return Re(arguments.front()->generate());
  }

  if (token.tag == Tag::IM) {
    if (arguments.size() != 1) {
      error("Incorrect number of parameters in call to Im()", token.line);
    }
    return Im(arguments.front()->generate());
  }

  llvm::Function *func = module->getFunction(name);
  if (!func) {
    error("Function " + name + " not defined", token.line);
  }

  std::vector<llvm::Value *> args;
  int i = 0;
  for (const auto &arg : func->args()) {
    if (i >= arguments.size()) {
      error("Incorrect number of parameters in call to " +
                func->getName().str(),
            token.line);
    }
    args.push_back(expand(arguments[i++]->generate(), arg.getType()));
  }
  return builder.CreateCall(func, args);
}

AbsoluteValue::AbsoluteValue(Token token, expr_ptr value)
    : Expression(std::move(token)), val_(std::move(value)) {}

llvm::Value *AbsoluteValue::generate() {
  llvm::Value *val = val_->generate();
  if (val->getType() == intType) {
    return builder.CreateCall(
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::abs,
                                        {intType}),
        {val, FALSE});
  } else if (val->getType() == doubleType) {
    return builder.CreateCall(
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::fabs,
                                        {doubleType}),
        {val});
  } else if (val->getType() == complexStruct) {
    llvm::AllocaInst *alloc = builder.CreateAlloca(complexStruct, nullptr);
    val = builder.CreateStore(val, alloc);

    llvm::Value *re = builder.CreateLoad(doubleType, Complex::re(alloc)),
                *im = builder.CreateLoad(doubleType, Complex::im(alloc));

    re = builder.CreateFMul(re, re);
    im = builder.CreateFMul(im, im);
    llvm::Value *sum = builder.CreateFAdd(re, im);
    return builder.CreateCall(
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::sqrt,
                                        {doubleType}),
        {sum});
  }
  error("Unsupported type inside absolute value", val_->token.line);
  return nullptr;
}

Complex::Complex(expr_ptr imaginary_, Token token)
    : Expression(std::move(token)), imaginary(std::move(imaginary_)) {}

llvm::Value *Complex::generate() {
  llvm::Value *re = DOUBLE_ZERO,
              *im = expand(imaginary->generate(), doubleType);
  return get(re, im);
}

llvm::GetElementPtrInst *Complex::re(llvm::Value *complex) {
  static std::vector<llvm::Value *> reIndex{
      llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)),
      llvm::ConstantInt::get(context, llvm::APInt(32, 0, true))};

  return static_cast<llvm::GetElementPtrInst *>(
      builder.CreateGEP(complexStruct, complex, reIndex));
}

llvm::GetElementPtrInst *Complex::im(llvm::Value *complex) {
  static std::vector<llvm::Value *> imIndex{
      llvm::ConstantInt::get(context, llvm::APInt(32, 0, true)),
      llvm::ConstantInt::get(context, llvm::APInt(32, 1, true))};

  return static_cast<llvm::GetElementPtrInst *>(
      builder.CreateGEP(complexStruct, complex, imIndex));
}

llvm::Value *Complex::get(llvm::Value *real_, llvm::Value *im_) {
  llvm::AllocaInst *alloc = builder.CreateAlloca(complexStruct, nullptr);
  builder.CreateStore(real_, re(alloc));
  builder.CreateStore(im_, im(alloc));

  return builder.CreateLoad(complexStruct, alloc);
}

std::pair<llvm::Value *, llvm::Value *> Complex::mul(llvm::Value *re1,
                                                     llvm::Value *im1,
                                                     llvm::Value *re2,
                                                     llvm::Value *im2) {
  llvm::Value *reMul = builder.CreateFMul(re1, re2),
              *imMul = builder.CreateFMul(im1, im2),
              *cross1 = builder.CreateFMul(re1, im2),
              *cross2 = builder.CreateFMul(im1, re2);

  return std::make_pair<llvm::Value *, llvm::Value *>(
      builder.CreateFSub(reMul, imMul), builder.CreateFAdd(cross1, cross2));
}

std::pair<llvm::Value *, llvm::Value *>
Complex::getComponents(llvm::Value *complex) {
  llvm::AllocaInst *alloc = builder.CreateAlloca(complexStruct, nullptr);
  builder.CreateStore(complex, alloc);

  return std::make_pair<llvm::Value *, llvm::Value *>(
      builder.CreateLoad(doubleType, re(alloc)),
      builder.CreateLoad(doubleType, im(alloc)));
}