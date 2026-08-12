"""Scaffold for the future C-to-Python formula ownership boundary.

The current C parser accepts ``FILE *`` through ``cm13_formula_parse``.  This
module intentionally exposes no loader until the C ABI offers a robust entry
point for external consumers, such as a future ``cm13_formula_load_path``.

That loader will own the complete native lifetime: parse into ``Cm13Formula``,
copy every clause position into :class:`model.formula.Formula`, and call
``cm13_formula_destroy`` in a ``finally`` block.  No ctypes pointer may cross
this module's public boundary, and Python-to-C formula marshalling is outside
its scope.

``native._lib`` and centralized loading of a future ``libwang.so`` are
deliberately deferred until a second native adapter exists.
"""
