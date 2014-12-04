//===-- ReExprEvaluator.cpp ----------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/util/ReExprEvaluator.h"
#include "klee/Reorder.h"
#include "llvm/ADT/APFloat.h"

#include <iostream>
#include <iomanip> 
#include <cstdlib> 
#include <math.h>

using namespace klee;
using namespace std;

/***/

void ReExprEvaluator::evalRead(const ReadExpr *e, vector<ref<Expr> > &res){
  if(ConstantExpr *CE = dyn_cast<ConstantExpr>(e->index)){
    evalUpdate(e->updates, CE->getZExtValue(), res);
  }else{
    assert(0 && "encounter non constant index update");
  }
}

void ReExprEvaluator::evalUpdate(const UpdateList &ul,
				   unsigned index, 
				   vector<ref<Expr> > &res) {
  for (const UpdateNode *un=ul.head; un; un=un->next) {    
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(un->index)) {
      if (CE->getZExtValue() == index){
	evaluate(un->value, res);
	return;
      } else {
	// update index is unknown, so may or may not be index, we
	// cannot guarantee value. we can rewrite to read at this
	// version though (mostly for debugging).
	assert(0 && "update index is unknown");
      }
    }
  }
  //  if (ul.root->isConstantArray() && index < ul.root->size)
  //    return Action::changeTo(ul.root->constantValues[index]);

  getInitialValue(*ul.root, index, res);
}

void ReExprEvaluator::evalReorder(const ReorderExpr *e, ReExprRes &res){
  //in this function we call the min/max method 
  //Fix me: need to add type support, currently we assume float
  //Fix me: need to add round mode support
  float max = 0.0f, min = 0.0f, fmaMax = 0.0f, fmaMin = 0.0f;
  vector<float> resVal;
  vector<float> ops;
  vector<float> opl;
  vector<float> opr;
  int len = (e->operands).size();

  Reorder ro(FE_TONEAREST);

  vector<ref<Expr> > kids;
  vector<ref<Expr> > tmp;

  srand(time(0));
  int random;

  for(int i = 0; i < len; i ++){
    evaluate((e->operands)[i], tmp);
    kids.push_back(tmp[0]);
    tmp.clear();
  }
  
  if(e->cat == Expr::RE_FMA){
    for(int i = 0; i < len; i = i + 2){
      float x,y;
      if(ConstantExpr *CE = dyn_cast<ConstantExpr>(kids[i])){        
	llvm::APFloat v = CE->getAPFValue();
	x = v.convertToFloat();
	opl.push_back(x);
      }else{
	assert(0 && "encounter non-constant in Reorder Rebuild");
      }

      if(ConstantExpr *CE = dyn_cast<ConstantExpr>(kids[i+1])){
	llvm::APFloat v = CE->getAPFValue();
	y = v.convertToFloat();
	opr.push_back(y);
      }else{
	assert(0 && "encounter non-constant in Reorder Rebuild");
      }
      
      ops.push_back(x*y);     
    }
  } else {
    for(int i = 0; i < len; i ++){
      if(ConstantExpr *CE = dyn_cast<ConstantExpr>(kids[i])){
	llvm::APFloat v = CE->getAPFValue();
	ops.push_back(v.convertToFloat());
      }else{
	assert(0 && "encounter non-constant in Reorder Rebuild");
      }
    }
  }

  switch(e->cat){
  case Expr::RE_Plus:{
    max = ro.getPlusMax(ops);
    min = ro.getPlusMin(ops);
    break;
  }

  case Expr::RE_Mult:{
    max = ro.getMultMax(ops);
    min = ro.getMultMin(ops);
    break;
  }

  case Expr::RE_FMA:{
    fmaMax = ro.getFMAMax(opl, opr);
    fmaMin = ro.getFMAMin(opl, opr);
    llvm::APFloat apFmaMax(fmaMax), apFmaMin(fmaMin);
    resVal.push_back(ConstantExpr::alloc(apFmaMin));
    resVal.push_back(ConstantExpr::alloc(apFmaMax));   
    max = ro.getPlusMax(ops);
    min = ro.getPlusMin(ops);
    break;
  }
  default:
    assert(0 && "unsupported reorderable expression");
  }

  llvm::APFloat apMax(max), apMin(min);
  resVal.push_back(ConstantExpr::alloc(apMin));
  resVal.push_back(ConstantExpr::alloc(apMax));
  return;
}

void ReExprEvaluator::evalFOlt(const FOltExpr *e, vector<ref<Expr> > &res){
  //Fix me:: handle e-> left is constant
  vector<ref<Expr> > kidEvalRes;
  ref<ConstantExpr> minDist;
  ref<ConstantExpr> minValue;
  if(ConstantExpr *CER = dyn_cast<ConstantExpr>(e->getKid(0))){
    evaluate(e->getKid(1), kidEvalRes);
    ref<Expr> tmp[2];
    tmp[0] = e->getKid(0);
    for(int i = 0; i < kidEvalRes.size(); i ++){
      if(ConstantExpr *CEL = dyn_cast<ConstantExpr>(kidEvalRes[i])){
	ref<ConstantExpr> value = CER->FSub(CEL);
	ref<ConstantExpr> dist = CER->FAbs(CEL);
	if(minValue.get()){
	  if((minDist -> FOgt(dist)) -> isTrue()){
	    minValue = value;
	    minDist = dist;
	  }
	}else{
	  minValue = value;
	  minDist = dist;
	}
      }else{
	assert(0 && "encounter non-constantExpr after evaluate");
      }
      tmp[1] = kidEvalRes[i];
      res.push_back(e->rebuild(tmp));
    }
  }else if(ConstantExpr *CE = dyn_cast<ConstantExpr>(e->getKid(1))){
    evaluate(e->getKid(0), kidEvalRes);
    ref<Expr> tmp[2];
    tmp[1] = e->getKid(1);
    for(int i = 0; i < kidEvalRes.size(); i ++){
      if(ConstantExpr *CEL = dyn_cast<ConstantExpr>(kidEvalRes[i])){
	ref<ConstantExpr> value = CE->FSub(CEL);
	ref<ConstantExpr> dist = CE->FAbs(CEL);
        if(minValue.get()){
	  if((minDist -> FOgt(dist)) -> isTrue()){
	    minValue = value;
	    minDist = dist;
	  }
	}else{
	  minValue = value;
	  minDist = dist;
	}
      }else{
	assert(0 && "encounter non-constantExpr after evaluate");
      }
      tmp[0] = kidEvalRes[i];
      res.push_back(e->rebuild(tmp));
    }
  }else{
    assert(0 && "we need one side of evalFOlt is constant");
  }
  epsilon = minValue;
} 

void ReExprEvaluator::evaluate(const ref<Expr> &e, vector<ref<Expr> > &res){
  if(isa<ConstantExpr>(e)){
    res.push_back(e);
    return;
  } else {
    switch(e->getKind()){
    case Expr::Reorder:{
      evalReorder(dyn_cast<ReorderExpr>(e), res);
      break;
    }

    case Expr::Read:{
      evalRead(dyn_cast<ReadExpr>(e), res);
      break;
    }

    case Expr::FOlt:{
      evalFOlt(dyn_cast<FOltExpr>(e), res);
      break;
    }

    case Expr::InvalidKind:{
      assert(0 && "evaluate invalid expr");
    }
    default: {
      vector<vector<ref<Expr> > > kids;
      unsigned count = e->getNumKids();
      kids.resize(count);
      assert(count < 3); //Fix me: need to handle select
      for(unsigned i = 0; i < count; i ++){
	ref<Expr> kid = e->getKid(i);
        evaluate(kid, kids[i]);
      }
      
      if(count == 1){
	for(unsigned i = 0; i < kids[0].size(); i ++){
	  res.push_back(kids[0][i]);
	}
      } else { // for count == 2
	ref<Expr> tmp[2];
        for(unsigned i = 0; i < kids[0].size(); i ++){	    
	  tmp[0] = kids[0][i];
	  for(unsigned j = 0; j < kids[1].size(); j ++){
	    tmp[1] = kids[1][j];
	    res.push_back(e->rebuild(tmp));
	  }
	}
      }
      return;
    }      
    }
  }
}

bool ReExprEvaluator::isAssignmentStable(const ref<Expr> &e, ref<Expr> &eps){
  bool trueRe = false, falseRe = false;
  vector<ref<Expr> > res;
  evaluate(e,res);
  vector<ref<Expr> >::iterator ite = res.begin();
  
  std::cout << "res.size: " << res.size() << std::endl;
  for(; ite != res.end(); ite++){
    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(*ite)){
      if(CE->isTrue()){
	trueRe = true;
      }else if(CE->isFalse()){
	falseRe = true;
      }
    }else{
      assert(0 && "encounter non-constant result in ReExprEvaluator");
    }
  }
  eps = epsilon;
  return trueRe && falseRe;
}
