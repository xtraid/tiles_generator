#ifndef WANG_FORMULA_H
#define WANG_FORMULA_H

#include <stddef.h>
#include <stdint.h>

/*
 * Canonical in-memory representation of a Cubic Monotone 1-in-3 SAT
 * instance.
 *
 * Storage is owned by the caller (eventually, the parser). Reduction
 * builders borrow the formula and its nested arrays without modifying them.
 */
typedef struct {
    uint32_t id; /* canonical 0-based ID; must equal the array index */
} Cm13Variable;

typedef struct {
    uint32_t variable_index[3]; /* indices into Cm13Formula.variables */
} Cm13Clause;

typedef struct {
    Cm13Variable *variables;
    uint32_t variable_count;

    Cm13Clause *clauses;
    size_t clause_count;
} Cm13Formula;

#endif /* WANG_FORMULA_H */
