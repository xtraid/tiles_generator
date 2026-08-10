#include "wang/permutation.h"
#include "wang/yang_zhang.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

static uint32_t random_state = UINT32_C(0x7a6b5c4d);

static uint32_t next_random(void)
{
    random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    return random_state;
}

static SignalToken make_token(uint32_t token_id)
{
    if (token_id % 4u == 3u) {
        return (SignalToken){
            .kind = SIGNAL_REDUNDANT,
            .token_id = token_id,
        };
    }

    return (SignalToken){
        .kind = SIGNAL_VARIABLE,
        .token_id = token_id,
        .variable = token_id / 3u,
        .occurrence = (uint8_t)(token_id % 3u),
    };
}

static bool tokens_equal(const SignalToken *a, const SignalToken *b)
{
    if (a->kind != b->kind || a->token_id != b->token_id) {
        return false;
    }

    return a->kind == SIGNAL_REDUNDANT ||
        (a->variable == b->variable && a->occurrence == b->occurrence);
}

static void assert_sequences_equal(
    const SignalToken *actual,
    const SignalToken *expected,
    size_t signal_count
)
{
    for (size_t i = 0; i < signal_count; ++i) {
        assert(tokens_equal(&actual[i], &expected[i]));
    }
}

static void shuffle(SignalToken *signals, size_t signal_count)
{
    for (size_t i = signal_count; i > 1; --i) {
        const size_t other = (size_t)next_random() % i;
        const SignalToken tmp = signals[i - 1];
        signals[i - 1] = signals[other];
        signals[other] = tmp;
    }
}

static void assert_build_failure(
    const SignalToken *source,
    const SignalToken *target,
    size_t signal_count
)
{
    AdjacentSwap sentinel = { .row = 123u };
    AdjacentSwap *swaps = &sentinel;
    size_t swap_count = 456u;

    assert(!yang_zhang_permutation_build(
        source,
        target,
        signal_count,
        &swaps,
        &swap_count
    ));
    assert(swaps == NULL);
    assert(swap_count == 0);
}

static void test_known_permutation(void)
{
    const SignalToken source[] = {
        make_token(0), make_token(1), make_token(2), make_token(3)
    };
    const SignalToken target[] = {
        make_token(2), make_token(0), make_token(3), make_token(1)
    };
    const AdjacentSwap expected_swaps[] = {
        { .row = 1 }, { .row = 0 }, { .row = 2 }
    };
    SignalToken source_before[ARRAY_COUNT(source)];
    SignalToken target_before[ARRAY_COUNT(target)];
    SignalToken actual[ARRAY_COUNT(source)];
    AdjacentSwap *swaps = NULL;
    size_t swap_count = 0;

    memcpy(source_before, source, sizeof(source));
    memcpy(target_before, target, sizeof(target));
    memcpy(actual, source, sizeof(source));

    assert(yang_zhang_permutation_build(
        source,
        target,
        ARRAY_COUNT(source),
        &swaps,
        &swap_count
    ));
    assert(swap_count == ARRAY_COUNT(expected_swaps));

    for (size_t i = 0; i < swap_count; ++i) {
        assert(swaps[i].row == expected_swaps[i].row);
    }

    assert_sequences_equal(source, source_before, ARRAY_COUNT(source));
    assert_sequences_equal(target, target_before, ARRAY_COUNT(target));

    assert(yang_zhang_permutation_apply(
        actual,
        ARRAY_COUNT(actual),
        swaps,
        swap_count
    ));
    assert_sequences_equal(actual, target, ARRAY_COUNT(target));

    free(swaps);
}

static void test_identity_and_empty_permutations(void)
{
    const SignalToken signals[] = {
        make_token(0), make_token(1), make_token(2)
    };
    AdjacentSwap sentinel = { .row = 42u };
    AdjacentSwap *swaps = &sentinel;
    size_t swap_count = 42u;

    assert(yang_zhang_permutation_build(
        signals,
        signals,
        ARRAY_COUNT(signals),
        &swaps,
        &swap_count
    ));
    assert(swaps == NULL);
    assert(swap_count == 0);

    swaps = &sentinel;
    swap_count = 42u;
    assert(yang_zhang_permutation_build(
        NULL,
        NULL,
        0,
        &swaps,
        &swap_count
    ));
    assert(swaps == NULL);
    assert(swap_count == 0);

    assert(yang_zhang_permutation_apply(NULL, 0, NULL, 0));
}

static void test_build_rejects_invalid_input(void)
{
    const SignalToken valid[] = {
        make_token(0), make_token(1), make_token(2)
    };
    SignalToken invalid[ARRAY_COUNT(valid)];
    AdjacentSwap *swaps = NULL;
    size_t swap_count = 0;

    assert(!yang_zhang_permutation_build(
        valid,
        valid,
        ARRAY_COUNT(valid),
        NULL,
        &swap_count
    ));
    assert(!yang_zhang_permutation_build(
        valid,
        valid,
        ARRAY_COUNT(valid),
        &swaps,
        NULL
    ));

    assert_build_failure(NULL, valid, ARRAY_COUNT(valid));
    assert_build_failure(valid, NULL, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[1].kind = (SignalKind)99;
    assert_build_failure(invalid, valid, ARRAY_COUNT(valid));
    assert_build_failure(valid, invalid, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[1].token_id = invalid[0].token_id;
    assert_build_failure(invalid, valid, ARRAY_COUNT(valid));
    assert_build_failure(valid, invalid, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[1].token_id = 99u;
    assert_build_failure(valid, invalid, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[1].variable += 1u;
    assert_build_failure(valid, invalid, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[1].occurrence += 1u;
    assert_build_failure(valid, invalid, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[1].kind = SIGNAL_REDUNDANT;
    assert_build_failure(valid, invalid, ARRAY_COUNT(valid));

    memcpy(invalid, valid, sizeof(valid));
    invalid[2].occurrence = 3u;
    assert_build_failure(invalid, invalid, ARRAY_COUNT(valid));
}

static void test_apply_is_atomic_on_invalid_swap(void)
{
    SignalToken signals[] = {
        make_token(0), make_token(1), make_token(2)
    };
    SignalToken before[ARRAY_COUNT(signals)];
    const AdjacentSwap swaps[] = {
        { .row = 0 },
        { .row = 2 }
    };
    const AdjacentSwap one_swap[] = {
        { .row = 0 }
    };

    memcpy(before, signals, sizeof(signals));

    assert(!yang_zhang_permutation_apply(
        signals,
        ARRAY_COUNT(signals),
        swaps,
        ARRAY_COUNT(swaps)
    ));
    assert_sequences_equal(signals, before, ARRAY_COUNT(signals));

    assert(!yang_zhang_permutation_apply(NULL, 1, NULL, 0));
    assert(!yang_zhang_permutation_apply(
        signals,
        ARRAY_COUNT(signals),
        NULL,
        1
    ));
    assert(!yang_zhang_permutation_apply(NULL, 0, one_swap, 1));
}

static void test_reverse_permutation_grows_swap_list(void)
{
    enum { SIGNAL_COUNT = 64 };
    SignalToken source[SIGNAL_COUNT];
    SignalToken target[SIGNAL_COUNT];
    SignalToken actual[SIGNAL_COUNT];
    AdjacentSwap *swaps = NULL;
    size_t swap_count = 0;

    for (size_t i = 0; i < SIGNAL_COUNT; ++i) {
        source[i] = make_token((uint32_t)i);
        target[SIGNAL_COUNT - 1u - i] = source[i];
    }
    memcpy(actual, source, sizeof(source));

    assert(yang_zhang_permutation_build(
        source,
        target,
        SIGNAL_COUNT,
        &swaps,
        &swap_count
    ));
    assert(swap_count ==
        ((size_t)SIGNAL_COUNT * (SIGNAL_COUNT - 1u)) / 2u);
    assert(yang_zhang_permutation_apply(
        actual,
        SIGNAL_COUNT,
        swaps,
        swap_count
    ));
    assert_sequences_equal(actual, target, SIGNAL_COUNT);

    free(swaps);
}

static void test_random_permutations_and_dimension_interaction(void)
{
    enum { MAX_SIGNALS = 31, ITERATIONS = 500 };
    SignalToken source[MAX_SIGNALS];
    SignalToken target[MAX_SIGNALS];
    SignalToken actual[MAX_SIGNALS];

    for (size_t iteration = 0; iteration < ITERATIONS; ++iteration) {
        const uint32_t variable_count = 1u + next_random() % 8u;
        const size_t signal_count = 4u * (size_t)variable_count - 1u;
        AdjacentSwap *swaps = NULL;
        size_t swap_count = 0;
        uint64_t expected_width =
            YANG_ZHANG_VARIABLE_WIDTH +
            YANG_ZHANG_LEFT_FORWARD_WIDTH +
            YANG_ZHANG_RIGHT_FORWARD_WIDTH +
            YANG_ZHANG_CLAUSE_WIDTH;
        int32_t height = 0;
        int32_t width = 0;

        assert(signal_count <= MAX_SIGNALS);

        for (size_t i = 0; i < signal_count; ++i) {
            source[i] = make_token((uint32_t)i);
        }
        shuffle(source, signal_count);
        memcpy(target, source, signal_count * sizeof(*target));
        shuffle(target, signal_count);
        memcpy(actual, source, signal_count * sizeof(*actual));

        assert(yang_zhang_permutation_build(
            source,
            target,
            signal_count,
            &swaps,
            &swap_count
        ));

        for (size_t i = 0; i < swap_count; ++i) {
            assert((size_t)swaps[i].row < signal_count - 1u);
            expected_width += (uint64_t)swaps[i].row + 1u;
        }

        assert(yang_zhang_permutation_apply(
            actual,
            signal_count,
            swaps,
            swap_count
        ));
        assert_sequences_equal(actual, target, signal_count);

        assert(yang_zhang_compute_dimensions(
            variable_count,
            swaps,
            swap_count,
            &height,
            &width
        ));
        assert(height == (int32_t)signal_count);
        assert((uint64_t)width == expected_width);

        free(swaps);
    }
}

int main(void)
{
    test_known_permutation();
    test_identity_and_empty_permutations();
    test_build_rejects_invalid_input();
    test_apply_is_atomic_on_invalid_swap();
    test_reverse_permutation_grows_swap_list();
    test_random_permutations_and_dimension_interaction();

    puts("test_permutation: OK");
    return 0;
}
