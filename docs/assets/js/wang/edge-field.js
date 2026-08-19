import { coordinateHash, hashString } from "./random.js";

export const EDGE_PALETTE = Object.freeze([
  "#f05d4f",
  "#3f9ec9",
  "#78b84f",
  "#e3b63f",
  "#a979c9",
  "#d9823f",
]);

/**
 * A coordinate edge is the single source of truth for both incident cells.
 * Horizontal edge (x, y) is north of cell (x, y) and south of (x, y - 1).
 * Vertical edge (x, y) is west of cell (x, y) and east of (x - 1, y).
 */
export class WangEdgeField {
  constructor(identity, palette = EDGE_PALETTE) {
    if (palette.length < 2) {
      throw new RangeError("a Wang field requires at least two edge labels");
    }

    this.seed = typeof identity === "number" ? identity >>> 0 : hashString(identity);
    this.palette = Object.freeze([...palette]);
  }

  edgeLabel(axis, x, y) {
    const labelCount = this.palette.length;
    return coordinateHash(this.seed, axis, x, y) % labelCount;
  }

  tileAt(x, y) {
    return Object.freeze({
      x,
      y,
      north: this.edgeLabel("horizontal", x, y),
      east: this.edgeLabel("vertical", x + 1, y),
      south: this.edgeLabel("horizontal", x, y + 1),
      west: this.edgeLabel("vertical", x, y),
    });
  }

  color(label) {
    return this.palette[label];
  }
}

export function horizontalMatch(left, right) {
  return left.east === right.west;
}

export function verticalMatch(top, bottom) {
  return top.south === bottom.north;
}
