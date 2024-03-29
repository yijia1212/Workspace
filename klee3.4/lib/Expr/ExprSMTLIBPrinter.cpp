//===-- ExprSMTLIBPrinter.cpp -----------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "klee/util/ExprSMTLIBPrinter.h"

#include <stack>

namespace ExprSMTLIBOptions {
// Command line options
llvm::cl::opt<klee::ExprSMTLIBPrinter::ConstantDisplayMode>
argConstantDisplayMode(
    "smtlib-display-constants",
    llvm::cl::desc("Sets how bitvector constants are written in generated "
                   "SMT-LIBv2 files (default=dec)"),
    llvm::cl::values(clEnumValN(klee::ExprSMTLIBPrinter::BINARY, "bin",
                                "Use binary form (e.g. #b00101101)"),
                     clEnumValN(klee::ExprSMTLIBPrinter::HEX, "hex",
                                "Use Hexadecimal form (e.g. #x2D)"),
                     clEnumValN(klee::ExprSMTLIBPrinter::DECIMAL, "dec",
                                "Use decimal form (e.g. (_ bv45 8) )"),
                     clEnumValEnd),
    llvm::cl::init(klee::ExprSMTLIBPrinter::DECIMAL));

llvm::cl::opt<bool> humanReadableSMTLIB(
    "smtlib-human-readable",
    llvm::cl::desc(
        "Enables generated SMT-LIBv2 files to be human readable (default=off)"),
    llvm::cl::init(false));

llvm::cl::opt<klee::ExprSMTLIBPrinter::AbbreviationMode> abbreviationMode(
    "smtlib-abbreviation-mode",
    llvm::cl::desc(
        "Choose abbreviation mode to use in SMT-LIBv2 files (default=let)"),
    llvm::cl::values(clEnumValN(klee::ExprSMTLIBPrinter::ABBR_NONE, "none",
                                "Do not abbreviate"),
                     clEnumValN(klee::ExprSMTLIBPrinter::ABBR_LET, "let",
                                "Abbreviate with let"),
                     clEnumValN(klee::ExprSMTLIBPrinter::ABBR_NAMED, "named",
                                "Abbreviate with :named annotations"),
                     clEnumValEnd),
    llvm::cl::init(klee::ExprSMTLIBPrinter::ABBR_LET));
}

namespace klee {

ExprSMTLIBPrinter::ExprSMTLIBPrinter()
    : usedArrays(), o(NULL), query(NULL), p(NULL), haveConstantArray(false),
      logicToUse(AUFNIRA),
      humanReadable(ExprSMTLIBOptions::humanReadableSMTLIB),
      smtlibBoolOptions(), arraysToCallGetValueOn(NULL) {
  setConstantDisplayMode(ExprSMTLIBPrinter::DECIMAL);
  setAbbreviationMode(ExprSMTLIBOptions::abbreviationMode);
}

ExprSMTLIBPrinter::~ExprSMTLIBPrinter() {
  if (p != NULL)
    delete p;
}

void ExprSMTLIBPrinter::setOutput(llvm::raw_ostream &output) {
  o = &output;
  if (p != NULL)
    delete p;

  p = new PrintContext(output);
}

void ExprSMTLIBPrinter::setQuery(const Query &q) {
  query = &q;
  reset(); // clear the data structures
  scanAll();
}

void ExprSMTLIBPrinter::reset() {
  bindings.clear();
  orderedBindings.clear();
  seenExprs.clear();
  usedArrays.clear();
  haveConstantArray = false;

  /* Clear the PRODUCE_MODELS option if it was automatically set.
   * We need to do this because the next query might not need the
   * (get-value) SMT-LIBv2 command.
   */
  if (arraysToCallGetValueOn != NULL)
    setSMTLIBboolOption(PRODUCE_MODELS, OPTION_DEFAULT);

  arraysToCallGetValueOn = NULL;
}

bool ExprSMTLIBPrinter::isHumanReadable() { return humanReadable; }

bool ExprSMTLIBPrinter::setConstantDisplayMode(ConstantDisplayMode cdm) {
  if (cdm > DECIMAL)
    return false;

  this->cdm = cdm;
  return true;
}

void ExprSMTLIBPrinter::printConstant(const ref<ConstantExpr> &e) {
  /* Handle simple boolean constants */

  if (e->isTrue()) {
    *p << "true";
    return;
  }

  if (e->isFalse()) {
    *p << "false";
    return;
  }

  /* Handle bitvector constants */

  std::string value;

  /* SMTLIBv2 deduces the bit-width (should be 8-bits in our case)
   * from the length of the string (e.g. zero is #b00000000). LLVM
   * doesn't know about this so we need to pad the printed output
   * with the appropriate number of zeros (zeroPad)
   */
  unsigned int zeroPad = 0;

  switch (cdm) {
  case BINARY:
    e->toString(value, 2);
    *p << "#b";

    zeroPad = e->getWidth() - value.length();

    for (unsigned int count = 0; count < zeroPad; count++)
      *p << "0";

    *p << value;
    break;

  case HEX:
    e->toString(value, 16);
    *p << "#x";

    zeroPad = (e->getWidth() / 4) - value.length();
    for (unsigned int count = 0; count < zeroPad; count++)
      *p << "0";

    *p << value;
    break;

  case DECIMAL:
    switch (logicToUse){
    case QF_ABV:
    case QF_AUFBV:
      e->toString(value, 10);
      *p << "(_ bv" << value << " " << e->getWidth() << ")";
      break;
    case AUFNIRA:
      e->toString(value, 10, 1);
      *p << value;
      break;
    default:
      llvm::errs() << "ExprSMTLIBPrinter::printConstant() : Unexpected Logic ";
    }
    break;

  default:
    llvm_unreachable("Unexpected constant display mode");
  }
}

void ExprSMTLIBPrinter::printExpression(
    const ref<Expr> &e, ExprSMTLIBPrinter::SMTLIB_SORT expectedSort) {
  // check if casting might be necessary
  //  if (getSort(e) != expectedSort) {
  //printCastToSort(e, expectedSort);
  //return;
  //  }

  switch (e->getKind()) {
  case Expr::Constant:
    printConstant(cast<ConstantExpr>(e));
    return; // base case

  case Expr::NotOptimized:
    // skip to child
    printExpression(e->getKid(0), expectedSort);
    return;

  case Expr::Reorder:
    //skip to child
    printExpression(e->getKid(0), expectedSort);
    return;

  case Expr::Read:
    printReadExpr(cast<ReadExpr>(e));
    return;

  case Expr::Extract:
    printExtractExpr(cast<ExtractExpr>(e));
    return;

  case Expr::SExt:
  case Expr::ZExt:
    printCastExpr(cast<CastExpr>(e));
    return;

  case Expr::Ne:
    printNotEqualExpr(cast<NeExpr>(e));
    return;

  case Expr::Select:
    // the if-then-else expression.
    printSelectExpr(cast<SelectExpr>(e), expectedSort);
    return;

  case Expr::Eq:
    /* The "=" operator is special in that it can take any sort but we must
     * enforce that both arguments are the same type. We do this a lazy way
     * by enforcing the second argument is of the same type as the first.
     */
    printSortArgsExpr(e, getSort(e->getKid(0)));

    return;

  case Expr::And:
  case Expr::Or:
  case Expr::Xor:
  case Expr::Not:
    /* These operators have a bitvector version and a bool version.
     * For these operators only (e.g. wouldn't apply to bvult) if the expected
     * sort the
     * expression is T then that implies the arguments are also of type T.
     */
    printLogicalOrBitVectorExpr(e, expectedSort);

    return;

  default:
    /* The remaining operators (Add,Sub...,Ult,Ule,..)
     * Expect SORT_BITVECTOR arguments
     */
    printSortArgsExpr(e, SORT_BITVECTOR);
    return;
  }
}

void ExprSMTLIBPrinter::printReadExpr(const ref<ReadExpr> &e) {
  *p << "(" << " ";
  p->pushIndent();

  printSeperator();

  // print array with updates recursively
  printUpdatesAndArray(e->updates.head, e->updates.root, e->index);
  p->popIndent();
  // print index
  printSeperator();
  *p << ")";
}

void ExprSMTLIBPrinter::printExtractExpr(const ref<ExtractExpr> &e) {
  unsigned int lowIndex = e->offset;
  unsigned int highIndex = lowIndex + e->width - 1;

  *p << "((_ " << getSMTLIBKeyword(e) << " " << highIndex << "  " << lowIndex
     << ") ";

  p->pushIndent(); // add indent for recursive call
  printSeperator();

  // recurse
  printExpression(e->getKid(0), SORT_BITVECTOR);

  p->popIndent(); // pop indent added for the recursive call
  printSeperator();
  *p << ")";
}

void ExprSMTLIBPrinter::printCastExpr(const ref<CastExpr> &e) {
  /* sign_extend and zero_extend behave slightly unusually in SMTLIBv2
   * instead of specifying of what bit-width we would like to extend to
   * we specify how many bits to add to the child expression
   *
   * e.g
   * ((_ sign_extend 64) (_ bv5 32))
   *
   * gives a (_ BitVec 96) instead of (_ BitVec 64)
   *
   * So we must work out how many bits we need to add.
   *
   * (e->width) is the desired number of bits
   * (e->src->getWidth()) is the number of bits in the child
   */
  unsigned int numExtraBits = (e->width) - (e->src->getWidth());

  *p << "((_ " << getSMTLIBKeyword(e) << " " << numExtraBits << ") ";

  p->pushIndent(); // add indent for recursive call
  printSeperator();

  // recurse
  printExpression(e->src, SORT_BITVECTOR);

  p->popIndent(); // pop indent added for recursive call
  printSeperator();

  *p << ")";
}

void ExprSMTLIBPrinter::printNotEqualExpr(const ref<NeExpr> &e) {
  *p << "(not (";
  p->pushIndent();
  *p << "="
     << " ";
  p->pushIndent();
  printSeperator();

  /* The "=" operators allows both sorts. We assume
   * that the second argument sort should be forced to be the same sort as the
   * first argument
   */
  SMTLIB_SORT s = getSort(e->getKid(0));

  printExpression(e->getKid(0), s);
  printSeperator();
  printExpression(e->getKid(1), s);
  p->popIndent();
  printSeperator();

  *p << ")";
  p->popIndent();
  printSeperator();
  *p << ")";
}

const char *ExprSMTLIBPrinter::getSMTLIBKeyword(const ref<Expr> &e) {

  switch (e->getKind()) {
  case Expr::Read:
    return "select";
  case Expr::Select:
    return "ite";
  case Expr::Concat:
    return "concat";
  case Expr::Extract:
    return "extract";
  case Expr::ZExt:
    return "zero_extend";
  case Expr::SExt:
    return "sign_extend";

  case Expr::Add:
    if(logicToUse == AUFNIRA){
      return "+";
    }
    return "bvadd";
  case Expr::FAdd:
    return "+";
  case Expr::Sub:
    if(logicToUse == AUFNIRA){
      return "-";
    }
    return "bvsub";
  case Expr::FSub:
    return "-";
  case Expr::Mul:
    if(logicToUse == AUFNIRA){
      return "*";
    }
    return "bvmul";
  case Expr::FMul:
    return "*";
  case Expr::UDiv:
    if(logicToUse == AUFNIRA){
      return "/";
    }
    return "bvudiv";
  case Expr::SDiv:
    if(logicToUse == AUFNIRA){
      return "/";
    }
    return "bvsdiv";
  case Expr::FDiv:
    return "/";
  case Expr::URem:
    if(logicToUse == AUFNIRA){
      return "%";
    }
    return "bvurem";
  case Expr::SRem:
    if(logicToUse == AUFNIRA){
      return "%";
    }
    return "bvsrem";
  case Expr::FRem:
    return "%";

  /* And, Xor, Not and Or are not handled here because there different versions
   * for different sorts. See printLogicalOrBitVectorExpr()
   */

  case Expr::Shl:
    return "bvshl";
  case Expr::LShr:
    return "bvlshr";
  case Expr::AShr:
    return "bvashr";

  case Expr::Eq:
    return "=";
 
  case Expr::FUeq:
    return "=";

  case Expr::FUne:
    return "!=";

  // Not Equal does not exist directly in SMTLIBv2

  case Expr::Ult:
    if(logicToUse == AUFNIRA){
      return "<";
    }
    return "bvult";
  case Expr::Ule:
    if(logicToUse == AUFNIRA){
      return "<=";
    }
    return "bvule";
  case Expr::Ugt:
    if(logicToUse == AUFNIRA){
      return ">";
    }
    return "bvugt";
  case Expr::Uge:
    if(logicToUse == AUFNIRA){
      return ">=";
    }
    return "bvuge";

  case Expr::Slt:
    if(logicToUse == AUFNIRA){
      return "<";
    }
    return "bvslt";
  case Expr::Sle:
    if(logicToUse == AUFNIRA){
      return "<=";
    }
    return "bvsle";
  case Expr::Sgt:
    if(logicToUse == AUFNIRA){
      return ">";
    }
    return "bvsgt";
  case Expr::Sge:
    if(logicToUse == AUFNIRA){
      return ">=";
    }
    return "bvsge";

  case Expr::FOgt:
    return ">";

  case Expr::FOlt:
    return "<";

  case Expr::FOle:
    return "<=";

  default:
    llvm_unreachable("Conversion from Expr to SMTLIB keyword failed");
  }
}

void ExprSMTLIBPrinter::printUpdatesAndArray(const UpdateNode *un,
                                             const Array *root,
					     const ref<Expr> index) {
  ConstantExpr *CE = cast<ConstantExpr>(index);

  if (un != NULL) {
    *p << "(";
    printExpression(un->value, SORT_BITVECTOR);
    p->popIndent();
    *p << ")";
  } else {
    // The base case of the recursion
    std::string res;
    CE->toString(res, 10, 1);
    *p << root->name + res;
  }
}

void ExprSMTLIBPrinter::scanAll() {
  // perform scan of all expressions
  for (ConstraintManager::const_iterator i = query->constraints.begin();
       i != query->constraints.end(); i++)
    scan(*i);

  // Scan the query too
  scan(query->expr);
}

void ExprSMTLIBPrinter::generateOutput() {
  if (p == NULL || query == NULL || o == NULL) {
    llvm::errs() << "ExprSMTLIBPrinter::generateOutput() Can't print SMTLIBv2. "
                    "Output or query bad!\n";
    return;
  }

  if (humanReadable)
    printNotice();
  printOptions();
  printSetLogic();
  printArrayDeclarations();
  printConstraints();
  printQuery();
  printAction();
  printExit();
}

void ExprSMTLIBPrinter::printSetLogic() {
  *o << "(set-logic ";
  switch (logicToUse) {
  case QF_ABV:
    *o << "QF_ABV";
    break;
  case QF_AUFBV:
    *o << "QF_AUFBV";
    break;
  case AUFNIRA:
    *o << "AUFNIRA";
    break;
  }
  *o << " )\n";
}

namespace {

struct ArrayPtrsByName {
  bool operator()(const Array *a1, const Array *a2) const {
    return a1->name < a2->name;
  }
};

}

void ExprSMTLIBPrinter::printArrayDeclarations() {
  // Assume scan() has been called
  if (humanReadable)
    *o << "; Array declarations\n";

  // Declare arrays in a deterministic order.
  std::vector<const Array *> sortedArrays(usedArrays.begin(), usedArrays.end());
  std::sort(sortedArrays.begin(), sortedArrays.end(), ArrayPtrsByName());
  for (std::vector<const Array *>::iterator it = sortedArrays.begin();
       it != sortedArrays.end(); it++) {
    if((*it) -> getDomain() != Expr::InvalidWidth){
      if(logicToUse == AUFNIRA){
	*o << "(declare-fun " << (*it)->name << " () "
	  "(Array Int Real)) "
	   << "\n";
      }else{
	*o << "(declare-fun " << (*it)->name << " () "
	  "(Array (_ BitVec "
	   << (*it)->getDomain() << ") "
	  "(_ BitVec " << (*it)->getRange() << ") ) )"
	   << "\n";
      }
    }else{
      *o << "(declare-fun " << (*it)-> name << " () Real)"
         << "\n";
    }
  }

  // Set array values for constant values
  if (haveConstantArray) {
    if (humanReadable)
      *o << "; Constant Array Definitions\n";

    const Array *array;

    // loop over found arrays
    for (std::vector<const Array *>::iterator it = sortedArrays.begin();
         it != sortedArrays.end(); it++) {
      array = *it;
      int byteIndex = 0;
      if (array->isConstantArray()) {
        /*loop over elements in the array and generate an assert statement
          for each one
         */
        for (std::vector<ref<ConstantExpr> >::const_iterator
                 ce = array->constantValues.begin();
             ce != array->constantValues.end(); ce++, byteIndex++) {
          *p << "(assert (";
          p->pushIndent();
          *p << "= ";
          p->pushIndent();
          printSeperator();

          *p << "(select " << array->name << " (_ bv" << byteIndex << " "
             << array->getDomain() << ") )";
          printSeperator();
          printConstant((*ce));

          p->popIndent();
          printSeperator();
          *p << ")";
          p->popIndent();
          printSeperator();
          *p << ")";

          p->breakLineI();
        }
      }
    }
  }
}

void ExprSMTLIBPrinter::printConstraints() {
  if (humanReadable)
    *o << "; Constraints\n";

  // Generate assert statements for each constraint
  for (ConstraintManager::const_iterator i = query->constraints.begin();
       i != query->constraints.end(); i++) {
    *p << "(assert ";
    p->pushIndent();
    printSeperator();

    // recurse into Expression
    printExpression(*i, SORT_BOOL);

    p->popIndent();
    printSeperator();
    *p << ")";
    p->breakLineI();
  }
}

void ExprSMTLIBPrinter::printAction() {
  // Ask solver to check for satisfiability
  *o << "(check-sat)\n";

  /* If we has arrays to find the values of then we'll
   * ask the solver for the value of each bitvector in each array
   */
  if (arraysToCallGetValueOn != NULL && !arraysToCallGetValueOn->empty()) {

    const Array *theArray = 0;

    // loop over the array names
    for (std::vector<const Array *>::const_iterator it =
             arraysToCallGetValueOn->begin();
         it != arraysToCallGetValueOn->end(); it++) {
      theArray = *it;
      // Loop over the array indices
      for (unsigned int index = 0; index < theArray->size; ++index) {
        *o << "(get-value ( (select " << (**it).name << " (_ bv" << index << " "
           << theArray->getDomain() << ") ) ) )\n";
      }
    }
  }
}

void ExprSMTLIBPrinter::scan(const ref<Expr> &e) {
  assert(!(e.isNull()) && "found NULL expression");

  if (isa<ConstantExpr>(e))
    return; // we don't need to scan simple constants

  if (const ReadExpr *re = dyn_cast<ReadExpr>(e)) {

    // Attempt to insert array and if array wasn't present before do more things
    if (usedArrays.insert(re->updates.root).second) {

      // check if the array is constant
      if (re->updates.root->isConstantArray())
        haveConstantArray = true;

      // scan the update list
      scanUpdates(re->updates.head);
    }
  }

  // recurse into the children
  Expr *ep = e.get();
  for (unsigned int i = 0; i < ep->getNumKids(); i++)
    scan(ep->getKid(i));
}

void ExprSMTLIBPrinter::scanUpdates(const UpdateNode *un) {
  while (un != NULL) {
    //scan(un->index);
    scan(un->value);
    un = un->next;
  }
}

void ExprSMTLIBPrinter::printExit() { *o << "(exit)\n"; }

bool ExprSMTLIBPrinter::setLogic(SMTLIBv2Logic l) {
  if (l > AUFNIRA)
    return false;

  logicToUse = l;
  return true;
}

void ExprSMTLIBPrinter::printSeperator() {
  if (humanReadable)
    p->breakLineI();
  else
    p->write(" ");
}

void ExprSMTLIBPrinter::printNotice() {
  *o << "; This file conforms to SMTLIBv2 and was generated by KLEE\n";
}

void ExprSMTLIBPrinter::setHumanReadable(bool hr) { humanReadable = hr; }

void ExprSMTLIBPrinter::printOptions() {
  // Print out SMTLIBv2 boolean options
  for (std::map<SMTLIBboolOptions, bool>::const_iterator i =
           smtlibBoolOptions.begin();
       i != smtlibBoolOptions.end(); i++) {
    *o << "(set-option :" << getSMTLIBOptionString(i->first) << " "
       << ((i->second) ? "true" : "false") << ")\n";
  }
}

void ExprSMTLIBPrinter::printQuery() {
  if (humanReadable) {
    *p << "; Query from solver turned into an assert";
    p->breakLineI();
  }

  p->pushIndent();
  *p << "(assert";
  p->pushIndent();
  printSeperator();

  printExpression(queryAssert, SORT_BOOL);

  p->popIndent();
  printSeperator();
  *p << ")";
  p->popIndent();
  p->breakLineI();
}

ExprSMTLIBPrinter::SMTLIB_SORT ExprSMTLIBPrinter::getSort(const ref<Expr> &e) {
  switch (e->getKind()) {
  case Expr::NotOptimized:
    return getSort(e->getKid(0));

  // The relational operators are bools.
  case Expr::Eq:
  case Expr::Ne:
  case Expr::Slt:
  case Expr::Sle:
  case Expr::Sgt:
  case Expr::Sge:
  case Expr::Ult:
  case Expr::Ule:
  case Expr::Ugt:
  case Expr::Uge:
    return SORT_BOOL;

  // These may be bitvectors or bools depending on their width (see
  // printConstant and printLogicalOrBitVectorExpr).
  case Expr::Constant:
  case Expr::And:
  case Expr::Not:
  case Expr::Or:
  case Expr::Xor:
    return e->getWidth() == Expr::Bool ? SORT_BOOL : SORT_BITVECTOR;

  // Everything else is a bitvector.
  default:
    return SORT_BITVECTOR;
  }
}

void ExprSMTLIBPrinter::printCastToSort(const ref<Expr> &e,
                                        ExprSMTLIBPrinter::SMTLIB_SORT sort) {
  switch (sort) {
  case SORT_BITVECTOR:
    if (humanReadable) {
      p->breakLineI();
      *p << ";Performing implicit bool to bitvector cast";
      p->breakLine();
    }
    // We assume the e is a bool that we need to cast to a bitvector sort.
    *p << "(ite";
    p->pushIndent();
    printSeperator();
    printExpression(e, SORT_BOOL);
    printSeperator();
    *p << "(_ bv1 1)";
    printSeperator(); // printing the "true" bitvector
    *p << "(_ bv0 1)";
    p->popIndent();
    printSeperator(); // printing the "false" bitvector
    *p << ")";
    break;
  case SORT_BOOL: {
    /* We make the assumption (might be wrong) that any bitvector whos unsigned
     *decimal value is
     * is zero is interpreted as "false", otherwise it is true.
     *
     * This may not be the interpretation we actually want!
     */
    Expr::Width bitWidth = e->getWidth();
    if (humanReadable) {
      p->breakLineI();
      *p << ";Performing implicit bitvector to bool cast";
      p->breakLine();
    }
    *p << "(bvugt";
    p->pushIndent();
    printSeperator();
    // We assume is e is a bitvector
    printExpression(e, SORT_BITVECTOR);
    printSeperator();
    *p << "(_ bv0 " << bitWidth << ")";
    p->popIndent();
    printSeperator(); // Zero bitvector of required width
    *p << ")";

    if (bitWidth != Expr::Bool)
      llvm::errs()
          << "ExprSMTLIBPrinter : Warning. Casting a bitvector (length "
          << bitWidth << ") to bool!\n";

  } break;
  default:
    llvm_unreachable("Unsupported cast");
  }
}

void ExprSMTLIBPrinter::printSelectExpr(const ref<SelectExpr> &e,
                                        ExprSMTLIBPrinter::SMTLIB_SORT s) {
  // This is the if-then-else expression

  *p << "(" << getSMTLIBKeyword(e) << " ";
  p->pushIndent(); // add indent for recursive call

  // The condition
  printSeperator();
  printExpression(e->getKid(0), SORT_BOOL);

  /* This operator is special in that the remaining children
   * can be of any sort.
   */

  // if true
  printSeperator();
  printExpression(e->getKid(1), s);

  // if false
  printSeperator();
  printExpression(e->getKid(2), s);

  p->popIndent(); // pop indent added for recursive call
  printSeperator();
  *p << ")";
}

void ExprSMTLIBPrinter::printSortArgsExpr(const ref<Expr> &e,
                                          ExprSMTLIBPrinter::SMTLIB_SORT s) {
  *p << "(" << getSMTLIBKeyword(e) << " ";
  p->pushIndent(); // add indent for recursive call

  // loop over children and recurse into each expecting they are of sort "s"
  for (unsigned int i = 0; i < e->getNumKids(); i++) {
    printSeperator();
    printExpression(e->getKid(i), s);
  }

  p->popIndent(); // pop indent added for recursive call
  printSeperator();
  *p << ")";
}

void ExprSMTLIBPrinter::printLogicalOrBitVectorExpr(
    const ref<Expr> &e, ExprSMTLIBPrinter::SMTLIB_SORT s) {
  /* For these operators it is the case that the expected sort is the same as
   * the sorts
   * of the arguments.
   */

  *p << "(";
  switch (e->getKind()) {
  case Expr::And:
    *p << ((s == SORT_BITVECTOR) ? "bvand" : "and");
    break;
  case Expr::Not:
    *p << ((s == SORT_BITVECTOR) ? "bvnot" : "not");
    break;
  case Expr::Or:
    *p << ((s == SORT_BITVECTOR) ? "bvor" : "or");
    break;

  case Expr::Xor:
    *p << ((s == SORT_BITVECTOR) ? "bvxor" : "xor");
    break;
  default:
    llvm_unreachable("Conversion from Expr to SMTLIB keyword failed");
  }
  *p << " ";

  p->pushIndent(); // add indent for recursive call

  // loop over children and recurse into each expecting they are of sort "s"
  for (unsigned int i = 0; i < e->getNumKids(); i++) {
    printSeperator();
    printExpression(e->getKid(i), s);
  }

  p->popIndent(); // pop indent added for recursive call
  printSeperator();
  *p << ")";
}

bool ExprSMTLIBPrinter::setSMTLIBboolOption(SMTLIBboolOptions option,
                                            SMTLIBboolValues value) {
  std::pair<std::map<SMTLIBboolOptions, bool>::iterator, bool> thePair;
  bool theValue = (value == OPTION_TRUE) ? true : false;

  switch (option) {
  case PRINT_SUCCESS:
  case PRODUCE_MODELS:
  case INTERACTIVE_MODE:
    thePair = smtlibBoolOptions.insert(
        std::pair<SMTLIBboolOptions, bool>(option, theValue));

    if (value == OPTION_DEFAULT) {
      // we should unset (by removing from map) this option so the solver uses
      // its default
      smtlibBoolOptions.erase(thePair.first);
      return true;
    }

    if (!thePair.second) {
      // option was already present so modify instead.
      thePair.first->second = value;
    }
    return true;
  default:
    return false;
  }
}

void
ExprSMTLIBPrinter::setArrayValuesToGet(const std::vector<const Array *> &a) {
  arraysToCallGetValueOn = &a;

  // This option must be set in order to use the SMTLIBv2 command (get-value ()
  // )
  if (!a.empty())
    setSMTLIBboolOption(PRODUCE_MODELS, OPTION_TRUE);

  /* There is a risk that users will ask about array values that aren't
   * even in the query. We should add them to the usedArrays list and hope
   * that the solver knows what to do when we ask for the values of arrays
   * that don't feature in our query!
   */
  for (std::vector<const Array *>::const_iterator i = a.begin(); i != a.end();
       ++i) {
    usedArrays.insert(*i);
  }
}

const char *ExprSMTLIBPrinter::getSMTLIBOptionString(
    ExprSMTLIBPrinter::SMTLIBboolOptions option) {
  switch (option) {
  case PRINT_SUCCESS:
    return "print-success";
  case PRODUCE_MODELS:
    return "produce-models";
  case INTERACTIVE_MODE:
    return "interactive-mode";
  default:
    return "unknown-option";
  }
}
}
