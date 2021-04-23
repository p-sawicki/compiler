#include "parse_tree.h"

Statement::Statement(Token token) : Node(std::move(token)) {}

IfStatement::IfStatement(Token token, expr_ptr condition_, stmt_ptr ifBlock_,
                         stmt_ptr elseBlock_)
    : Statement(std::move(token)), condition(std::move(condition_)),
      ifBlock(std::move(ifBlock_)), elseBlock(std::move(elseBlock_)) {}

llvm::Value *IfStatement::generate() {
  llvm::Value *cond = condition->generate();
  cond = builder.CreateICmpNE(cond,
                              llvm::ConstantInt::get(context, llvm::APInt()));

  llvm::Function *func = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *if_ = llvm::BasicBlock::Create(context, "", func),
                   *cont = llvm::BasicBlock::Create(context),
                   *else_ =
                       elseBlock ? llvm::BasicBlock::Create(context) : cont;

  builder.CreateCondBr(cond, if_, else_);
  builder.SetInsertPoint(if_);

  symbols.push();
  llvm::Value *then = ifBlock->generate();
  symbols.pop();

  if (llvm::dyn_cast<llvm::ReturnInst>(then) == nullptr) {
    builder.CreateBr(cont);
  }

  if (elseBlock) {
    func->getBasicBlockList().push_back(else_);
    builder.SetInsertPoint(else_);

    symbols.push();
    llvm::Value *elseStmt = elseBlock->generate();
    symbols.pop();

    if (llvm::dyn_cast<llvm::ReturnInst>(elseStmt) == nullptr) {
      builder.CreateBr(cont);
    }
  }

  func->getBasicBlockList().push_back(cont);
  builder.SetInsertPoint(cont);

  return TRUE;
}

WhileStatement::WhileStatement(Token token, expr_ptr condition_,
                               stmt_ptr block_)
    : Statement(std::move(token)), condition(std::move(condition_)),
      block(std::move(block_)) {}

llvm::Value *WhileStatement::generate() {
  llvm::Function *func = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *preCond = llvm::BasicBlock::Create(context, "", func);
  builder.CreateBr(preCond);
  builder.SetInsertPoint(preCond);

  llvm::Value *cond = condition->generate();
  cond = builder.CreateICmpNE(cond,
                              llvm::ConstantInt::get(context, llvm::APInt()));

  llvm::BasicBlock *loop = llvm::BasicBlock::Create(context, "", func),
                   *cont = llvm::BasicBlock::Create(context);

  builder.CreateCondBr(cond, loop, cont);
  builder.SetInsertPoint(loop);

  symbols.push();
  llvm::Value *body = block->generate();
  symbols.pop();

  if (llvm::dyn_cast<llvm::ReturnInst>(body) == nullptr) {
    builder.CreateBr(preCond);
  }

  func->getBasicBlockList().push_back(cont);
  builder.SetInsertPoint(cont);

  return TRUE;
}

ReturnStatement::ReturnStatement(Token token, expr_ptr return__)
    : Statement(token), return_(std::move(return__)) {}

llvm::Value *ReturnStatement::generate() {
  llvm::Function *func = builder.GetInsertBlock()->getParent();
  return builder.CreateRet(expand(return_->generate(), func->getReturnType()));
}

Assignment::Assignment(id_ptr identifier_, expr_ptr expression_)
    : Statement(identifier_->token), identifier(std::move(identifier_)),
      expression(std::move(expression_)) {}

llvm::Value *Assignment::generate() {
  Identifier *lhs = getSymbol(identifier->token.getString());
  llvm::Value *rhs = expand(expression->generate(), getType(lhs->type));
  builder.CreateStore(rhs, lhs->alloc);
  return rhs;
}

VariableDefinition::VariableDefinition(id_ptr identifier_, expr_ptr expression_)
    : Assignment(std::move(identifier_), std::move(expression_)) {}

llvm::Value *VariableDefinition::generate() {
  llvm::BasicBlock *block = builder.GetInsertBlock();
  llvm::Function *func = nullptr;
  if (block) {
    func = block->getParent();
  }
  llvm::Type *type = getType(identifier->type);
  const std::string name = identifier->token.getString();
  llvm::Value *alloc;
  if (func) {
    llvm::Value *init = expand(expression->generate(), type);
    alloc = entryBlockAlloca(func, name, type);
    builder.CreateStore(init, alloc);
  } else {
    llvm::Constant *constInit;
    switch (identifier->type) {
    case TypeID::INT:
      constInit = INT_ZERO;
      break;
    case TypeID::DOUBLE:
      constInit = DOUBLE_ZERO;
      break;
    case TypeID::COMPLEX:
      constInit = COMPLEX_ZERO;
      break;
    case TypeID::STRING:
      constInit = STRING_ZERO;
      break;
    default:
      error("Unsupported type of global variable " + name,
            identifier->token.line);
    }
    llvm::GlobalVariable *global = new llvm::GlobalVariable(
        *module, type, false, llvm::GlobalValue::CommonLinkage, constInit,
        name);
    alloc = global;
    symbols.addGlobal(global, std::move(expression), identifier->type);
  }
  identifier->alloc = alloc;
  symbols.add(name, std::move(identifier));
  return alloc;
}

FunctionDeclaration::FunctionDeclaration(Token id_, TypeID returnType_,
                                         std::vector<id_ptr> &params)
    : Statement(id_), parameters(std::move(params)), returnType(returnType_) {}

llvm::Value *FunctionDeclaration::generate() {
  if (token.tag != Tag::ID && token.tag != Tag::MAIN) {
    error("Cannot redefine reserved keyword " + token.getString(), token.line);
  }

  if (token.tag == Tag::MAIN &&
      (!parameters.empty() || returnType != TypeID::INT)) {
    error("Invalid main function signature", token.line);
  }

  std::vector<llvm::Type *> types;
  for (const auto &param : parameters) {
    types.push_back(getType(param->type));
  }

  llvm::Type *funcReturnType = getType(returnType);
  llvm::FunctionType *ft =
      llvm::FunctionType::get(funcReturnType, types, false);
  return llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                token.getString(), *module);
}

FunctionDefinition::FunctionDefinition(Token id_, TypeID returnType_,
                                       stmt_ptr block_,
                                       std::vector<id_ptr> &params)
    : FunctionDeclaration(std::move(id_), returnType_, params),
      block(std::move(block_)) {}

llvm::Value *FunctionDefinition::generate() {
  const std::string name = token.getString();
  llvm::Function *func = module->getFunction(name);
  if (!func) {
    func = static_cast<llvm::Function *>(FunctionDeclaration::generate());
  } else if (!func->empty()) {
    error("Two functions with the same name: " + name, token.line);
  }

  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "", func);
  builder.SetInsertPoint(bb);

  symbols.push();

  int i = 0;
  for (auto &arg : func->args()) {
    if (i >= parameters.size() ||
        arg.getType() != getType(parameters[i]->type)) {
      error("Mismatch between signatures in definition and declaration of " +
                func->getName().str(),
            parameters[i]->token.line);
    }
    arg.setName(parameters[i]->token.getString());

    llvm::AllocaInst *alloc =
        entryBlockAlloca(func, arg.getName().str(), arg.getType());
    parameters[i]->alloc = alloc;
    builder.CreateStore(&arg, alloc);

    symbols.add(arg.getName().str(), std::move(parameters[i]));
    ++i;
  }
  if (i != parameters.size()) {
    error("Mismatch between signatures in definition and declaration of " +
              func->getName().str(),
          parameters[i]->token.line);
  }

  llvm::Value *ret = block->generate();
  if (llvm::dyn_cast<llvm::ReturnInst>(ret) == nullptr) {
    error("Function " + token.getString() +
              " does not end with a return statement",
          token.line);
  }

  symbols.pop();
  if (llvm::verifyFunction(*func)) {
    error("Function " + token.getString() + " could not be verified",
          token.line);
  }
  return func;
}

Sequence::Sequence(Token token, std::vector<stmt_ptr> &statements_)
    : Statement(std::move(token)), statements(std::move(statements_)) {}

llvm::Value *Sequence::generate() {
  llvm::Value *ret = nullptr;
  for (auto &statement : statements) {
    ret = statement->generate();
    if (dynamic_cast<ReturnStatement *>(statement.get())) {
      break;
    }
  }
  return ret;
}

std::vector<global_tuple> SymbolTable::globals_;

SymbolTable::SymbolTable() : tables(1) {}

void SymbolTable::add(const std::string &token, id_ptr id) {
  tables.back()[token] = std::move(id);
}

void SymbolTable::addGlobal(llvm::GlobalVariable *global, expr_ptr init,
                            TypeID type) {
  globals_.push_back({global, std::move(init), type});
}

const std::vector<global_tuple> &SymbolTable::globals() { return globals_; }

Identifier *SymbolTable::get(const std::string &token) const {
  for (auto i = tables.rbegin(); i < tables.rend(); ++i) {
    if (i->find(token) != i->end()) {
      return i->at(token).get();
    }
  }
  return nullptr;
}

void SymbolTable::push() { tables.emplace_back(); }

void SymbolTable::pop() { tables.pop_back(); }

llvm::AllocaInst *entryBlockAlloca(llvm::Function *func,
                                   const std::string &name, llvm::Type *type) {
  llvm::IRBuilder<> builder(&func->getEntryBlock(),
                            func->getEntryBlock().begin());
  return builder.CreateAlloca(type, 0, name.c_str());
}