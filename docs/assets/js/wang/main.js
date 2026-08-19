import { generateClusterPlan } from "./cluster-generator.js";
import { WangEdgeField } from "./edge-field.js";
import { assertPlanInDevelopment } from "./invariants.js";
import { planSectionEvents } from "./section-planner.js";
import { ScrollController, renderWangFrame } from "./scroll-controller.js";
import { TileRenderer } from "./tile-renderer.js";

const canvas = document.querySelector("[data-wang-canvas]");
const root = document.querySelector("[data-wang-root]");
const contentColumn = document.querySelector("[data-content-column]");

if (canvas && root && contentColumn) {
  const identity = `${document.title}:${window.location.pathname}`;
  const edgeField = new WangEdgeField(identity);
  const renderer = new TileRenderer(canvas, edgeField);
  const motionQuery = window.matchMedia("(prefers-reduced-motion: reduce)");
  const developmentMode = (
    document.body.dataset.jekyllEnvironment !== "production"
    || new URLSearchParams(window.location.search).has("wang-debug")
  );

  let model = null;
  let resizeTimer = null;

  const controller = new ScrollController(() => {
    if (!model) return;
    renderWangFrame({
      renderer,
      plan: model.plan,
      tileSize: model.tileSize,
      scrollY: window.scrollY,
      viewportHeight: window.innerHeight,
      reducedMotion: motionQuery.matches,
    });
  });

  function rebuild() {
    const lightMode = document.body.dataset.pageKind === "home";
    const tileSize = window.innerWidth < 720 ? 15 : 18;
    const contentRect = contentColumn.getBoundingClientRect();

    renderer.configure({
      width: window.innerWidth,
      height: window.innerHeight,
      tileSize,
      contentRect,
    });

    const sectionPlan = planSectionEvents({
      root,
      identity,
      tileSize,
      contentRect,
      lightMode,
    });
    const plan = generateClusterPlan({
      identity,
      descriptors: sectionPlan.descriptors,
      edgeField,
      widthInCells: sectionPlan.widthInCells,
      exclusion: sectionPlan.exclusion,
    });

    model = { plan, tileSize };

    if (developmentMode) {
      assertPlanInDevelopment(plan);
      const firstTile = plan.clusters[0]?.cells[0]?.tile;
      if (firstTile) renderer.assertOpaqueSprite(firstTile);
      window.__tilingFoundryWang = Object.freeze({ edgeField, plan, sectionPlan });
    }

    controller.requestFrame();
  }

  function scheduleRebuild() {
    window.clearTimeout(resizeTimer);
    resizeTimer = window.setTimeout(rebuild, 120);
  }

  window.addEventListener("resize", scheduleRebuild, { passive: true });
  window.addEventListener("pageshow", scheduleRebuild, { passive: true });
  motionQuery.addEventListener?.("change", controller.onScroll);

  if ("ResizeObserver" in window) {
    const observer = new ResizeObserver(scheduleRebuild);
    observer.observe(root);
  }

  rebuild();
  controller.start();
}
