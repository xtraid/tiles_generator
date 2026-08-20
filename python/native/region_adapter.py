"""Copy native Wang regions into immutable Python storage."""

from ctypes import (
    CDLL,
    POINTER,
    Structure,
    byref,
    c_bool,
    c_int32,
    c_size_t,
    c_uint8,
    c_void_p,
)
from functools import cache

from model.region import Region
from native._lib import library
from native.formula_adapter import _Cm13Formula


_DIRECTION_COUNT = 4


class _RegionCell(Structure):
    _fields_ = [
        ("active", c_bool),
        ("boundary", c_uint8 * _DIRECTION_COUNT),
    ]


class _Region(Structure):
    _fields_ = [
        ("width", c_int32),
        ("height", c_int32),
        ("cell_count", c_size_t),
        ("cells", POINTER(_RegionCell)),
    ]


class _YangZhangReduction(Structure):
    _fields_ = [
        ("region", _Region),
        ("swaps", c_void_p),
        ("swap_count", c_size_t),
    ]


class RegionBuildError(RuntimeError):
    """The native Yang–Zhang builder could not construct a region."""


@cache
def _region_library() -> CDLL:
    lib = library()
    lib.yang_zhang_build.argtypes = [
        POINTER(_Cm13Formula),
        POINTER(_YangZhangReduction),
    ]
    lib.yang_zhang_build.restype = c_bool
    lib.yang_zhang_reduction_destroy.argtypes = [
        POINTER(_YangZhangReduction)
    ]
    lib.yang_zhang_reduction_destroy.restype = None
    return lib


def _copy_region(native_region: _Region) -> Region:
    width = int(native_region.width)
    height = int(native_region.height)
    cell_count = int(native_region.cell_count)
    if (
        width <= 0
        or height <= 0
        or cell_count != width * height
        or not native_region.cells
    ):
        raise RuntimeError("invalid native region metadata")

    active = tuple(
        bool(native_region.cells[index].active)
        for index in range(cell_count)
    )
    boundary = tuple(
        (
            int(native_region.cells[index].boundary[0]),
            int(native_region.cells[index].boundary[1]),
            int(native_region.cells[index].boundary[2]),
            int(native_region.cells[index].boundary[3]),
        )
        for index in range(cell_count)
    )

    return Region(
        width=width,
        height=height,
        active=active,
        boundary=boundary,
    )


def _build_region(native_formula: _Cm13Formula) -> Region:
    native_reduction = _YangZhangReduction()
    lib = _region_library()
    try:
        if not lib.yang_zhang_build(
            byref(native_formula),
            byref(native_reduction),
        ):
            raise RegionBuildError("could not build Yang-Zhang region")
        return _copy_region(native_reduction.region)
    finally:
        lib.yang_zhang_reduction_destroy(byref(native_reduction))
