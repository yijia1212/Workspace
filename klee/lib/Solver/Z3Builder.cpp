#include "Z3Builder.h"

#include "klee/Expr.h"
#include "klee/Solver.h"
#include "SolverStats.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sstream>
#include <vector>
#include <math.h>
#include <iomanip>

using namespace z3;
using namespace klee;

Z3ArrayExprHash::~Z3ArrayExprHash(){
}

Z3Builder::~Z3Builder(){
  ///fix me: delete _arr_hash;
}

void Z3Builder::getInitialRead(const Array *os, const unsigned index, 
			       model &m, std::vector<unsigned char> &value){  
  float v;
  value.resize(4);
  
  try{
  expr res = m.get_const_interp(getInitialArray(os,index).decl());

  Z3_string s = Z3_get_numeral_decimal_string(*c, res, 50);
  std::stringstream sstm;
  sstm << os->name << index;
  std::cout << sstm.str() << ":" << s << std::endl;  
  char *stopString;
  v = strtof(s, &stopString);
  }catch(exception e){
    v = 0;
  }

  char *p = reinterpret_cast<char*>(&v);
  std::copy(p, p + sizeof(float), value.begin());
}

expr Z3Builder::getInitialArray(const Array *root, const unsigned index){
  assert(root);
  std::stringstream sstm;
  sstm << root->name << index;
  return c -> real_const((sstm.str()).c_str());
}

expr Z3Builder::getArrayForUpdate(const Array *root, 
				  const UpdateNode *un,
				  const unsigned index) {
  expr *un_expr;
  if (!un) {
    return getInitialArray(root, index);
  } else {
    // FIXME: This really needs to be non-recursive.
    bool hashed = _arr_hash.lookupUpdateNodeExpr(un, un_expr);
    if(!hashed){
      	un_expr = new expr(construct(un->value));
       _arr_hash.hashUpdateNodeExpr(un, un_expr);
    }
    return *un_expr;
  }
}

expr Z3Builder::constructBlockClause(const Array* var, const unsigned index, const std::vector<unsigned char> &val){
  unsigned offset = index * (var->range/8);
  float v = *((float*)&val[offset]);
  upper = nextafterf(v, v + 1.0f); // next floating-point value
  lower = nextafterf(v, v - 1.0f); // previous floating-point value
   
  std::ostringstream upperStream;
  upperStream << fixed << setprecision(9) << upper;
  string upper_s = upperStream.str();

  std::ostringstream lowerStream;
  lowerStream << fixed << setprecision(9) << lower;
  string lower_s = lowerStream.str();

  expr upper_e = c.real_val(upper_s.c_str());
  expr lower_e = c.real_val(lower_s.c_str());
   
  std::stringstream sstm;
  sstm << var->name << index;
  expr var_e =  c -> real_const((sstm.str()).c_str());
  
  return var_e < lower_e || var_e > upper_e;
}

expr Z3Builder::construct(ref<Expr> e){

  ++stats::queryConstructs;

  switch (e->getKind()){

  case Expr::Constant: {
    ConstantExpr *CE = cast<ConstantExpr>(e);
    if(CE->getWidth() == Expr::Bool){
      return CE->isTrue()?c->bool_val(true):c->bool_val(false);
    }
    std::string s;
    CE->toString(s, 10, 1);
    return c->real_val(s.c_str());
  }

  case Expr::NotOptimized: {
    NotOptimizedExpr *noe = cast<NotOptimizedExpr>(e);
    return construct(noe->src);
  }
  
  case Expr::Reorder:{
    ReorderExpr *re = cast<ReorderExpr>(e);
    return construct(re->src);
  }

  case Expr::Read: {
    ///We require re->index is constant
    ReadExpr *re = cast<ReadExpr>(e);
    assert(re && re->updates.root);
    if(ConstantExpr *CE=dyn_cast<ConstantExpr>(re->index)){
      uint64_t index = CE->getZExtValue();
      return getArrayForUpdate(re->updates.root, re->updates.head, index);
    }else{
      assert(0 && "non-constant index for array");
    }
  }

  case Expr::Select: {
    SelectExpr *se = cast<SelectExpr>(e);
    expr cond = construct(se->cond);
    expr tExpr = construct(se->trueExpr);
    expr fExpr = construct(se->falseExpr);
    return ite(cond, tExpr, fExpr);
  }

    //Arithmetic

  case Expr::Add: {
    AddExpr *ae = cast<AddExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left + right;
  }

  case Expr::Sub: {
    SubExpr *ae = cast<SubExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left - right;
  }

  case Expr::Mul: {
    MulExpr *ae = cast<MulExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left * right;
  }

  case Expr::UDiv: {
  }

  case Expr::SDiv: {
  }

  case Expr::URem: {
  }

  case Expr::SRem: {
  }

  case Expr::FAdd: {
    FAddExpr *ae = cast<FAddExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left + right;
  }

  case Expr::FSub: {
    FSubExpr *ae = cast<FSubExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left - right;
  }

  case Expr::FMul: {
    FMulExpr *ae = cast<FMulExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left * right;
  }
  case Expr::FDiv: {
    FDivExpr *ae = cast<FDivExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left / right;
  }

  case Expr::FRem: {
  }
    //logic
  case Expr::Not: {
    NotExpr *ne = cast<NotExpr>(e);
    expr e = construct(ne->expr);
    return !e;
  }
    
  case Expr::And: {
    AndExpr *ae = cast<AndExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left && right;
  }

  case Expr::Or: {
    OrExpr *ae = cast<OrExpr>(e);
    expr left = construct(ae->left);
    expr right = construct(ae->right);
    return left || right;
  }

  case Expr::Xor: {
  }

    //comparison
  case Expr::Eq: {
    EqExpr *ee = cast<EqExpr>(e);
    expr left = construct(ee->left);
    expr right = construct(ee->right);
    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(ee->left)){
      if(CE->isTrue())
	return right;
      if(CE->isFalse())
	return !right;
    }
    return left == right;
  }

  case Expr::Ult: {
  }

  case Expr::Ule: {
  }

  case Expr::Slt: {
  }

  case Expr::Sle: {
  }
  
  case Expr::FOlt: {
    FOltExpr *ee = cast<FOltExpr>(e);
    expr left = construct(ee->left);
    expr right = construct(ee->right);
    return left < right;
  }

#if 0
  case Expr::Ne:
  case Expr::Ugt:
  case Expr::Uge:
  case Expr::Sgt:
  case Expr::Sge:
  case Expr::FOgt;
#endif

  default:
    assert(0 && "unhandled Expr type");
    return c->bool_val(true);
  }
}
