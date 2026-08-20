from dataclasses import FrozenInstanceError
import unittest

from model.region import Region


class RegionModelTests(unittest.TestCase):
    def test_is_immutable_and_preserves_dense_storage(self) -> None:
        region = Region(
            width=1,
            height=1,
            active=(True,),
            boundary=((0, 1, 2, 3),),
        )

        self.assertEqual(region.active, (True,))
        self.assertEqual(region.boundary, ((0, 1, 2, 3),))
        with self.assertRaises(FrozenInstanceError):
            region.width = 2  # type: ignore[misc]

    def test_rejects_non_positive_dimensions(self) -> None:
        for width, height in ((0, 1), (1, 0), (-1, 1), (1, -1)):
            with self.subTest(width=width, height=height):
                with self.assertRaises(ValueError):
                    Region(
                        width=width,
                        height=height,
                        active=(),
                        boundary=(),
                    )

    def test_requires_dense_storage_matching_area(self) -> None:
        no_boundary = (255, 255, 255, 255)
        invalid_storage = (
            ((True,), (no_boundary, no_boundary)),
            ((True, True), (no_boundary,)),
        )

        for active, boundary in invalid_storage:
            with self.subTest(active=active, boundary=boundary):
                with self.assertRaises(ValueError):
                    Region(
                        width=2,
                        height=1,
                        active=active,
                        boundary=boundary,
                    )

    def test_rejects_mutable_storage(self) -> None:
        no_boundary = (255, 255, 255, 255)
        mutable_storage = (
            ([True], (no_boundary,)),
            ((True,), [no_boundary]),
        )

        for active, boundary in mutable_storage:
            with self.subTest(active=active, boundary=boundary):
                with self.assertRaises(TypeError):
                    Region(
                        width=1,
                        height=1,
                        active=active,  # type: ignore[arg-type]
                        boundary=boundary,  # type: ignore[arg-type]
                    )

    def test_rejects_non_boolean_activity(self) -> None:
        no_boundary = (255, 255, 255, 255)

        for value in (0, 1, "active"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    Region(
                        width=1,
                        height=1,
                        active=(value,),  # type: ignore[arg-type]
                        boundary=(no_boundary,),
                    )

    def test_requires_four_immutable_boundary_sides_per_cell(self) -> None:
        invalid_boundaries = (
            ([255, 255, 255, 255],),
            ((255, 255, 255),),
            ((255, 255, 255, 255, 255),),
        )

        for boundary in invalid_boundaries:
            with self.subTest(boundary=boundary):
                with self.assertRaises(ValueError):
                    Region(
                        width=1,
                        height=1,
                        active=(True,),
                        boundary=boundary,  # type: ignore[arg-type]
                    )

    def test_accepts_only_native_color_ids_or_no_boundary(self) -> None:
        Region(
            width=1,
            height=1,
            active=(True,),
            boundary=((0, 15, 255, 255),),
        )

        for color in (-1, 16, 254, 256, True, "0"):
            with self.subTest(color=color):
                with self.assertRaises(ValueError):
                    Region(
                        width=1,
                        height=1,
                        active=(True,),
                        boundary=((color, 255, 255, 255),),  # type: ignore[arg-type]
                    )

    def test_rejects_constraints_on_inactive_cells(self) -> None:
        with self.assertRaises(ValueError):
            Region(
                width=1,
                height=1,
                active=(False,),
                boundary=((0, 255, 255, 255),),
            )

    def test_rejects_constraints_on_internal_edges(self) -> None:
        no_boundary = (255, 255, 255, 255)
        invalid_regions = (
            (2, 1, ((255, 0, 255, 255), no_boundary)),
            (2, 1, (no_boundary, (255, 255, 255, 0))),
            (1, 2, ((255, 255, 0, 255), no_boundary)),
            (1, 2, (no_boundary, (0, 255, 255, 255))),
        )

        for width, height, boundary in invalid_regions:
            with self.subTest(width=width, height=height, boundary=boundary):
                with self.assertRaises(ValueError):
                    Region(
                        width=width,
                        height=height,
                        active=(True, True),
                        boundary=boundary,
                    )

    def test_accepts_disconnected_geometry(self) -> None:
        no_boundary = (255, 255, 255, 255)

        region = Region(
            width=3,
            height=1,
            active=(True, False, True),
            boundary=((0, 1, 2, 3), no_boundary, (4, 5, 6, 7)),
        )

        self.assertEqual(region.active, (True, False, True))

    def test_accepts_region_without_active_cells(self) -> None:
        no_boundary = (255, 255, 255, 255)

        region = Region(
            width=2,
            height=1,
            active=(False, False),
            boundary=(no_boundary, no_boundary),
        )

        self.assertEqual(region.active, (False, False))


if __name__ == "__main__":
    unittest.main()
