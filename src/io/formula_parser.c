#include "wang/formula_parser.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    size_t line;
    size_t next_line;
    size_t next_column;
} Cm13LineReader;

static void cm13_set_error(Cm13ParseLocation *location, size_t line, size_t column)
{
    if (location != NULL) {
        location->line = line;
        location->column = column;
    }
}

static int cm13_read_line(FILE *input, Cm13LineReader *reader)
{
    int character;

    reader->length = 0;
    reader->line = reader->next_line;
    while ((character = fgetc(input)) != EOF) {
        if (character == '\n') {
            if (reader->length != 0 && reader->data[reader->length - 1] == '\r') {
                reader->length--;
            }
            reader->next_line++;
            reader->next_column = 1;
            return 1;
        }
        if (reader->length == reader->capacity) {
            size_t capacity = reader->capacity == 0 ? 128 : reader->capacity * 2;
            char *data;
            if (capacity < reader->capacity) {
                return -3;
            }
            data = realloc(reader->data, capacity);
            if (data == NULL) {
                return -1;
            }
            reader->data = data;
            reader->capacity = capacity;
        }
        reader->data[reader->length++] = (char)character;
        reader->next_column++;
    }
    if (ferror(input)) {
        return -2;
    }
    return reader->length == 0 ? 0 : 1;
}

static int cm13_horizontal_space(char character)
{
    return character == ' ' || character == '\t';
}

static void cm13_skip_space(const Cm13LineReader *reader, size_t *position)
{
    while (*position < reader->length && cm13_horizontal_space(reader->data[*position])) {
        (*position)++;
    }
}

static int cm13_next_token(const Cm13LineReader *reader, size_t *position,
                           size_t *start, size_t *length)
{
    cm13_skip_space(reader, position);
    if (*position == reader->length) {
        return 0;
    }
    *start = *position;
    while (*position < reader->length && !cm13_horizontal_space(reader->data[*position])) {
        (*position)++;
    }
    *length = *position - *start;
    return 1;
}

static int cm13_token_equals(const Cm13LineReader *reader, size_t start,
                             size_t length, const char *word)
{
    size_t index = 0;
    while (index < length && word[index] != '\0') {
        if (reader->data[start + index] != word[index]) {
            return 0;
        }
        index++;
    }
    return index == length && word[index] == '\0';
}

/* Returns 0 for non-decimal syntax, 1 for a value, and 2 for overflow. */
static int cm13_decimal(const Cm13LineReader *reader, size_t start, size_t length,
                        uintmax_t maximum, uintmax_t *value)
{
    uintmax_t result = 0;
    size_t index;
    if (length == 0) {
        return 0;
    }
    for (index = 0; index < length; index++) {
        unsigned char character = (unsigned char)reader->data[start + index];
        uintmax_t digit;
        if (character < '0' || character > '9') {
            return 0;
        }
        digit = (uintmax_t)(character - '0');
        if (result > (maximum - digit) / 10) {
            return 2;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return 1;
}

static Cm13ParseStatus cm13_failure(Cm13Formula *formula, Cm13Clause *clauses,
                                    uint32_t *occurrences, Cm13ParseStatus status)
{
    free(occurrences);
    free(clauses);
    formula->variable_count = 0;
    formula->clauses = NULL;
    formula->clause_count = 0;
    return status;
}

static int cm13_allocation_overflow(size_t count, size_t element_size)
{
    return count != 0 && element_size > SIZE_MAX / count;
}

Cm13ParseStatus cm13_formula_parse(FILE *input, Cm13Formula *out_formula,
                                    Cm13ParseLocation *out_error_location)
{
    Cm13LineReader reader = { .line = 1, .next_line = 1, .next_column = 1 };
    Cm13Clause *clauses = NULL;
    uint32_t *occurrences = NULL;
    uint32_t variable_count = 0;
    size_t clause_count = 0;
    size_t clauses_read = 0;
    int have_header = 0;
    int read_result;

    cm13_set_error(out_error_location, 0, 0);
    if (input == NULL || out_formula == NULL || out_formula->variable_count != 0 ||
        out_formula->clauses != NULL || out_formula->clause_count != 0) {
        return CM13_PARSE_INVALID_ARGUMENT;
    }

    while ((read_result = cm13_read_line(input, &reader)) == 1) {
        size_t position = 0;
        size_t start;
        size_t length;
        size_t variable_count_start;
        size_t clause_count_start;
        uintmax_t value;
        int decimal_result;

        cm13_skip_space(&reader, &position);
        if (position == reader.length || reader.data[position] == 'c') {
            continue;
        }
        if (!have_header) {
            if (!cm13_next_token(&reader, &position, &start, &length) ||
                !cm13_token_equals(&reader, start, length, "p")) {
                cm13_set_error(out_error_location, reader.line, start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            if (!cm13_next_token(&reader, &position, &start, &length)) {
                cm13_set_error(out_error_location, reader.line, reader.length + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            if (!cm13_token_equals(&reader, start, length, "cm13")) {
                cm13_set_error(out_error_location, reader.line, start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            if (!cm13_next_token(&reader, &position, &start, &length)) {
                cm13_set_error(out_error_location, reader.line, reader.length + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            decimal_result = cm13_decimal(&reader, start, length, UINT32_MAX, &value);
            if (decimal_result != 1) {
                cm13_set_error(out_error_location, reader.line, start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences,
                    decimal_result == 2 ? CM13_PARSE_DOMAIN_ERROR : CM13_PARSE_SYNTAX_ERROR);
            }
            variable_count_start = start;
            variable_count = (uint32_t)value;
            if (!cm13_next_token(&reader, &position, &start, &length)) {
                cm13_set_error(out_error_location, reader.line, reader.length + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            decimal_result = cm13_decimal(&reader, start, length, SIZE_MAX, &value);
            if (decimal_result != 1) {
                cm13_set_error(out_error_location, reader.line, start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences,
                    decimal_result == 2 ? CM13_PARSE_DOMAIN_ERROR : CM13_PARSE_SYNTAX_ERROR);
            }
            clause_count_start = start;
            clause_count = (size_t)value;
            if (cm13_next_token(&reader, &position, &start, &length)) {
                cm13_set_error(out_error_location, reader.line, start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            if (variable_count == 0 || clause_count == 0 || clause_count != (size_t)variable_count ||
                cm13_allocation_overflow(clause_count, sizeof(*clauses)) ||
                cm13_allocation_overflow((size_t)variable_count, sizeof(*occurrences))) {
                cm13_set_error(out_error_location, reader.line,
                    (variable_count == 0 || cm13_allocation_overflow(
                        (size_t)variable_count, sizeof(*occurrences)))
                        ? variable_count_start + 1 : clause_count_start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_DOMAIN_ERROR);
            }
            clauses = malloc(clause_count * sizeof(*clauses));
            occurrences = calloc((size_t)variable_count, sizeof(*occurrences));
            if (clauses == NULL || occurrences == NULL) {
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_OUT_OF_MEMORY);
            }
            have_header = 1;
            continue;
        }
        if (clauses_read == clause_count) {
            cm13_set_error(out_error_location, reader.line, position + 1);
            free(reader.data);
            return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
        }
        for (size_t field = 0; field < 4; field++) {
            if (!cm13_next_token(&reader, &position, &start, &length)) {
                cm13_set_error(out_error_location, reader.line, reader.length + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
            }
            decimal_result = cm13_decimal(&reader, start, length, UINT32_MAX, &value);
            if (decimal_result != 1) {
                cm13_set_error(out_error_location, reader.line, start + 1);
                free(reader.data);
                return cm13_failure(out_formula, clauses, occurrences,
                    decimal_result == 2 ? CM13_PARSE_DOMAIN_ERROR : CM13_PARSE_SYNTAX_ERROR);
            }
            if (field == 3) {
                if (value != 0) {
                    cm13_set_error(out_error_location, reader.line, start + 1);
                    free(reader.data);
                    return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
                }
            } else {
                if (value == 0 || value > variable_count) {
                    cm13_set_error(out_error_location, reader.line, start + 1);
                    free(reader.data);
                    return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_DOMAIN_ERROR);
                }
                clauses[clauses_read].variable_index[field] = (uint32_t)value - 1;
                if (occurrences[value - 1] < 4) {
                    occurrences[value - 1]++;
                }
            }
        }
        if (cm13_next_token(&reader, &position, &start, &length)) {
            cm13_set_error(out_error_location, reader.line, start + 1);
            free(reader.data);
            return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
        }
        clauses_read++;
    }
    free(reader.data);
    if (read_result == -1) {
        return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_OUT_OF_MEMORY);
    }
    if (read_result == -3) {
        return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_DOMAIN_ERROR);
    }
    if (read_result == -2) {
        return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_IO_ERROR);
    }
    if (!have_header || clauses_read != clause_count) {
        cm13_set_error(out_error_location, reader.next_line, reader.next_column);
        return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_SYNTAX_ERROR);
    }
    for (size_t index = 0; index < (size_t)variable_count; index++) {
        if (occurrences[index] != 3) {
            cm13_set_error(out_error_location, reader.next_line, reader.next_column);
            return cm13_failure(out_formula, clauses, occurrences, CM13_PARSE_DOMAIN_ERROR);
        }
    }
    free(occurrences);
    out_formula->variable_count = variable_count;
    out_formula->clauses = clauses;
    out_formula->clause_count = clause_count;
    return CM13_PARSE_OK;
}

Cm13ParseStatus cm13_formula_load_path(const char *path, Cm13Formula *out_formula,
                                        Cm13ParseLocation *out_error_location)
{
    FILE *input;
    Cm13ParseStatus status;

    cm13_set_error(out_error_location, 0, 0);
    if (path == NULL || out_formula == NULL || out_formula->variable_count != 0 ||
        out_formula->clauses != NULL || out_formula->clause_count != 0) {
        return CM13_PARSE_INVALID_ARGUMENT;
    }

    input = fopen(path, "rb");
    if (input == NULL) {
        return CM13_PARSE_IO_ERROR;
    }

    status = cm13_formula_parse(input, out_formula, out_error_location);
    if (fclose(input) != 0 && status == CM13_PARSE_OK) {
        cm13_formula_destroy(out_formula);
        cm13_set_error(out_error_location, 0, 0);
        return CM13_PARSE_IO_ERROR;
    }
    return status;
}
