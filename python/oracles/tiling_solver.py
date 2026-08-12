"""Scaffold for the future Wang Z3 oracle.

Its input will be a pure Python Region copied from the native representation,
together with the canonical 23-tile set.  It will not accept a Boolean formula,
parse input, call the Yang-Zhang builder, or duplicate reduction logic.
Its result will distinguish SAT, UNSAT, and UNKNOWN, with a tiling witness only
for SAT.

No callable API is defined yet because a pure Python Region contract and its
``native/region.py`` adapter do not exist.
"""
