import { Random } from "./random.js";

export const ORTHOGONAL_DIRECTIONS = Object.freeze([
  Object.freeze({ x: 0, y: -1 }),
  Object.freeze({ x: 1, y: 0 }),
  Object.freeze({ x: 0, y: 1 }),
  Object.freeze({ x: -1, y: 0 }),
]);

export function cellKey(x, y) {
  return `${x}:${y}`;
}

export class GrowthFrontier {
  constructor({ seed, randomSeed, isBlocked = () => false }) {
    this.random = new Random(randomSeed);
    this.isBlocked = isBlocked;
    this.occupied = new Map();
    this.frontier = new Map();
    this.frontierKeys = [];
    this.sequence = [];

    this.#place(seed.x, seed.y, null);
  }

  #place(x, y, parentIndex) {
    const index = this.sequence.length;
    const placement = Object.freeze({ x, y, parentIndex, step: index });
    const key = cellKey(x, y);

    this.sequence.push(placement);
    this.occupied.set(key, index);
    this.frontier.delete(key);

    for (const direction of ORTHOGONAL_DIRECTIONS) {
      const neighborX = x + direction.x;
      const neighborY = y + direction.y;
      const neighborKey = cellKey(neighborX, neighborY);

      if (this.occupied.has(neighborKey) || this.isBlocked(neighborX, neighborY)) {
        continue;
      }

      const existing = this.frontier.get(neighborKey);
      if (existing) {
        existing.parents.add(index);
      } else {
        this.frontier.set(neighborKey, {
          x: neighborX,
          y: neighborY,
          parents: new Set([index]),
          noise: this.random.next() * 2 - 1,
        });
        this.frontierKeys.push(neighborKey);
      }
    }

    return placement;
  }

  #neighborCount(candidate) {
    let count = 0;

    for (const direction of ORTHOGONAL_DIRECTIONS) {
      if (this.occupied.has(cellKey(candidate.x + direction.x, candidate.y + direction.y))) {
        count += 1;
      }
    }

    return count;
  }

  #localDensity(candidate) {
    let count = 0;

    for (let offsetY = -1; offsetY <= 1; offsetY += 1) {
      for (let offsetX = -1; offsetX <= 1; offsetX += 1) {
        if (this.occupied.has(cellKey(candidate.x + offsetX, candidate.y + offsetY))) {
          count += 1;
        }
      }
    }

    return count;
  }

  #momentum(candidate) {
    let best = 0;

    for (const parentIndex of candidate.parents) {
      const parent = this.sequence[parentIndex];
      if (parent.parentIndex === null) {
        continue;
      }

      const grandparent = this.sequence[parent.parentIndex];
      const previousX = parent.x - grandparent.x;
      const previousY = parent.y - grandparent.y;
      const nextX = candidate.x - parent.x;
      const nextY = candidate.y - parent.y;
      best = Math.max(best, previousX === nextX && previousY === nextY ? 0.52 : 0.14);
    }

    return best;
  }

  #score(candidate, strategy) {
    const neighborCount = this.#neighborCount(candidate);
    const density = this.#localDensity(candidate);
    const newestParent = Math.max(...candidate.parents);
    const age = this.sequence.length - 1 - newestParent;

    let thickness = 0;
    if (neighborCount === 1) thickness = 0.28;
    if (neighborCount === 2) thickness = 1.42;
    if (neighborCount === 3) thickness = -0.72;
    if (neighborCount === 4) thickness = -2.4;

    let compactness = 0;
    if (density <= 1) compactness = -0.18;
    if (density >= 2 && density <= 5) compactness = 0.38;
    if (density >= 7) compactness = -0.62;

    const staleness = -Math.min(age, 70) * 0.014;
    const localRevival = this.sequence.length % 17 === 0 && age > 12 ? 0.62 : 0;

    return (
      candidate.noise * 1.08
      + thickness
      + compactness
      + staleness
      + localRevival
      + this.#momentum(candidate)
      + strategy.score(candidate, this.sequence)
    );
  }

  growOne(strategy) {
    let selected = null;
    let selectedScore = Number.NEGATIVE_INFINITY;
    const sampleSize = Math.min(72, this.frontier.size);
    const keys = new Set();

    if (this.frontierKeys.length > this.frontier.size * 2 + 96) {
      this.frontierKeys = this.frontierKeys.filter((key) => this.frontier.has(key));
    }

    for (
      let index = this.frontierKeys.length - 1;
      index >= 0 && keys.size < Math.min(28, sampleSize);
      index -= 1
    ) {
      const key = this.frontierKeys[index];
      if (this.frontier.has(key)) keys.add(key);
    }

    const maximumAttempts = Math.max(32, sampleSize * 5);
    for (let attempt = 0; attempt < maximumAttempts && keys.size < sampleSize; attempt += 1) {
      const key = this.frontierKeys[this.random.integer(0, this.frontierKeys.length - 1)];
      if (this.frontier.has(key)) keys.add(key);
    }

    if (keys.size < sampleSize) {
      for (const key of this.frontier.keys()) {
        keys.add(key);
        if (keys.size >= sampleSize) break;
      }
    }

    for (const key of keys) {
      const candidate = this.frontier.get(key);
      if (!candidate) continue;
      if (this.isBlocked(candidate.x, candidate.y)) {
        this.frontier.delete(key);
        continue;
      }

      const score = this.#score(candidate, strategy);
      if (score > selectedScore) {
        selected = candidate;
        selectedScore = score;
      }
    }

    if (!selected) {
      return null;
    }

    let parentIndex = -1;
    for (const candidateParent of selected.parents) {
      parentIndex = Math.max(parentIndex, candidateParent);
    }

    return this.#place(selected.x, selected.y, parentIndex);
  }
}
