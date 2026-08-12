#include "wang/solver.h"
#include "wang/tile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static ColorId signal_color(unsigned signal)
{
    assert(signal <= 1);
    return signal == 0 ? COLOR_0 : COLOR_1;
}

static void activate_rectangle(Region *region, int32_t width, int32_t height)
{
    assert(region_init(region, width, height));
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            assert(region_set_active(region, x, y, true));
        }
    }
}

static TileId solved_tile_at(
    const Region *region,
    const WangSolveResult *result,
    int32_t x,
    int32_t y
)
{
    uint32_t domain = result->domains[region_index(region, x, y)];
    assert(domain != 0 && (domain & (domain - UINT32_C(1))) == 0);

    TileId tile = 0;
    while ((domain & UINT32_C(1)) == 0) {
        domain >>= 1;
        ++tile;
    }
    assert(tile < TILE_COUNT);
    return tile;
}

static void solve_forwarder_band(unsigned signal)
{
    Region region = {0};
    WangSolveResult result = {0};
    activate_rectangle(&region, 2, 1);

    for (int32_t x = 0; x < region.width; ++x) {
        assert(region_set_boundary(&region, x, 0, N, COLOR_B));
        assert(region_set_boundary(&region, x, 0, S, COLOR_B));
    }
    assert(region_set_boundary(&region, 0, 0, W, signal_color(signal)));
    assert(region_set_boundary(&region, 1, 0, E, signal_color(signal)));

    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);
    const TileId expected = signal == 0 ? TILE_F0 : TILE_F1;
    assert(solved_tile_at(&region, &result, 0, 0) == expected);
    assert(solved_tile_at(&region, &result, 1, 0) == expected);
    assert(TILESET[solved_tile_at(&region, &result, 1, 0)].edge[E] ==
           signal_color(signal));

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void solve_left_anchor(unsigned signal)
{
    Region region = {0};
    WangSolveResult result = {0};
    activate_rectangle(&region, 1, 1);

    assert(region_set_boundary(&region, 0, 0, N, COLOR_L));
    assert(region_set_boundary(&region, 0, 0, S, COLOR_L));
    assert(region_set_boundary(&region, 0, 0, W, signal_color(signal)));

    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);
    const TileId tile = solved_tile_at(&region, &result, 0, 0);
    assert(tile == (signal == 0 ? TILE_L0 : TILE_L1));
    assert(TILESET[tile].edge[E] == signal_color(signal));

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void solve_right_anchor(unsigned signal)
{
    Region region = {0};
    WangSolveResult result = {0};
    activate_rectangle(&region, 2, 1);

    assert(region_set_boundary(&region, 0, 0, N, COLOR_B));
    assert(region_set_boundary(&region, 0, 0, S, COLOR_R));
    assert(region_set_boundary(&region, 0, 0, W, signal_color(signal)));
    assert(region_set_boundary(&region, 1, 0, N, COLOR_R));
    assert(region_set_boundary(&region, 1, 0, S, COLOR_B));

    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);
    const TileId left = solved_tile_at(&region, &result, 0, 0);
    const TileId right = solved_tile_at(&region, &result, 1, 0);
    assert(left == (signal == 0 ? TILE_R0_LEFT : TILE_R1_LEFT));
    assert(right == (signal == 0 ? TILE_R0_RIGHT : TILE_R1_RIGHT));
    assert(TILESET[right].edge[E] == signal_color(signal));

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void solve_crossover(unsigned top_input, unsigned bottom_input)
{
    Region region = {0};
    WangSolveResult result = {0};
    activate_rectangle(&region, 1, 2);

    assert(region_set_boundary(&region, 0, 0, N, COLOR_R));
    assert(region_set_boundary(
        &region, 0, 0, W, signal_color(top_input)
    ));
    assert(region_set_boundary(&region, 0, 1, S, COLOR_L));
    assert(region_set_boundary(
        &region, 0, 1, W, signal_color(bottom_input)
    ));

    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);
    const TileId top = solved_tile_at(&region, &result, 0, 0);
    const TileId bottom = solved_tile_at(&region, &result, 0, 1);
    const unsigned output_pair = 2u * bottom_input + top_input;
    const TileId expected_top = (TileId)(TILE_X00_TOP + 2u * output_pair);

    assert(top == expected_top);
    assert(bottom == (TileId)(expected_top + 1u));
    assert(TILESET[top].edge[E] == signal_color(bottom_input));
    assert(TILESET[bottom].edge[E] == signal_color(top_input));

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

int main(void)
{
    for (unsigned signal = 0; signal <= 1; ++signal) {
        solve_forwarder_band(signal);
        solve_left_anchor(signal);
        solve_right_anchor(signal);
    }

    for (unsigned top = 0; top <= 1; ++top) {
        for (unsigned bottom = 0; bottom <= 1; ++bottom) {
            solve_crossover(top, bottom);
        }
    }

    puts("test_yang_zhang_gadgets: OK");
    return 0;
}
