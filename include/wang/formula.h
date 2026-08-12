#ifndef WANG_FORMULA_H
#define WANG_FORMULA_H

#include <stddef.h>
#include <stdint.h>

/*
 * Canonical in-memory representation of a Cubic Monotone 1-in-3 SAT
 * instance.
 *
 * Clause storage is owned by the caller (eventually, the parser). Reduction
 * builders borrow the formula and its clauses without modifying them.
 */
typedef struct {
    /* Canonical 0-based indices in 0 .. variable_count - 1. */
    uint32_t variable_index[3];
} Cm13Clause;

typedef struct {
    uint32_t variable_count;

    Cm13Clause *clauses;
    size_t clause_count;
} Cm13Formula;

void cm13_formula_destroy(Cm13Formula *formula);

#endif /* WANG_FORMULA_H */
