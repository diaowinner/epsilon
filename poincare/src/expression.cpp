#include <poincare/expression.h>
#include <poincare/preferences.h>
#include <poincare/function.h>
#include <poincare/list_data.h>
#include <poincare/matrix_data.h>
#include "expression_parser.hpp"
#include "expression_lexer.hpp"
extern "C" {
#include <assert.h>
}

#include "simplify/rules.h"

int poincare_expression_yyparse(Poincare::Expression ** expressionOutput);

namespace Poincare {

Expression::~Expression() {
}

#include <stdio.h>

Expression * Expression::parse(char const * string) {
  if (string[0] == 0) {
    return nullptr;
  }
  YY_BUFFER_STATE buf = poincare_expression_yy_scan_string(string);
  Expression * expression = 0;
  if (poincare_expression_yyparse(&expression) != 0) {
    // Parsing failed because of invalid input or memory exhaustion
    if (expression != nullptr) {
      delete expression;
      expression = nullptr;
    }
  }
  poincare_expression_yy_delete_buffer(buf);

  return expression;
}

ExpressionLayout * Expression::createLayout(FloatDisplayMode floatDisplayMode) const {
  switch (floatDisplayMode) {
    case FloatDisplayMode::Default:
      return privateCreateLayout(Preferences::sharedPreferences()->displayMode());
    default:
      return privateCreateLayout(floatDisplayMode);
  }
}

Expression * Expression::evaluate(Context& context, AngleUnit angleUnit) const {
  switch (angleUnit) {
    case AngleUnit::Default:
      return privateEvaluate(context, Preferences::sharedPreferences()->angleUnit());
    default:
      return privateEvaluate(context, angleUnit);
  }
}

float Expression::approximate(Context& context, AngleUnit angleUnit) const {
  switch (angleUnit) {
    case AngleUnit::Default:
      return privateApproximate(context, Preferences::sharedPreferences()->angleUnit());
    default:
      return privateApproximate(context, angleUnit);
  }
}

Expression * Expression::simplify() const {
  /* We make sure that the simplification is deletable.
   * Indeed, we don't want an expression with some parts deletable and some not
   */

  // If we have a leaf node nothing can be simplified.
  if (this->numberOfOperands()==0) {
    return this->clone();
  }

  Expression * result = this->clone();
  Expression * tmp = nullptr;

  bool simplification_pass_was_useful = true;
  while (simplification_pass_was_useful) {
    /* We recursively simplify the children expressions.
     * Note that we are sure to get the samne number of children as we had before
     */
    Expression ** simplifiedOperands = (Expression**) malloc(result->numberOfOperands() * sizeof(Expression*));
    for (int i = 0; i < result->numberOfOperands(); i++) {
      simplifiedOperands[i] = result->operand(i)->simplify();
    }

    /* Note that we don't need to clone the simplified children because they are
     * already cloned before. */
    tmp = result->cloneWithDifferentOperands(simplifiedOperands, result->numberOfOperands(), false);
    delete result;
    result = tmp;

    // The table is no longer needed.
    free(simplifiedOperands);

    simplification_pass_was_useful = false;
    for (int i=0; i<knumberOfSimplifications; i++) {
      const Simplification * simplification = (simplifications + i); // Pointer arithmetics
      Expression * simplified = simplification->simplify(result);
      if (simplified != nullptr) {
        simplification_pass_was_useful = true;
        delete result;
        result = simplified;
        break;
      }
    }
  }

  return result;
}

bool Expression::sequentialOperandsIdentity(const Expression * e) const {
  /* Here we simply test all operands for identity in the order they are defined
   * in. */
  for (int i=0; i<this->numberOfOperands(); i++) {
    if (!this->operand(i)->isIdenticalTo(e->operand(i))) {
      return false;
    }
  }
  return true;
}

bool Expression::combinatoryCommutativeOperandsIdentity(const Expression * e,
    bool * operandMatched, int leftToMatch) const {
  if (leftToMatch == 0) {
    return true;
  }

  // We try to test for equality the i-th operand of our first expression.
  int i = this->numberOfOperands() - leftToMatch;
  for (int j = 0; j<e->numberOfOperands(); j++) {
    /* If the operand of the second expression has already been associated with
     * a previous operand we skip it */
    if (operandMatched[j]) {
      continue;
    }
    if (this->operand(i)->isIdenticalTo(e->operand(j))) {
      // We managed to match this operand.
      operandMatched[j] = true;
      /* We check that we can match the rest in this configuration, if so we
       * are good. */
      if (this->combinatoryCommutativeOperandsIdentity(e, operandMatched, leftToMatch - 1)) {
        return true;
      }
      // Otherwise we backtrack.
      operandMatched[j] = false;
    }
  }

  return false;
}

bool Expression::commutativeOperandsIdentity(const Expression * e) const {
  int leftToMatch = this->numberOfOperands();

  /* We create a table allowing us to know which operands of the second
   * expression have been associated with one of the operands of the first
   * expression */
  bool * operandMatched = (bool *) malloc (this->numberOfOperands() * sizeof(bool));
  for (int i(0); i<this->numberOfOperands(); i++) {
    operandMatched[i] = false;
  }

  // We call our recursive helper.
  bool commutativelyIdentical = this->combinatoryCommutativeOperandsIdentity(e, operandMatched, leftToMatch);

  free(operandMatched);
  return commutativelyIdentical;
}

bool Expression::isIdenticalTo(const Expression * e) const {
  if (e->type() != this->type() || e->numberOfOperands() != this->numberOfOperands()) {
    return false;
  }
  if (this->isCommutative()) {
    if (!this->commutativeOperandsIdentity(e)) {
      return false;
    }
  } else {
    if (!this->sequentialOperandsIdentity(e)) {
      return false;
    }
  }
  return this->valueEquals(e);
}

bool Expression::isEquivalentTo(Expression * e) const {
  Expression * a = this->simplify();
  Expression * b = e->simplify();
  bool result = a->isIdenticalTo(b);
  delete a;
  delete b;
  return result;
}

bool Expression::valueEquals(const Expression * e) const {
  assert(this->type() == e->type());
  /* This behavior makes sense for value-less nodes (addition, product, fraction
   * power, etc… For nodes with a value (Integer, Float), this must be over-
   * -riden. */
  return true;
}

bool Expression::isCommutative() const {
  return false;
}

int Expression::writeTextInBuffer(char * buffer, int bufferSize) {
  return 0;
}

}
