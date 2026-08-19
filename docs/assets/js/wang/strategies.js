const distance = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);

function bestParentProgress(candidate, target, sequence) {
  let best = Number.NEGATIVE_INFINITY;
  const candidateDistance = distance(candidate, target);

  for (const parentIndex of candidate.parents) {
    const parent = sequence[parentIndex];
    best = Math.max(best, distance(parent, target) - candidateDistance);
  }

  return best;
}

function centerPenalty(x, exclusion, strength) {
  if (!exclusion) {
    return 0;
  }

  const padding = 2;
  if (x >= exclusion.left - padding && x <= exclusion.right + padding) {
    return -strength;
  }

  const distanceFromColumn = x < exclusion.left
    ? exclusion.left - x
    : x - exclusion.right;

  return distanceFromColumn < 6 ? -strength * (1 - distanceFromColumn / 6) : 0;
}

export function crossStrategy(target, exclusion) {
  return {
    kind: "CROSS",
    score(candidate, sequence) {
      const progress = bestParentProgress(candidate, target, sequence);
      const remaining = distance(candidate, target);
      return progress * 0.62 - remaining * 0.007 + centerPenalty(candidate.x, exclusion, 0.24);
    },
  };
}

export function mergeStrategy(target, exclusion) {
  return {
    kind: "MERGE",
    score(candidate, sequence) {
      const progress = bestParentProgress(candidate, target, sequence);
      const remaining = distance(candidate, target);
      return progress * 2.05 - remaining * 0.018 + centerPenalty(candidate.x, exclusion, 1.35);
    },
  };
}

export function driftStrategy(direction, exclusion) {
  const magnitude = Math.hypot(direction.x, direction.y) || 1;
  const unit = { x: direction.x / magnitude, y: direction.y / magnitude };

  return {
    kind: "DRIFT",
    score(candidate, sequence) {
      let bestProjection = Number.NEGATIVE_INFINITY;

      for (const parentIndex of candidate.parents) {
        const parent = sequence[parentIndex];
        const projection = (candidate.x - parent.x) * unit.x + (candidate.y - parent.y) * unit.y;
        bestProjection = Math.max(bestProjection, projection);
      }

      return bestProjection * 0.72 + centerPenalty(candidate.x, exclusion, 1.8);
    },
  };
}
