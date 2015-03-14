#include<vector>
#include"z3++.h"
#include <sstream>
#include <cstdlib>
#include <math.h>
#include <iomanip>
#include <string>
#include <exception>      // std::exception
using namespace z3;

/**
   Demonstration of how Z3 can be used to prove validity of
   De Morgan's Duality Law: {e not(x and y) <-> (not x) or ( not y) }
*/
void demorgan() {
    std::cout << "de-Morgan example\n";
    
    context c;

    expr x = c.bool_const("x");
    expr y = c.bool_const("y");
    expr conjecture = !(x && y) == (!x || !y);
    
    solver s(c);
    // adding the negation of the conjecture as a constraint.
    s.add(x);
    s.add(y);
    s.add(!conjecture);
    std::cout << s << "\n";
    switch (s.check()) {
    case unsat:   std::cout << "de-Morgan is valid\n"; break;
    case sat:     std::cout << "de-Morgan is not valid\n"; break;
    case unknown: std::cout << "unknown\n"; break;
    }
}

/**
   \brief Find a model for <tt>x >= 1 and y < x + 3</tt>.
*/
void find_model_example1() {
    std::cout << "find_model_example1\n";
    context c;
    expr x = c.real_const("x");
    expr y = c.real_const("y");
    solver s(c);

    s.add(x*x == 2);
    std::cout << s.check() << "\n";
     c.set(":pp-decimal", true);
    model m = s.get_model();
    std::cout << m << "\n";
    // traversing the model
    for (unsigned i = 0; i < m.size(); i++) {
        func_decl v = m[i];
        // this problem contains only constants
        assert(v.arity() == 0); 
        std::cout << v.name() << " = " << m.get_const_interp(v) << "\n";
    }
    // we can evaluate expressions in the model.
    std::cout << "x + y + 1 = " << m.eval(x + y + 1) << "\n";
}


/**
   \brief Prove <tt>x = y implies g(x) = g(y)</tt>, and
   disprove <tt>x = y implies g(g(x)) = g(y)</tt>.

   This function demonstrates how to create uninterpreted types and
   functions.
*/
void prove_example1() {
    std::cout << "prove_example1\n";
    
    context c;
    expr x      = c.int_const("x");
    expr y      = c.int_const("y");
    sort I      = c.int_sort();
    func_decl g = function("g", I, I);
    
    solver s(c);
    expr conjecture1 = implies(x == y, g(x) == g(y));
    std::cout << "conjecture 1\n" << conjecture1 << "\n";
    s.add(!conjecture1);
    if (s.check() == unsat) 
        std::cout << "proved" << "\n";
    else
        std::cout << "failed to prove" << "\n";

    s.reset(); // remove all assertions from solver s

    expr conjecture2 = implies(x == y, g(g(x)) == g(y));
    std::cout << "conjecture 2\n" << conjecture2 << "\n";
    s.add(!conjecture2);
    if (s.check() == unsat) {
        std::cout << "proved" << "\n";
    }
    else {
        std::cout << "failed to prove" << "\n";
        model m = s.get_model();
        std::cout << "counterexample:\n" << m << "\n";
        std::cout << "g(g(x)) = " << m.eval(g(g(x))) << "\n";
        std::cout << "g(y)    = " << m.eval(g(y)) << "\n";
    }
}

/**
   \brief Prove <tt>not(g(g(x) - g(y)) = g(z)), x + z <= y <= x implies z < 0 </tt>.
   Then, show that <tt>z < -1</tt> is not implied.

   This example demonstrates how to combine uninterpreted functions and arithmetic.
*/
void prove_example2() {
    std::cout << "prove_example1\n";
    
    context c;
    expr x      = c.int_const("x");
    expr y      = c.int_const("y");
    expr z      = c.int_const("z");
    sort I      = c.int_sort();
    func_decl g = function("g", I, I);
    
    expr conjecture1 = implies(g(g(x) - g(y)) != g(z) && x + z <= y && y <= x,
                               z < 0);

    solver s(c);
    s.add(!conjecture1);
    std::cout << "conjecture 1:\n" << conjecture1 << "\n";
    if (s.check() == unsat) 
        std::cout << "proved" << "\n";
    else
        std::cout << "failed to prove" << "\n";

    expr conjecture2 = implies(g(g(x) - g(y)) != g(z) && x + z <= y && y <= x,
                               z < -1);
    s.reset();
    s.add(!conjecture2);
    std::cout << "conjecture 2:\n" << conjecture2 << "\n";
    if (s.check() == unsat) {
        std::cout << "proved" << "\n";
    }
    else {
        std::cout << "failed to prove" << "\n";
        std::cout << "counterexample:\n" << s.get_model() << "\n";
    }
}

/**
   \brief Nonlinear arithmetic example 1
*/
void nonlinear_example1() {
    std::cout << "nonlinear example 1\n";
    config cfg;
    cfg.set(":auto-config", true);
    context c(cfg);

    expr x = c.real_const("x");
    expr y = c.real_const("y");
    expr z = c.real_const("z");
                     
    solver s(c);

    s.add(x*x + y*y == 1);                     // x^2 + y^2 == 1
    s.add(x*x*x + z*z*z < c.real_val("1/2"));  // x^3 + z^3 < 1/2
    s.add(z != 0);
    std::cout << s.check() << "\n";
    model m = s.get_model();
    std::cout << m << "\n";
    c.set(":pp-decimal", true); // set decimal notation
    std::cout << "model in decimal notation\n";
    std::cout << m << "\n";
    c.set(":pp-decimal-precision", 50); // increase number of decimal places to 50.
    std::cout << "model using 50 decimal places\n";
    std::cout << m << "\n";
}

/**
   \brief Simple function that tries to prove the given conjecture using the following steps:
   1- create a solver
   2- assert the negation of the conjecture
   3- checks if the result is unsat.
*/
void prove(expr conjecture) {
    context & c = conjecture.ctx();
    solver s(c);
    s.add(!conjecture);
    std::cout << "conjecture:\n" << conjecture << "\n";
    if (s.check() == unsat) {
        std::cout << "proved" << "\n";
    }
    else {
        std::cout << "failed to prove" << "\n";
        std::cout << "counterexample:\n" << s.get_model() << "\n";
    }
}

/**
   \brief Simple bit-vector example. This example disproves that x - 10 <= 0 IFF x <= 10 for (32-bit) machine integers
*/
void bitvector_example1() {
    std::cout << "bitvector example 1\n";
    context c;
    expr x = c.bv_const("x", 32);
    
    // using signed <=
    prove((x - 10 <= 0) == (x <= 10));

    // using unsigned <=
    prove(ule(x - 10, 0) == ule(x, 10));
}

/**
   \brief Find x and y such that: x ^ y - 103 == x * y
*/
void bitvector_example2() {
    std::cout << "bitvector example 2\n";
    context c;
    expr x = c.bv_const("x", 32);
    expr y = c.bv_const("y", 32);
    solver s(c);
    // In C++, the operator == has higher precedence than ^.
    s.add((x ^ y) - 103 == x * y);
    std::cout << s << "\n";
    std::cout << s.check() << "\n";
    std::cout << s.get_model() << "\n";
}

/**
   \brief Mixing C and C++ APIs.
*/
void capi_example() {
    std::cout << "capi example\n";
    context c;
    expr x = c.bv_const("x", 32);
    expr y = c.bv_const("y", 32);
    // Invoking a C API function, and wrapping the result using an expr object.
    expr r = to_expr(c, Z3_mk_bvsrem(c, x, y));
    std::cout << "r: " << r << "\n";
}

/**
   \brief Demonstrate how to evaluate expressions in a model.
*/
void eval_example1() {
    std::cout << "eval example 1\n";
    context c;
    expr x = c.int_const("x");
    expr y = c.int_const("y");
    solver s(c);

    /* assert x < y */
    s.add(x < y);
    /* assert x > 2 */
    s.add(x > 2);
    
    std::cout << s.check() << "\n";
    
    model m = s.get_model();
    std::cout << "Model:\n" << m << "\n";
    std::cout << "x+y = " << m.eval(x+y) << "\n";
}

/**
   \brief Several contexts can be used simultaneously.
*/
void two_contexts_example1() {
    std::cout << "two contexts example 1\n";
    context c1, c2;
    
    expr x = c1.int_const("x");
    expr n = x + 1;
    // We cannot mix expressions from different contexts, but we can copy
    // an expression from one context to another.
    // The following statement copies the expression n from c1 to c2.
    expr n1 = to_expr(c2, Z3_translate(c1, n, c2));
    std::cout << n1 << "\n";
}

/**
   \brief Demonstrates how to catch API usage errors.
 */
void error_example() {
    std::cout << "error example\n";

    context c;
    expr x = c.bool_const("x");

    // Error using the C API can be detected using Z3_get_error_code.
    // The next call fails because x is a constant.
    Z3_ast arg = Z3_get_app_arg(c, x, 0);
    if (Z3_get_error_code(c) != Z3_OK) {
        std::cout << "last call failed.\n";
    }

    // The C++ layer converts API usage errors into exceptions.
    try {
        // The next call fails because x is a Boolean.
        expr n = x + 1;
    }
    catch (exception ex) {
        std::cout << "failed: " << ex << "\n";
    }

    // The functions to_expr, to_sort and to_func_decl also convert C API errors into exceptions.
    try {
        expr arg = to_expr(c, Z3_get_app_arg(c, x, 0));
    }
    catch (exception ex) {
        std::cout << "failed: " << ex << "\n";
    }
}

/**
    \brief Demonstrate different ways of creating rational numbers: decimal and fractional representations.
*/
void numeral_example() {
    std::cout << "numeral example\n";
    context c;
    
    expr n1 = c.real_val("1/2");
    expr n2 = c.real_val("0.5");
    expr n3 = c.real_val(1, 2);
    std::cout << n1 << " " << n2 << " " << n3 << "\n";
    prove(n1 == n2 && n1 == n3);
    
    n1 = c.real_val("-1/3");
    n2 = c.real_val("-0.3333333333333333333333333333333333");
    std::cout << n1 << " " << n2 << "\n";
    prove(n1 != n2);
}

/**
   \brief Test ite-term (if-then-else terms).
*/
void ite_example() {
    std::cout << "if-then-else example\n";
    context c;
    
    expr f    = c.bool_val(false);
    expr one  = c.int_val(1);
    expr zero = c.int_val(0);
    expr ite  = to_expr(c, Z3_mk_ite(c, f, one, zero));

    std::cout << "term: " << ite << "\n";
}

/**
   \brief Small example using quantifiers.
*/
void quantifier_example() {
    std::cout << "quantifier example\n";
    context c;
    
    expr x = c.int_const("x");
    expr y = c.int_const("y");
    sort I = c.int_sort();
    func_decl f = function("f", I, I, I);
    
    solver s(c);
    
    // making sure model based quantifier instantiation is enabled.
    params p(c);
    p.set(":mbqi", true);
    s.set(p);

    s.add(forall(x, y, f(x, y) >= 0));
    expr a = c.int_const("a");
    s.add(f(a, a) < a);
    std::cout << s << "\n";
    std::cout << s.check() << "\n";
    std::cout << s.get_model() << "\n";
    s.add(a < 0);
    std::cout << s.check() << "\n";
}

/**
   \brief Unsat core example
*/
void unsat_core_example1() {
    std::cout << "unsat core example1\n";
    context c;
    // We use answer literals to track assertions.
    // An answer literal is essentially a fresh Boolean marker
    // that is used to track an assertion.
    // For example, if we want to track assertion F, we 
    // create a fresh Boolean variable p and assert (p => F)
    // Then we provide p as an argument for the check method.
    expr p1 = c.bool_const("p1");
    expr p2 = c.bool_const("p2");
    expr p3 = c.bool_const("p3");
    expr x  = c.int_const("x");
    expr y  = c.int_const("y");
    solver s(c);
    s.add(implies(p1, x > 10));
    s.add(implies(p1, y > x));
    s.add(implies(p2, y < 5));
    s.add(implies(p3, y > 0));
    expr assumptions[3] = { p1, p2, p3 };
    std::cout << s.check(3, assumptions) << "\n";
    expr_vector core = s.unsat_core();
    std::cout << core << "\n";
    std::cout << "size: " << core.size() << "\n";
    for (unsigned i = 0; i < core.size(); i++) {
        std::cout << core[i] << "\n";
    }
    // Trying again without p2
    expr assumptions2[2] = { p1, p3 };
    std::cout << s.check(2, assumptions2) << "\n";
}

/**
   \brief Unsat core example 2
*/
void unsat_core_example2() {
    std::cout << "unsat core example 2\n";
    context c;
    // The answer literal mechanism, described in the previous example,
    // tracks assertions. An assertion can be a complicated
    // formula containing containing the conjunction of many subformulas.
    expr p1 = c.bool_const("p1");
    expr x  = c.int_const("x");
    expr y  = c.int_const("y");
    solver s(c);
    expr F  = x > 10 && y > x && y < 5 && y > 0;
    s.add(implies(p1, F));
    expr assumptions[1] = { p1 };
    std::cout << s.check(1, assumptions) << "\n";
    expr_vector core = s.unsat_core();
    std::cout << core << "\n";
    std::cout << "size: " << core.size() << "\n";
    for (unsigned i = 0; i < core.size(); i++) {
        std::cout << core[i] << "\n";
    }
    // The core is not very informative, since p1 is tracking the formula F
    // that is a conjunction of subformulas.
    // Now, we use the following piece of code to break this conjunction
    // into individual subformulas. First, we flat the conjunctions by
    // using the method simplify.
    std::vector<expr> qs; // auxiliary vector used to store new answer literals.
    assert(F.is_app()); // I'm assuming F is an application.
    if (F.decl().decl_kind() == Z3_OP_AND) {
        // F is a conjunction
        std::cout << "F num. args (before simplify): " << F.num_args() << "\n";
        F = F.simplify();
        std::cout << "F num. args (after simplify):  " << F.num_args() << "\n";
        for (unsigned i = 0; i < F.num_args(); i++) {
            std::cout << "Creating answer literal q" << i << " for " << F.arg(i) << "\n";
            std::stringstream qname; qname << "q" << i;
            expr qi = c.bool_const(qname.str().c_str()); // create a new answer literal
            s.add(implies(qi, F.arg(i)));
            qs.push_back(qi);
        }
    }
    // The solver s already contains p1 => F
    // To disable F, we add (not p1) as an additional assumption
    qs.push_back(!p1);
    std::cout << s.check(qs.size(), &qs[0]) << "\n";
    expr_vector core2 = s.unsat_core();
    std::cout << core2 << "\n";
    std::cout << "size: " << core2.size() << "\n";
    for (unsigned i = 0; i < core2.size(); i++) {
        std::cout << core2[i] << "\n";
    }
}

/**
   \brief Unsat core example 3
*/
void unsat_core_example3() {
    // Extract unsat core using tracked assertions
    std::cout << "unsat core example 3\n";
    context c;
    expr x  = c.int_const("x");
    expr y  = c.int_const("y");
    solver s(c);

    // enabling unsat core tracking
    params p(c);
    p.set(":unsat-core", true);
    s.set(p);

    // The following assertion will not be tracked.
    s.add(x > 0);

    // The following assertion will be tracked using Boolean variable p1.
    // The C++ wrapper will automatically create the Boolean variable.
    s.add(y > 0, "p1");

    // Asserting other tracked assertions.
    s.add(x < 10, "p2");
    s.add(y < 0,  "p3");

    std::cout << s.check() << "\n";
    std::cout << s.unsat_core() << "\n";
}

void tactic_example1() {
    /*
      Z3 implements a methodology for orchestrating reasoning engines where "big" symbolic
      reasoning steps are represented as functions known as tactics, and tactics are composed
      using combinators known as tacticals. Tactics process sets of formulas called Goals.

      When a tactic is applied to some goal G, four different outcomes are possible. The tactic succeeds
      in showing G to be satisfiable (i.e., feasible); succeeds in showing G to be unsatisfiable (i.e., infeasible); 
      produces a sequence of subgoals; or fails. When reducing a goal G to a sequence of subgoals G1, ..., Gn, 
      we face the problem of model conversion. A model converter construct a model for G using a model for some subgoal Gi.

      In this example, we create a goal g consisting of three formulas, and a tactic t composed of two built-in tactics: 
      simplify and solve-eqs. The tactic simplify apply transformations equivalent to the ones found in the command simplify. 
      The tactic solver-eqs eliminate variables using Gaussian elimination. Actually, solve-eqs is not restricted 
      only to linear arithmetic. It can also eliminate arbitrary variables. 
      Then, sequential composition combinator & applies simplify to the input goal and solve-eqs to each subgoal produced by simplify. 
      In this example, only one subgoal is produced.
    */
    std::cout << "tactic example 1\n";
    context c;
    expr x = c.real_const("x");
    expr y = c.real_const("y");
    goal g(c);
    g.add(x > 0);
    g.add(y > 0);
    g.add(x == y + 2);
    std::cout << g << "\n";
    tactic t1(c, "simplify");
    tactic t2(c, "solve-eqs");
    tactic t = t1 & t2;
    apply_result r = t(g);
    std::cout << r << "\n";
}

void tactic_example2() {
    /*
      In Z3, we say a clause is any constraint of the form (f_1 || ... || f_n). 
      The tactic split-clause will select a clause in the input goal, and split it n subgoals. 
      One for each subformula f_i.
    */
    std::cout << "tactic example 2\n";
    context c;
    expr x = c.real_const("x");
    expr y = c.real_const("y");
    goal g(c);
    g.add(x < 0 || x > 0);
    g.add(x == y + 1);
    g.add(y < 0);
    tactic t(c, "split-clause");
    apply_result r = t(g);
    for (unsigned i = 0; i < r.size(); i++) {
        std::cout << "subgoal " << i << "\n" << r[i] << "\n";
    }
}

void tactic_example3() {
    /*
      - The choice combinator t | s first applies t to the given goal, if it fails then returns the result of s applied to the given goal.      
      - repeat(t) Keep applying the given tactic until no subgoal is modified by it.
      - repeat(t, n) Keep applying the given tactic until no subgoal is modified by it, or the number of iterations is greater than n.
      - try_for(t, ms) Apply tactic t to the input goal, if it does not return in ms millisenconds, it fails.
      - with(t, params) Apply the given tactic using the given parameters.      
    */
    std::cout << "tactic example 3\n";
    context c;
    expr x = c.real_const("x");
    expr y = c.real_const("y");
    expr z = c.real_const("z");
    goal g(c);
    g.add(x == 0 || x == 1);
    g.add(y == 0 || y == 1);
    g.add(z == 0 || z == 1);
    g.add(x + y + z > 2);
    // split all clauses
    tactic split_all = repeat(tactic(c, "split-clause") | tactic(c, "skip"));
    std::cout << split_all(g) << "\n";
    tactic split_at_most_2 = repeat(tactic(c, "split-clause") | tactic(c, "skip"), 1);
    std::cout << split_at_most_2(g) << "\n";
    // In the tactic split_solver, the tactic solve-eqs discharges all but one goal. 
    // Note that, this tactic generates one goal: the empty goal which is trivially satisfiable (i.e., feasible)
    tactic split_solve = split_all & tactic(c, "solve-eqs");
    std::cout << split_solve(g) << "\n";
}

void tactic_example4() {
    /*
      A tactic can be converted into a solver object using the method mk_solver(). 
      If the tactic produces the empty goal, then the associated solver returns sat. 
      If the tactic produces a single goal containing False, then the solver returns unsat. 
      Otherwise, it returns unknown.

      In this example, the tactic t implements a basic bit-vector solver using equation solving, 
      bit-blasting, and a propositional SAT solver. 
      We use the combinator `with` to configure our little solver. 
      We also include the tactic `aig` which tries to compress Boolean formulas using And-Inverted Graphs.
    */
    std::cout << "tactic example 4\n";
    context c;
    params p(c);
    p.set(":mul2concat", true);
    tactic t = 
        with(tactic(c, "simplify"), p) &
        tactic(c, "solve-eqs") &
        tactic(c, "bit-blast") &
        tactic(c, "aig") &
        tactic(c, "sat");
    solver s = t.mk_solver();
    expr x = c.bv_const("x", 16);
    expr y = c.bv_const("y", 16);
    s.add(x*32 + y == 13);
    // In C++, the operator < has higher precedence than &.
    s.add((x & y) < 10);
    s.add(y > -100);
    std::cout << s.check() << "\n";
    model m = s.get_model();
    std::cout << m << "\n";
    std::cout << "x*32 + y = " << m.eval(x*32 + y) << "\n";
    std::cout << "x & y    = " << m.eval(x & y) << "\n";
}

void tactic_example5() {
    /*
      The tactic smt wraps the main solver in Z3 as a tactic.
    */
    std::cout << "tactic example 5\n";
    context c;
    expr x = c.int_const("x");
    expr y = c.int_const("y");
    solver s = tactic(c, "smt").mk_solver();
    s.add(x > y + 1);
    std::cout << s.check() << "\n";
    std::cout << s.get_model() << "\n";
}

void tactic_example6() {
    /*
      In this example, we show how to implement a solver for integer arithmetic using SAT. 
      The solver is complete only for problems where every variable has a lower and upper bound.
    */ 
    std::cout << "tactic example 6\n";
    context c;
    params p(c);
    p.set(":arith-lhs", true);
    p.set(":som", true); // sum-of-monomials normal form
    solver s = 
        (with(tactic(c, "simplify"), p) &
         tactic(c, "normalize-bounds") &
         tactic(c, "lia2pb") &
         tactic(c, "pb2bv") &
         tactic(c, "bit-blast") &
         tactic(c, "sat")).mk_solver();
    expr x = c.int_const("x");
    expr y = c.int_const("y");
    expr z = c.int_const("z");
    s.add(x > 0 && x < 10);
    s.add(y > 0 && y < 10);
    s.add(z > 0 && z < 10);
    s.add(3*y + 2*x == z);
    std::cout << s.check() << "\n";
    std::cout << s.get_model() << "\n";
    s.reset();
    s.add(3*y + 2*x == z);
    std::cout << s.check() << "\n";
}

void tactic_example7() {
    /*
      Tactics can be combined with solvers. 
      For example, we can apply a tactic to a goal, produced a set of subgoals, 
      then select one of the subgoals and solve it using a solver. 
      This example demonstrates how to do that, and 
      how to use model converters to convert a model for a subgoal into a model for the original goal.
    */
    std::cout << "tactic example 7\n";
    context c;
    tactic t = 
        tactic(c, "simplify") &
        tactic(c, "normalize-bounds") &
        tactic(c, "solve-eqs");
    expr x = c.int_const("x");
    expr y = c.int_const("y");
    expr z = c.int_const("z");
    goal g(c);
    g.add(x > 10);
    g.add(y == x + 3);
    g.add(z > y);
    apply_result r = t(g);
    // r contains only one subgoal
    std::cout << r << "\n";
    solver s(c);
    goal subgoal = r[0];
    for (unsigned i = 0; i < subgoal.size(); i++) {
        s.add(subgoal[i]);
    }
    std::cout << s.check() << "\n";
    model m = s.get_model();
    std::cout << "model for subgoal:\n" << m << "\n";
    std::cout << "model for original goal:\n" << r.convert_model(m) << "\n";
}

void tactic_example8() {
    /*
      Probes (aka formula measures) are evaluated over goals. 
      Boolean expressions over them can be built using relational operators and Boolean connectives. 
      The tactic fail_if(cond) fails if the given goal does not satisfy the condition cond.
      Many numeric and Boolean measures are available in Z3.

      In this example, we build a simple tactic using fail_if. 
      It also shows that a probe can be applied directly to a goal.
    */
    std::cout << "tactic example 8\n";
    context c;

    expr x = c.int_const("x");
    expr y = c.int_const("y");
    expr z = c.int_const("z");
    
    goal g(c);
    g.add(x + y + z > 0);
    
    probe p(c, "num-consts");
    std::cout << "num-consts: " << p(g) << "\n";
    
    tactic t = fail_if(p > 2);
    try {
        t(g);
    }
    catch (exception) {
        std::cout << "tactic failed...\n";
    }
    std::cout << "trying again...\n";
    g.reset();
    g.add(x + y > 0);
    std::cout << t(g) << "\n";
}

void tactic_example9() {
    /*
      The combinator (tactical) cond(p, t1, t2) is a shorthand for:
      
      (fail_if(p) & t1) | t2

      The combinator when(p, t) is a shorthand for:

      cond(p, t, tactic(c, "skip"))
      
      The tactic skip just returns the input goal. 
      This example demonstrates how to use the cond combinator.
    */
    std::cout << "tactic example 9\n";
    context c;

    expr x = c.int_const("x");
    expr y = c.int_const("y");
    expr z = c.int_const("z");

    goal g(c);
    g.add(x*x - y*y >= 0);

    probe p(c, "num-consts");
    tactic t = cond(p > 2, tactic(c, "simplify"), tactic(c, "factor"));
    std::cout << t(g) << "\n";

    g.reset();
    g.add(x + x + y + z >= 0);
    g.add(x*x - y*y >= 0);
    std::cout << t(g) << "\n";
}

void tactic_qe() {
    std::cout << "tactic example using quantifier elimination\n";
    context c;
    
    // Create a solver using "qe" and "smt" tactics
    solver s = 
        (tactic(c, "qe") &
         tactic(c, "smt")).mk_solver();
    
    expr a = c.int_const("a");
    expr b = c.int_const("b");
    expr x = c.int_const("x");
    expr f = implies(x <= a, x < b);
    
    expr qf = forall(x, f);

    std::cout << qf << "\n";
    
    s.add(qf);
    std::cout << s.check() << "\n";
    std::cout << s.get_model() << "\n";
}

void visit(expr const & e) {
    if (e.is_app()) {
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; i++) {
            visit(e.arg(i));
        }
        // do something
        // Example: print the visited expression
        func_decl f = e.decl();
        std::cout << "application of " << f.name() << ": " << e << "\n";
    }
    else if (e.is_quantifier()) {
        visit(e.body());
        // do something
    }
    else { 
        assert(e.is_var());
        // do something
    }
}

void tst_visit() {
    std::cout << "visit example\n";
    context c;

    expr x = c.int_const("x");
    expr y = c.int_const("y");
    expr z = c.int_const("z");
    expr f = x*x - y*y >= 0;
    
    visit(f);
}

float getModelVal(context &c, model &m, expr &r){
 char *stopString;
 z3::expr rl = m.get_const_interp(r.decl());
 Z3_string sl = Z3_get_numeral_decimal_string(c, rl,50);
 return strtof(sl, &stopString);
}

void test_approxPoly(){
  context c;
  expr x = c.real_const("x");
  expr y = c.real_const("y");

  expr x2y3_c = c.real_val("57220.4558");
  expr x2y2_c = c.real_val("132179.2488");
  expr x2y_c = c.real_val("89111.3145");
  expr x2_c = c.real_val("13230.89058");
  expr xy3_c = c.real_val("51307.67848");
  expr xy2_c = c.real_val("185922.6236");
  expr xy_c = c.real_val("223447.4574");
  expr x_c = c.real_val("89246.349");
  expr y3_c = c.real_val("7418.314546");
  expr y2_c = c.real_val("23459.31982");
  expr y_c = c.real_val("24817.0252");
  expr c_c = c.real_val("8768.247439");

  expr x_down = c.real_val("0.18");
  expr x_up = c.real_val("0.20");
  expr y_down = c.real_val("1.18");
  expr y_up = c.real_val("1.21");

  solver s(c);
  s.add(x > x_down);
  s.add(x < x_up);
  s.add(y > y_down);
  s.add(y < y_up);
  s.add(x2y3_c*x*x*y*y*y - x2y2_c*x*x*y*y + x2y_c*x*x*y - x2_c*x*x + xy3_c*x*y*y*y -xy2_c*x*y*y + xy_c*x*y -x_c*x + y3_c*y*y*y - y2_c*y*y + y_c*y - c_c == 100);
  std::cout << s.check() << std::endl;
  model m(c, s.get_model());
  float x_v = getModelVal(c, m, x);
  float y_v = getModelVal(c, m, y);

  std::cout << "var_v: " << std::setprecision(17) 
       << x_v << " " 
       << y_v << " ";
}

void test_params(){
 context c;
  expr x = c.real_const("x");
  expr y = c.real_const("y");
  expr stepPosX = c.real_const("stepPosX");
  expr stepPosY = c.real_const("stepPosY");
  expr x0 = c.real_const("x0");
  expr y0 = c.real_const("y0");
  expr windowPosX = c.int_const("windowPosX");
  expr windowPosY = c.int_const("windowPosY");
  expr stepSize = c.real_const("stepSize");

  solver s(c);
  //params p(c);
  //p.set("shuffle_vars", true);

  //p.set("random_freq", 1.0); 
  //p.set("random_seed", (unsigned)1024) ;
  //s.set(p);
  
  s.add(stepPosX == x0 + windowPosX * stepSize);
  s.add(stepPosY == y0 - windowPosY * stepSize);
  s.add(y == 2 * stepPosX * stepPosY + stepPosY);
  s.add(x == stepPosX * stepPosX - stepPosY * stepPosY + stepPosX);
  s.add(4.0 == x*x + y*y);
  s.add(x0 > 0);
  s.add(y0 > 0);
  s.add(stepSize > 0);

std::cout << s.check() << std::endl;
}
void test_raytracing_timeout(){
  context c;
  expr rx = c.real_const("rx");
  expr ry = c.real_const("ry");
  expr rz = c.real_const("rz");
  expr sx = c.real_const("sx");
  expr sy = c.real_const("sy");
  expr sz = c.real_const("sz");
  expr radius = c.real_const("radius");
  expr A = c.real_const("A");
  expr B = c.real_const("A");
  expr C = c.real_const("C");
  expr D = c.real_const("D");
  expr two = c.real_val("-2.0");
  expr four = c.real_val("4.0");
  expr lower = c.real_val("9.720005035400391");
  expr upper = c.real_val("9.820005416870117");
  expr cond = c.real_val("0");
  expr constant = c.real_val("0.125");
  
  expr s0_upper = c.real_val("-0.963098645210266");
  expr s1_upper = c.real_val("0.500000059604645");
  expr s2_upper = c.real_val("-0.999999940395355");
  expr r0_upper = c.real_val( "9.750000953674316");
  expr r1_upper = c.real_val( "-0.499999970197678");
  expr r2_upper = c.real_val( "0.125000014901161");
  expr radius_upper = c.real_val( "1.177559256553650");

  expr s0_lower = c.real_val("-0.963098764419556");
  expr s1_lower = c.real_val("0.499999970197678");
  expr s2_lower = c.real_val("-1.000000119209290");
  expr r0_lower = c.real_val("9.749999046325684");
  expr r1_lower = c.real_val("-0.500000059604645");
  expr r2_lower = c.real_val("0.124999992549419");
  expr radius_lower = c.real_val("1.177559018135071");


  solver s(c);
  
  params p(c);
  //p.set("shuffle_vars", true);

  //p.set("random_freq", 1.0); 
  //p.set("random_seed", (unsigned)1024) ;
  s.set(p);

  s.add(cond == (two*(sx*rx+sy*ry+sz*rz))*(two*(sx*rx+sy*ry+sz*rz)) - four * (rx*rx+ry*ry+rz*rz) * (sx*sx+sy*sy+sz*sz - radius) - constant);
  s.add(rx > lower);
  s.add(rx < upper);
  s.add(sx < s0_lower || sx > s0_upper);
  s.add(sy < s1_lower || sy > s1_upper);
  s.add(sz < s2_lower || sz > s2_upper);
  s.add(rx < r0_lower || rx > r0_upper);
  s.add(ry < r1_lower || ry > r1_upper);
  s.add(rz < r2_lower || rz > r2_upper);
  s.add(radius < radius_lower || radius > radius_upper);
  s.check();
  model m(c, s.get_model());
  std::cout << m;
}

int main() {
    try {
      //test_raytracing_timeout();
      test_approxPoly();
      /*
        demorgan(); std::cout << "\n";
        find_model_example1(); std::cout << "\n";
        prove_example1(); std::cout << "\n";
        prove_example2(); std::cout << "\n";
        nonlinear_example1(); std::cout << "\n";
        bitvector_example1(); std::cout << "\n";
        bitvector_example2(); std::cout << "\n";
        capi_example(); std::cout << "\n";
        eval_example1(); std::cout << "\n";
        two_contexts_example1(); std::cout << "\n";
        error_example(); std::cout << "\n";
        numeral_example(); std::cout << "\n";
        ite_example(); std::cout << "\n";
        quantifier_example(); std::cout << "\n";
        unsat_core_example1(); std::cout << "\n";
        unsat_core_example2(); std::cout << "\n";
        unsat_core_example3(); std::cout << "\n";
        tactic_example1(); std::cout << "\n";
        tactic_example2(); std::cout << "\n";
        tactic_example3(); std::cout << "\n";
        tactic_example4(); std::cout << "\n";
        tactic_example5(); std::cout << "\n";
        tactic_example6(); std::cout << "\n";
        tactic_example7(); std::cout << "\n";
        tactic_example8(); std::cout << "\n";
        tactic_example9(); std::cout << "\n";
        tactic_qe(); std::cout << "\n";
        tst_visit(); std::cout << "\n";
        std::cout << "done\n";
      */
    }
    catch (exception & ex) {
        std::cout << "unexpected error: " << ex << "\n";
    }
    return 0;
}

