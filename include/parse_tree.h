#ifndef PARSE_TREE_H
#define PARSE_TREE_H

#include "symbols.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

struct Expression;
struct Statement;
struct Identifier;
class SymbolTable;

using expr_ptr = std::unique_ptr<Expression>;
using stmt_ptr = std::unique_ptr<Statement>;
using id_ptr = std::unique_ptr<Identifier>;
using global_tuple = std::tuple<llvm::GlobalVariable *, expr_ptr, TypeID>;

struct CodeGenError : std::runtime_error {
public:
  CodeGenError(const std::string &err);
};

struct Node {
  static llvm::LLVMContext context;
  static llvm::IRBuilder<> builder;
  static std::unique_ptr<llvm::Module> module;
  static SymbolTable symbols;
  static llvm::StructType *complexStruct;
  static llvm::Type *intType, *doubleType, *boolType, *stringType;
  static llvm::Constant *TRUE, *FALSE, *MINUS_ONE_INT, *MINUS_ONE_DOUBLE,
      *INT_ZERO, *DOUBLE_ZERO, *COMPLEX_ZERO, *STRING_ZERO;

  Token token;

  Node(Token token_);
  virtual ~Node();

  virtual llvm::Value *generate() = 0;

  void error(const std::string &msg, int line);
  Identifier *getSymbol(const std::string &name);
  llvm::Type *getType(TypeID type);
  llvm::Type *getMaxType(llvm::Type *a, llvm::Type *b);
  llvm::Value *expand(llvm::Value *val, llvm::Type *to);

  static void initGlobals();
};

struct Expression : Node {
  Expression(Token token);
};

struct Identifier : Expression {
  TypeID type;
  llvm::Value *alloc;
  Identifier(Token id, TypeID type_, llvm::Value *alloc_ = nullptr);

  virtual llvm::Value *generate() override;
};

struct FunctionCall : Expression {
  std::vector<expr_ptr> arguments;
  FunctionCall(Token name, std::vector<expr_ptr> &args);

  llvm::Value *Re(llvm::Value *val);
  llvm::Value *Im(llvm::Value *val);

  virtual llvm::Value *generate() override;
};

struct AbsoluteValue : Expression {
  expr_ptr val_;
  AbsoluteValue(Token token, expr_ptr val);

  virtual llvm::Value *generate() override;
};

struct Complex : Expression {
  expr_ptr imaginary;
  Complex(expr_ptr imaginary_, Token token);

  virtual llvm::Value *generate() override;
  static llvm::GetElementPtrInst *re(llvm::Value *complex);
  static llvm::GetElementPtrInst *im(llvm::Value *complex);
  static llvm::Value *get(llvm::Value *real_, llvm::Value *im_);
  static std::pair<llvm::Value *, llvm::Value *>
  mul(llvm::Value *re1, llvm::Value *im1, llvm::Value *re2, llvm::Value *im2);
  static std::pair<llvm::Value *, llvm::Value *>
  getComponents(llvm::Value *complex);
};

struct Operation : Expression {
  Operation(Token token);
};

struct BinaryOperation : Operation {
  expr_ptr lhs, rhs;
  BinaryOperation(expr_ptr lhs_, Token operator_, expr_ptr rhs_);

  llvm::Value *multiplyComplex(llvm::Value *re1, llvm::Value *im1,
                               llvm::Value *re2, llvm::Value *im2);
  llvm::Value *divideComplex(llvm::Value *re1, llvm::Value *im1,
                             llvm::Value *re2, llvm::Value *im2);
  virtual llvm::Value *generate() override;
};

struct UnaryOperation : Operation {
  expr_ptr expression;
  UnaryOperation(Token operator_, expr_ptr expression_);

  virtual llvm::Value *generate() override;
};

struct Constant : Expression {
  TypeID type;
  Constant(Token token, TypeID type_);

  virtual llvm::Value *generate() override;
};

struct LogicalOperation : Expression {
  const expr_ptr lhs, rhs;
  LogicalOperation(expr_ptr lhs_, Token operator_, expr_ptr rhs_);
};

struct Disjunction : LogicalOperation {
  Disjunction(expr_ptr lhs_, Token operator_, expr_ptr rhs_);

  virtual llvm::Value *generate() override;
};

struct Conjunction : LogicalOperation {
  Conjunction(expr_ptr lhs_, Token operator_, expr_ptr rhs_);

  virtual llvm::Value *generate() override;
};

struct Negation : Expression {
  const expr_ptr expression;
  Negation(Token operator_, expr_ptr expression_);

  virtual llvm::Value *generate() override;
};

struct Relation : LogicalOperation {
  Relation(expr_ptr lhs_, Token operator_, expr_ptr rhs_);

  virtual llvm::Value *generate() override;
};

struct Statement : Node {
  Statement(Token token);
};

struct IfStatement : Statement {
  expr_ptr condition;
  stmt_ptr ifBlock, elseBlock;
  IfStatement(Token token, expr_ptr condition_, stmt_ptr ifBlock_,
              stmt_ptr elseBlock_);

  llvm::Value *generate() override;
};

struct WhileStatement : Statement {
  expr_ptr condition;
  stmt_ptr block;
  WhileStatement(Token token, expr_ptr condition_, stmt_ptr block_);

  llvm::Value *generate() override;
};

struct ReturnStatement : Statement {
  expr_ptr return_;
  ReturnStatement(Token token, expr_ptr return__);

  llvm::Value *generate() override;
};

struct Assignment : Statement {
  id_ptr identifier;
  expr_ptr expression;
  Assignment(id_ptr identifier_, expr_ptr expression_);

  virtual llvm::Value *generate() override;
};

struct VariableDefinition : Assignment {
  VariableDefinition(id_ptr identifier_, expr_ptr expression_);

  virtual llvm::Value *generate() override;
};

struct FunctionDeclaration : Statement {
  std::vector<id_ptr> parameters;
  TypeID returnType;
  FunctionDeclaration(Token id_, TypeID returnType_,
                      std::vector<id_ptr> &params);

  virtual llvm::Value *generate() override;
};

struct FunctionDefinition : FunctionDeclaration {
  stmt_ptr block;
  FunctionDefinition(Token id_, TypeID returnType, stmt_ptr block_,
                     std::vector<id_ptr> &params);

  virtual llvm::Value *generate() override;
};

struct Sequence : Statement {
  std::vector<stmt_ptr> statements;
  Sequence(Token token, std::vector<stmt_ptr> &statements_);

  virtual llvm::Value *generate() override;
};

class SymbolTable {
  using Table = std::unordered_map<std::string, id_ptr>;
  std::vector<Table> tables;
  static std::vector<global_tuple> globals_;

public:
  std::unique_ptr<SymbolTable> prev;
  SymbolTable();

  void add(const std::string &token, id_ptr id);
  Identifier *get(const std::string &token) const;
  void push();
  void pop();

  static void addGlobal(llvm::GlobalVariable *global, expr_ptr init,
                        TypeID type);
  static const std::vector<global_tuple> &globals();
};

llvm::AllocaInst *entryBlockAlloca(llvm::Function *func,
                                   const std::string &name, llvm::Type *type);

#endif