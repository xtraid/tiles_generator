#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wang/formula.h"
#include "wang/formula_parser.h"

static void check_canonical_formula(const Cm13Formula *formula)
{
    uint8_t *occurrences;

    if (formula->variable_count == 0 || formula->clauses == NULL ||
        formula->clause_count != (size_t)formula->variable_count) {
        abort();
    }
    occurrences = calloc((size_t)formula->variable_count, sizeof(*occurrences));
    if (occurrences == NULL) {
        abort();
    }
    for (size_t clause = 0; clause < formula->clause_count; clause++) {
        for (size_t field = 0; field < 3; field++) {
            uint32_t variable = formula->clauses[clause].variable_index[field];
            if (variable >= formula->variable_count || occurrences[variable] == 3) {
                free(occurrences);
                abort();
            }
            occurrences[variable]++;
        }
    }
    for (uint32_t variable = 0; variable < formula->variable_count; variable++) {
        if (occurrences[variable] != 3) {
            free(occurrences);
            abort();
        }
    }
    free(occurrences);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Cm13Formula formula = { 0 };
    Cm13ParseLocation location;
    Cm13ParseStatus status;
    FILE *input;

    /* fmemopen accepts a zero-sized buffer and presents it as immediate EOF. */
    input = fmemopen((void *)data, size, "rb");
    if (input == NULL) {
        return 0;
    }

    status = cm13_formula_parse(input, &formula, &location);
    if (status == CM13_PARSE_OK) {
        check_canonical_formula(&formula);
    } else if (formula.variable_count != 0 || formula.clauses != NULL ||
               formula.clause_count != 0) {
        abort();
    }
    cm13_formula_destroy(&formula);
    (void)fclose(input);
    return 0;
}
