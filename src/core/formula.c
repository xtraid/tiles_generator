#include "wang/formula.h"

#include <stdlib.h>

void cm13_formula_destroy(Cm13Formula *formula)
{
    if (formula == NULL) {
        return;
    }

    free(formula->clauses);
    formula->variable_count = 0;
    formula->clauses = NULL;
    formula->clause_count = 0;
}
