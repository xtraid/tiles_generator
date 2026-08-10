# Yang-Zhang reduction implementation notes

## Document role

This is the living record of reduction-specific conventions adopted by the
implementation. It documents indexing, geometry, project adaptations, and the
correctness obligations that must become regression tests.

It is not the repository roadmap or a second architecture specification. The
public headers and tests define implemented behavior; the architecture PDF
describes the broader future design.

This document records both:

1. conventions inherited from Yang-Zhang;
2. explicit conventions introduced by this project.

Those two categories must not be confused.

Primary reference:

> Chao Yang, Zhujun Zhang,  
> *NP-completeness of Tiling Finite Simply Connected Regions with a Fixed Set of Wang Tiles*,  
> arXiv:2405.01017v2 (2024).

## 1. Signal height

For `n` variables, the construction has three signal rows per variable and one redundant
row between adjacent variable groups:

```text
height = 3n + (n - 1) = 4n - 1
```

For `n = 3`, the logical height is 11.

## 2. Adjacent-swap indexing

The paper uses 1-based rows.

If a paper crossover has width `w`, it swaps rows `w` and `w + 1`.

The implementation stores:

```text
AdjacentSwap.row = w - 1
```

therefore:

```text
crossover_block_width = AdjacentSwap.row + 1
```

This conversion must remain explicit in tests because an off-by-one error here changes
the geometry of every crossover gadget.

## 3. Paper example

For the paper's three-variable example, the adjacent-transposition sequence is:

```text
swap(8), swap(7), swap(6), swap(5), swap(4), swap(3),
swap(4), swap(5), swap(6),
swap(9), swap(8), swap(7), swap(9), swap(8)
```

The C zero-based sequence is:

```text
7, 6, 5, 4, 3, 2, 3, 4, 5, 8, 7, 6, 8, 7
```

The corresponding crossover widths sum to:

```text
89
```

This sequence is kept as a golden regression test.

## 4. Project convention: explicit forwarder bands

The project deliberately keeps:

```text
2 forwarder columns before the crossover chain
2 forwarder columns after the crossover chain
```

The purpose is architectural and diagnostic:

- make the point where variable signals enter the crossover chain explicit;
- make the point where reordered signals leave the chain explicit;
- separate gadget boundaries visually;
- provide a simple neutral propagation band for debugging and rendering.

These `2 + 2` columns are a **project convention**. They must not be described as if the
paper required those exact standalone bands.

The coarse layout is therefore:

```text
[V] [FF] [ crossover chain ] [FF] [clause area]
 1   2                              2       2
```

and:

```text
width =
    1
  + 2
  + sum(crossover widths)
  + 2
  + 2
```

For the paper example:

```text
width = 1 + 2 + 89 + 2 + 2 = 96
```

## 5. Correctness obligation introduced by the convention

Because the forwarder bands are an adaptation made by this project, later region-level
tests must establish that they are neutral signal extensions.

For each signal value admitted by the construction, the band must:

- propagate the same value from its input edge to its output edge;
- introduce no additional signal choice;
- preserve redundant rows as required;
- not alter SAT/UNSAT of the constructed tiling instance.

The concrete `Region` builder now has black-box tests for the complete active mask,
boundary encoding, and exact swap trace. Once a reference solver exists, small
gadget-level tests must additionally establish the forced-tiling behavior of the
forwarder bands and isolated crossover blocks.

## 6. Dimension calculator and completed builder

`yang_zhang_compute_dimensions()` remains only a coarse dimension calculator.

It computes:

- total height;
- total width.

The adjacent-swap sequence remains owned by the permutation layer. The dimension
calculator only reads it for the duration of the call.

The public `yang_zhang_build()` composes this calculator with the remaining
reduction stages:

- validation of the canonical in-memory formula;
- unique occurrence and redundant signal tokens;
- source/target permutation construction;
- a fully active rectangular `Region`;
- complete exterior boundary colors;
- variable, clause, and isolated crossover boundary markers;
- transactional transfer of the exact adjacent-swap trace.

The builder deliberately does not parse text, solve the formula, choose tiles, or
store gadget annotations in `Region`. Its full implementation contract is recorded
in `yang_zhang_builder_design.md`; public headers and black-box tests are
authoritative for implemented behavior.
