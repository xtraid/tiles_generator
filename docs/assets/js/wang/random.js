const UINT32_MAX_PLUS_ONE = 0x100000000;

export function hashString(value) {
  let hash = 0x811c9dc5;

  for (let index = 0; index < value.length; index += 1) {
    hash ^= value.charCodeAt(index);
    hash = Math.imul(hash, 0x01000193);
  }

  return hash >>> 0;
}

export function mix32(value) {
  let mixed = value >>> 0;
  mixed = Math.imul(mixed ^ (mixed >>> 16), 0x21f0aaad);
  mixed = Math.imul(mixed ^ (mixed >>> 15), 0x735a2d97);
  return (mixed ^ (mixed >>> 15)) >>> 0;
}

export function coordinateHash(seed, axis, x, y) {
  const axisSalt = axis === "horizontal" ? 0x9e3779b9 : 0x85ebca6b;
  const xHash = Math.imul(x | 0, 0x27d4eb2d);
  const yHash = Math.imul(y | 0, 0x165667b1);
  return mix32(seed ^ axisSalt ^ xHash ^ yHash);
}

export class Random {
  constructor(seed) {
    this.state = seed >>> 0;
  }

  next() {
    this.state = (this.state + 0x6d2b79f5) >>> 0;
    let value = this.state;
    value = Math.imul(value ^ (value >>> 15), value | 1);
    value ^= value + Math.imul(value ^ (value >>> 7), value | 61);
    return ((value ^ (value >>> 14)) >>> 0) / UINT32_MAX_PLUS_ONE;
  }

  integer(minimum, maximum) {
    if (maximum < minimum) {
      throw new RangeError("maximum must be greater than or equal to minimum");
    }

    return minimum + Math.floor(this.next() * (maximum - minimum + 1));
  }

  chance(probability) {
    return this.next() < probability;
  }

  pick(values) {
    if (values.length === 0) {
      throw new RangeError("cannot pick from an empty array");
    }

    return values[this.integer(0, values.length - 1)];
  }
}

export function clamp(value, minimum, maximum) {
  return Math.min(maximum, Math.max(minimum, value));
}
