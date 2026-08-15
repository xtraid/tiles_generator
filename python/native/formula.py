"""Scaffold for the C-to-Python formula ownership boundary.

The C ABI now offers ``cm13_formula_load_path`` as a robust external entry
point.  This module intentionally exposes no loader until the build produces a
shared library that Python can load.

That loader will own the complete native lifetime: parse into ``Cm13Formula``,
copy every clause position into :class:`model.formula.Formula`, and call
``cm13_formula_destroy`` in a ``finally`` block.  No ctypes pointer may cross
this module's public boundary, and Python-to-C formula marshalling is outside
its scope.

``native._lib`` and centralized loading of ``libwang.so`` remain deliberately
deferred until a second native adapter exists.
"""
