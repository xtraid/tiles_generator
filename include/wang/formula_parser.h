#ifndef WANG_FORMULA_PARSER_H
#define WANG_FORMULA_PARSER_H

#include <stddef.h>
#include <stdio.h>

#include "wang/formula.h"

typedef enum {
    CM13_PARSE_OK = 0,
    CM13_PARSE_INVALID_ARGUMENT,
    CM13_PARSE_IO_ERROR,
    CM13_PARSE_SYNTAX_ERROR,
    CM13_PARSE_DOMAIN_ERROR,
    CM13_PARSE_OUT_OF_MEMORY
} Cm13ParseStatus;

typedef struct {
    size_t line;
    size_t column;
} Cm13ParseLocation;

Cm13ParseStatus cm13_formula_parse(
    FILE *input,
    Cm13Formula *out_formula,
    Cm13ParseLocation *out_error_location
);

#endif /* WANG_FORMULA_PARSER_H */
