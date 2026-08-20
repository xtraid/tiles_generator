"""Lazily load the shared library used by the native Python adapters."""

from ctypes import CDLL
from functools import cache
from pathlib import Path

_LIBRARY_PATH = Path(__file__).resolve().parents[2] / "build" / "lib" / "libwang.so"


@cache
def library() -> CDLL:
    return CDLL(str(_LIBRARY_PATH))
