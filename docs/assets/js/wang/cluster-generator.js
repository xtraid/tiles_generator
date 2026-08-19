import { GrowthFrontier, ORTHOGONAL_DIRECTIONS, cellKey } from "./growth-frontier.js";
import { Random, hashString } from "./random.js";
import { crossStrategy, driftStrategy, mergeStrategy } from "./strategies.js";

const MAX_PLAN_TILES = 1800;

function claimSequence(cluster, edgeField) {
  const cells = cluster.frontier.sequence.map((placement) => Object.freeze({
    ...placement,
    tile: edgeField.tileAt(placement.x, placement.y),
  }));

  const xValues = cells.map((cell) => cell.x);
  const yValues = cells.map((cell) => cell.y);

  return Object.freeze({
    id: cluster.id,
    kind: cluster.kind,
    anchorY: cluster.anchorY,
    growthDistance: cluster.growthDistance,
    opacity: cluster.opacity,
    cells: Object.freeze(cells),
    bounds: Object.freeze({
      left: Math.min(...xValues),
      right: Math.max(...xValues),
      top: Math.min(...yValues),
      bottom: Math.max(...yValues),
    }),
  });
}

function hasNeighborOwnedBy(cell, claims, ownerId) {
  return ORTHOGONAL_DIRECTIONS.some((direction) => (
    claims.get(cellKey(cell.x + direction.x, cell.y + direction.y)) === ownerId
  ));
}

function nearestFreeSeed(seed, claims, limits) {
  if (!claims.has(cellKey(seed.x, seed.y))) {
    return seed;
  }

  for (let radius = 1; radius <= 8; radius += 1) {
    for (let offset = -radius; offset <= radius; offset += 1) {
      const candidates = [
        { x: seed.x + offset, y: seed.y - radius },
        { x: seed.x + offset, y: seed.y + radius },
        { x: seed.x - radius, y: seed.y + offset },
        { x: seed.x + radius, y: seed.y + offset },
      ];

      for (const candidate of candidates) {
        if (
          candidate.x >= limits.left
          && candidate.x <= limits.right
          && !claims.has(cellKey(candidate.x, candidate.y))
        ) {
          return candidate;
        }
      }
    }
  }

  return null;
}

function createFrontier({ descriptor, id, seed, claims, limits, randomSeed }) {
  const freeSeed = nearestFreeSeed(seed, claims, limits);
  if (!freeSeed) {
    return null;
  }

  claims.set(cellKey(freeSeed.x, freeSeed.y), id);

  const frontier = new GrowthFrontier({
    seed: freeSeed,
    randomSeed,
    isBlocked(x, y) {
      return x < limits.left || x > limits.right || claims.has(cellKey(x, y));
    },
  });

  return {
    id,
    kind: descriptor.kind,
    anchorY: descriptor.anchorY,
    growthDistance: descriptor.growthDistance,
    opacity: descriptor.opacity,
    frontier,
  };
}

function growCluster(cluster, strategy, tileBudget, claims) {
  while (cluster.frontier.sequence.length < tileBudget) {
    const placement = cluster.frontier.growOne(strategy);
    if (!placement) {
      break;
    }

    claims.set(cellKey(placement.x, placement.y), cluster.id);
  }
}

function generateSingle(descriptor, context, index) {
  const id = `cluster-${index}`;
  const cluster = createFrontier({
    descriptor,
    id,
    seed: descriptor.seeds[0],
    claims: context.claims,
    limits: context.limits,
    randomSeed: hashString(`${context.identity}:${id}:growth`),
  });

  if (!cluster) {
    return [];
  }

  const strategy = descriptor.kind === "CROSS"
    ? crossStrategy(descriptor.target, context.exclusion)
    : driftStrategy(descriptor.direction, context.exclusion);

  growCluster(cluster, strategy, descriptor.tileBudget, context.claims);
  return [claimSequence(cluster, context.edgeField)];
}

function generateMerge(descriptor, context, index) {
  const clusters = descriptor.seeds.map((seed, pairIndex) => createFrontier({
    descriptor,
    id: `cluster-${index}-${pairIndex}`,
    seed,
    claims: context.claims,
    limits: context.limits,
    randomSeed: hashString(`${context.identity}:merge:${index}:${pairIndex}`),
  })).filter(Boolean);

  if (clusters.length !== 2) {
    return clusters.map((cluster) => claimSequence(cluster, context.edgeField));
  }

  const strategies = clusters.map(() => mergeStrategy(descriptor.target, context.exclusion));
  let contactCountdown = null;

  while (clusters.some((cluster, pairIndex) => (
    cluster.frontier.sequence.length < descriptor.tileBudgets[pairIndex]
  ))) {
    let advanced = false;

    for (let pairIndex = 0; pairIndex < clusters.length; pairIndex += 1) {
      const cluster = clusters[pairIndex];
      if (cluster.frontier.sequence.length >= descriptor.tileBudgets[pairIndex]) {
        continue;
      }

      const placement = cluster.frontier.growOne(strategies[pairIndex]);
      if (!placement) {
        continue;
      }

      advanced = true;
      context.claims.set(cellKey(placement.x, placement.y), cluster.id);

      const other = clusters[1 - pairIndex];
      if (hasNeighborOwnedBy(placement, context.claims, other.id) && contactCountdown === null) {
        contactCountdown = 12;
      }
    }

    if (!advanced) {
      break;
    }

    if (contactCountdown !== null) {
      contactCountdown -= 1;
      if (contactCountdown <= 0) {
        break;
      }
    }
  }

  return clusters.map((cluster) => claimSequence(cluster, context.edgeField));
}

export function generateClusterPlan({ identity, descriptors, edgeField, widthInCells, exclusion }) {
  const claims = new Map();
  const limits = { left: 0, right: Math.max(0, widthInCells - 1) };
  const context = { identity, edgeField, claims, limits, exclusion };
  const clusters = [];
  let plannedTiles = 0;

  for (let index = 0; index < descriptors.length; index += 1) {
    if (plannedTiles >= MAX_PLAN_TILES) {
      break;
    }

    const descriptor = descriptors[index];
    const generated = descriptor.kind === "MERGE"
      ? generateMerge(descriptor, context, index)
      : generateSingle(descriptor, context, index);

    for (const cluster of generated) {
      const room = MAX_PLAN_TILES - plannedTiles;
      if (cluster.cells.length > room) {
        const truncated = Object.freeze({
          ...cluster,
          cells: Object.freeze(cluster.cells.slice(0, room)),
        });
        clusters.push(truncated);
        plannedTiles += truncated.cells.length;
      } else {
        clusters.push(cluster);
        plannedTiles += cluster.cells.length;
      }
    }
  }

  return Object.freeze({
    clusters: Object.freeze(clusters),
    tileCount: plannedTiles,
    claims,
  });
}
