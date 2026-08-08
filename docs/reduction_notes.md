# Yang-Zhang reduction implementation notes

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

In practice this should become a small gadget-level regression test once the concrete
`Region` builder exists.

## 6. Current boundary of `YangZhangLayout`

`YangZhangLayout` is only a coarse layout planner.

It currently owns:

- total height;
- total width;
- a private copy of the adjacent-swap sequence.

It does not yet implement:

- SAT formula parsing;
- unique occurrence tokens;
- source/target permutation construction;
- concrete cells;
- boundary colors;
- variable/clause/crossover rasterization.

Those stages should stay separate and independently testable.
