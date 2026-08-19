import { clamp } from "./random.js";

function easeOutQuadratic(value) {
  return 1 - (1 - value) * (1 - value);
}

export function renderWangFrame({ renderer, plan, tileSize, scrollY, viewportHeight, reducedMotion }) {
  renderer.clear();

  for (const cluster of plan.clusters) {
    const rawProgress = (scrollY + viewportHeight - cluster.anchorY) / cluster.growthDistance;
    if (rawProgress < 0) {
      continue;
    }

    const top = cluster.bounds.top * tileSize;
    const bottom = (cluster.bounds.bottom + 1) * tileSize;
    if (bottom < scrollY - tileSize || top > scrollY + viewportHeight + tileSize) {
      continue;
    }

    const progress = easeOutQuadratic(clamp(rawProgress, 0, 1));
    const visibleCount = reducedMotion
      ? cluster.cells.length
      : Math.min(
        cluster.cells.length,
        1 + Math.floor(progress * Math.max(0, cluster.cells.length - 1)),
      );

    for (let index = 0; index < visibleCount; index += 1) {
      const cell = cluster.cells[index];
      renderer.draw(
        cell.tile,
        cell.x * tileSize,
        cell.y * tileSize - scrollY,
        cluster.opacity,
      );
    }
  }
}
export class ScrollController {
  constructor(drawFrame) {
    this.drawFrame = drawFrame;
    this.framePending = false;
    this.active = false;
    this.onScroll = this.requestFrame.bind(this);
    this.onVisibility = () => {
      if (!document.hidden) this.requestFrame();
    };
  }

  start() {
    if (this.active) return;
    this.active = true;
    window.addEventListener("scroll", this.onScroll, { passive: true });
    document.addEventListener("visibilitychange", this.onVisibility);
    this.requestFrame();
  }

  requestFrame() {
    if (!this.active || this.framePending) return;
    this.framePending = true;
    window.requestAnimationFrame(() => {
      this.framePending = false;
      this.drawFrame();
    });
  }

  stop() {
    if (!this.active) return;
    this.active = false;
    window.removeEventListener("scroll", this.onScroll);
    document.removeEventListener("visibilitychange", this.onVisibility);
  }
}
