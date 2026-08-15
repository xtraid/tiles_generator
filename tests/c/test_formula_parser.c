#include "wang/formula_parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void assert_destroyed(const Cm13Formula *formula);

static Cm13ParseStatus parse_text(const char *text, Cm13Formula *formula,
                                  Cm13ParseLocation *location)
{
    FILE *input = tmpfile();
    Cm13ParseStatus status;

    assert(input != NULL);
    assert(fwrite(text, 1, strlen(text), input) == strlen(text));
    rewind(input);
    status = cm13_formula_parse(input, formula, location);
    assert(fclose(input) == 0);
    return status;
}

static void assert_error(const char *text, Cm13ParseStatus expected_status,
                         size_t expected_line, size_t expected_column)
{
    Cm13Formula formula = {0};
    Cm13ParseLocation location = { 99, 99 };

    assert(parse_text(text, &formula, &location) == expected_status);
    assert_destroyed(&formula);
    assert(location.line == expected_line);
    assert(location.column == expected_column);
}

static void assert_destroyed(const Cm13Formula *formula)
{
    assert(formula->variable_count == 0);
    assert(formula->clauses == NULL);
    assert(formula->clause_count == 0);
}

static void test_success_and_lifetime(void)
{
    Cm13Formula formula = {0};
    Cm13ParseLocation location = { 99, 99 };
    const char *text =
        " c before\r\n"
        "\tp cm13 3 3 \t\r\n"
        "1 1 3 0\r\n"
        "c between\r\n"
        "2 2 3 0\r\n"
        "\r\n"
        "1 2 3 0\r\n"
        " c after";

    assert(parse_text(text, &formula, &location) == CM13_PARSE_OK);
    assert(location.line == 0 && location.column == 0);
    assert(formula.variable_count == 3 && formula.clause_count == 3);
    assert(formula.clauses[0].variable_index[0] == 0);
    assert(formula.clauses[0].variable_index[1] == 0);
    assert(formula.clauses[0].variable_index[2] == 2);
    assert(formula.clauses[1].variable_index[0] == 1);
    assert(formula.clauses[2].variable_index[1] == 1);
    cm13_formula_destroy(&formula);
    assert_destroyed(&formula);
    cm13_formula_destroy(&formula);
    cm13_formula_destroy(NULL);
}

static void test_minimal_lf_and_caller_stream(void)
{
    Cm13Formula formula = {0};
    FILE *input = tmpfile();

    assert(input != NULL);
    assert(fputs("p cm13 1 1\n1 1 1 0", input) >= 0);
    rewind(input);
    assert(cm13_formula_parse(input, &formula, NULL) == CM13_PARSE_OK);
    assert(formula.variable_count == 1 && formula.clause_count == 1);
    assert(formula.clauses[0].variable_index[0] == 0);
    assert(formula.clauses[0].variable_index[1] == 0);
    assert(formula.clauses[0].variable_index[2] == 0);
    assert(fseek(input, 0, SEEK_SET) == 0);
    assert(fclose(input) == 0);
    cm13_formula_destroy(&formula);
    assert_destroyed(&formula);
}

static void test_syntax_errors(void)
{
    const char *texts[] = {
        "",
        "1 1 1 0\n",
        "P cm13 1 1\n1 1 1 0\n",
        "p CM13 1 1\n1 1 1 0\n",
        "p cm13 1\n",
        "p cm13 1 1 extra\n",
        "p cm13 +1 1\n",
        "p cm13 0x1 1\n",
        "p cm13 1.0 1\n",
        "p cm13 1 1\n1 1 1\n",
        "p cm13 1 1\n1 1\n",
        "p cm13 1 1\n1 1 1 1\n",
        "p cm13 1 1\n1 1 1 0 0\n",
        "p cm13 1 1\n1 1 1 0 c nope\n",
        "p cm13 1 1\n1 1 1 0\np cm13 1 1\n"
    };
    for (size_t index = 0; index < sizeof(texts) / sizeof(texts[0]); index++) {
        Cm13Formula formula = {0};
        Cm13ParseLocation location = {0};
        assert(parse_text(texts[index], &formula, &location) == CM13_PARSE_SYNTAX_ERROR);
        assert_destroyed(&formula);
        assert(location.line != 0 && location.column != 0);
    }
}

static void test_exact_error_locations(void)
{
    assert_error("p WRONG 1 1\n", CM13_PARSE_SYNTAX_ERROR, 1, 3);
    assert_error("\tp\tWRONG 1 1\n", CM13_PARSE_SYNTAX_ERROR, 1, 4);
    assert_error("p", CM13_PARSE_SYNTAX_ERROR, 1, 2);
    assert_error("p cm13", CM13_PARSE_SYNTAX_ERROR, 1, 7);
    assert_error("p cm13 1", CM13_PARSE_SYNTAX_ERROR, 1, 9);
    assert_error("p cm13 1 1\n1 1 1", CM13_PARSE_SYNTAX_ERROR, 2, 6);
    assert_error("p cm13 1 1\n1 1 1 0\n bad\n",
                 CM13_PARSE_SYNTAX_ERROR, 3, 2);
    assert_error("p cm13 1 1\n2 1 1 0\n",
                 CM13_PARSE_DOMAIN_ERROR, 2, 1);
}

static void test_domain_errors_and_arguments(void)
{
    const char *texts[] = {
        "p cm13 0 0\n",
        "p cm13 1 0\n",
        "p cm13 0 1\n",
        "p cm13 1 2\n",
        "p cm13 4294967296 4294967296\n",
        "p cm13 1 1\n0 1 1 0\n",
        "p cm13 1 1\n2 1 1 0\n",
        "p cm13 3 3\n1 1 1 0\n1 2 2 0\n2 3 3 0\n",
        "p cm13 3 3\n1 1 2 0\n2 2 3 0\n3 3 3 0\n",
        "p cm13 1 1\n999999999999999999999999999999999999 1 1 0\n"
    };
    Cm13Formula occupied = { .variable_count = 1, .clauses = (Cm13Clause *)(void *)texts,
                             .clause_count = 1 };
    Cm13ParseLocation location = { 5, 6 };

    assert(cm13_formula_parse(NULL, &(Cm13Formula){0}, &location) == CM13_PARSE_INVALID_ARGUMENT);
    assert(location.line == 0 && location.column == 0);
    assert(cm13_formula_parse(stdin, NULL, NULL) == CM13_PARSE_INVALID_ARGUMENT);
    assert(cm13_formula_parse(stdin, &occupied, NULL) == CM13_PARSE_INVALID_ARGUMENT);
    assert(occupied.variable_count == 1);
    assert(occupied.clauses == (Cm13Clause *)(void *)texts);
    assert(occupied.clause_count == 1);
    for (size_t index = 0; index < sizeof(texts) / sizeof(texts[0]); index++) {
        Cm13Formula formula = {0};
        location = (Cm13ParseLocation){0};
        assert(parse_text(texts[index], &formula, &location) == CM13_PARSE_DOMAIN_ERROR);
        assert_destroyed(&formula);
        assert(location.line != 0 && location.column != 0);
    }
}

static void write_path_text(const char *path, const char *text)
{
    FILE *output = fopen(path, "wb");

    assert(output != NULL);
    assert(fwrite(text, 1, strlen(text), output) == strlen(text));
    assert(fclose(output) == 0);
}

static void test_load_path(void)
{
    static const char path[] = "build/tests/c/cm13_formula_load_path_test.cm13";
    static const char missing_path[] =
        "build/tests/c/cm13_formula_load_path_missing.cm13";
    static const char valid_text[] =
        "p cm13 3 3\n"
        "1 1 3 0\n"
        "2 2 3 0\n"
        "1 2 3 0\n";
    Cm13Formula formula = {0};
    Cm13ParseLocation location = { 99, 99 };
    Cm13Formula occupied = {
        .variable_count = 1,
        .clauses = (Cm13Clause *)(void *)valid_text,
        .clause_count = 1
    };

    (void)remove(path);
    (void)remove(missing_path);
    write_path_text(path, valid_text);

    assert(cm13_formula_load_path(path, &formula, &location) == CM13_PARSE_OK);
    assert(location.line == 0 && location.column == 0);
    assert(formula.variable_count == 3 && formula.clause_count == 3);
    assert(formula.clauses[0].variable_index[0] == 0);
    assert(formula.clauses[1].variable_index[1] == 1);
    assert(formula.clauses[2].variable_index[2] == 2);
    cm13_formula_destroy(&formula);
    assert_destroyed(&formula);

    write_path_text(path, "p WRONG 1 1\n");
    location = (Cm13ParseLocation){ 99, 99 };
    assert(cm13_formula_load_path(path, &formula, &location) ==
           CM13_PARSE_SYNTAX_ERROR);
    assert_destroyed(&formula);
    assert(location.line == 1 && location.column == 3);

    location = (Cm13ParseLocation){ 99, 99 };
    assert(cm13_formula_load_path(missing_path, &formula, &location) ==
           CM13_PARSE_IO_ERROR);
    assert_destroyed(&formula);
    assert(location.line == 0 && location.column == 0);

    location = (Cm13ParseLocation){ 99, 99 };
    assert(cm13_formula_load_path(NULL, &formula, &location) ==
           CM13_PARSE_INVALID_ARGUMENT);
    assert(location.line == 0 && location.column == 0);
    assert(cm13_formula_load_path(path, NULL, NULL) ==
           CM13_PARSE_INVALID_ARGUMENT);
    assert(cm13_formula_load_path(path, &occupied, NULL) ==
           CM13_PARSE_INVALID_ARGUMENT);
    assert(occupied.variable_count == 1);
    assert(occupied.clauses == (Cm13Clause *)(void *)valid_text);
    assert(occupied.clause_count == 1);

    assert(remove(path) == 0);
}

int main(void)
{
    test_success_and_lifetime();
    test_minimal_lf_and_caller_stream();
    test_syntax_errors();
    test_exact_error_locations();
    test_domain_errors_and_arguments();
    test_load_path();
    return 0;
}
