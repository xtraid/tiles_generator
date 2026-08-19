import { cellKey, ORTHOGONAL_DIRECTIONS } from "./growth-frontier.js";
import { horizontalMatch, verticalMatch } from "./edge-field.js";

export function inspectPlan(plan) {
  const violations = [];
  const allCells = new Map();

  for (const cluster of plan.clusters) {
    const seeds = cluster.cells.filter((cell) => cell.parentIndex === null);
    if (cluster.cells.length === 0 || seeds.length !== 1 || cluster.cells[0].parentIndex !== null) {
      violations.push(`${cluster.id}: expected exactly one seed at sequence index zero`);
    }

    const prior = new Set();
    cluster.cells.forEach((cell, index) => {
      if (index > 0) {
        const hasPriorNeighbor = ORTHOGONAL_DIRECTIONS.some((direction) => (
          prior.has(cellKey(cell.x + direction.x, cell.y + direction.y))
        ));

        if (!hasPriorNeighbor) {
          violations.push(`${cluster.id}:${index}: no prior orthogonal neighbor`);
        }

        if (cell.parentIndex === null || cell.parentIndex >= index) {
          violations.push(`${cluster.id}:${index}: invalid causal parent`);
        } else {
          const parent = cluster.cells[cell.parentIndex];
          const parentDistance = Math.abs(parent.x - cell.x) + Math.abs(parent.y - cell.y);
          if (parentDistance !== 1) {
            violations.push(`${cluster.id}:${index}: parent is not orthogonally adjacent`);
          }
        }
      }

      prior.add(cellKey(cell.x, cell.y));
      allCells.set(cellKey(cell.x, cell.y), cell.tile);
    });
  }

  for (const tile of allCells.values()) {
    const east = allCells.get(cellKey(tile.x + 1, tile.y));
    const south = allCells.get(cellKey(tile.x, tile.y + 1));

    if (east && !horizontalMatch(tile, east)) {
      violations.push(`edge mismatch: east(${tile.x},${tile.y}) !== west(${east.x},${east.y})`);
    }

    if (south && !verticalMatch(tile, south)) {
      violations.push(`edge mismatch: south(${tile.x},${tile.y}) !== north(${south.x},${south.y})`);
    }
  }

  return violations;
}

export function assertPlanInDevelopment(plan) {
  const violations = inspectPlan(plan);

  console.assert(
    violations.length === 0,
    "Wang plan invariant failure",
    violations,
  );

  if (violations.length > 0) {
    throw new Error(`Wang plan invariant failure: ${violations[0]}`);
  }
}
