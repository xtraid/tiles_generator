import { Random, clamp, hashString } from "./random.js";

function documentY(element) {
  return element.getBoundingClientRect().top + window.scrollY;
}

function collectLandmarks(root) {
  const elements = [
    root.querySelector("h1"),
    ...root.querySelectorAll("[data-wang-landmark], h2, h3"),
  ].filter(Boolean);

  return [...new Set(elements)].map((element) => ({
    y: documentY(element),
    weight: element.matches("h1, h2, [data-wang-landmark]") ? 1 : 0.62,
  })).sort((left, right) => left.y - right.y);
}

function chooseAnchors({ root, landmarks, count, random }) {
  const rootRect = root.getBoundingClientRect();
  const top = rootRect.top + window.scrollY;
  const height = Math.max(root.scrollHeight, rootRect.height, window.innerHeight);
  const used = new Set();
  const anchors = [];

  for (let index = 0; index < count; index += 1) {
    const bucketStart = top + (height * index) / count;
    const bucketSize = height / count;
    const target = bucketStart + bucketSize * (0.22 + random.next() * 0.56);
    let bestIndex = -1;
    let bestScore = Number.POSITIVE_INFINITY;

    for (let landmarkIndex = 0; landmarkIndex < landmarks.length; landmarkIndex += 1) {
      if (used.has(landmarkIndex)) {
        continue;
      }

      const landmark = landmarks[landmarkIndex];
      const distanceScore = Math.abs(landmark.y - target) / landmark.weight;
      if (distanceScore < bestScore && distanceScore < Math.max(900, bucketSize * 1.15)) {
        bestIndex = landmarkIndex;
        bestScore = distanceScore;
      }
    }

    if (bestIndex >= 0) {
      used.add(bestIndex);
      anchors.push(landmarks[bestIndex].y);
    } else {
      anchors.push(target);
    }
  }

  return anchors.sort((left, right) => left - right);
}

function sideSeed(side, seedY, exclusion, widthInCells, random) {
  if (side === "left") {
    const laneEnd = Math.max(0, exclusion.left - 3);
    return { x: random.integer(0, laneEnd), y: seedY + random.integer(-3, 3) };
  }

  const laneStart = Math.min(widthInCells - 1, exclusion.right + 3);
  return { x: random.integer(laneStart, widthInCells - 1), y: seedY + random.integer(-3, 3) };
}

function oppositeTarget(side, seedY, widthInCells, random) {
  return {
    x: side === "left" ? widthInCells - 2 : 1,
    y: seedY + random.integer(-7, 7),
  };
}

function mergeGeometry(side, seedY, exclusion, widthInCells, random) {
  const leftLaneEnd = Math.max(3, exclusion.left - 3);
  const rightLaneStart = Math.min(widthInCells - 4, exclusion.right + 3);

  if (side === "left") {
    const outer = random.integer(0, Math.min(3, leftLaneEnd));
    const inner = Math.max(outer + 6, leftLaneEnd - random.integer(0, 3));
    return {
      seeds: [
        { x: outer, y: seedY - random.integer(5, 9) },
        { x: Math.min(inner, widthInCells - 1), y: seedY + random.integer(5, 9) },
      ],
      target: { x: Math.round((outer + inner) / 2), y: seedY },
    };
  }

  const outer = random.integer(Math.max(rightLaneStart, widthInCells - 4), widthInCells - 1);
  const inner = Math.min(outer - 6, rightLaneStart + random.integer(0, 3));
  return {
    seeds: [
      { x: outer, y: seedY - random.integer(5, 9) },
      { x: Math.max(0, inner), y: seedY + random.integer(5, 9) },
    ],
    target: { x: Math.round((outer + inner) / 2), y: seedY },
  };
}

function eventKind(random, lightMode, narrowViewport) {
  const roll = random.next();
  if (lightMode) return roll < 0.28 ? "CROSS" : "DRIFT";
  if (roll < (narrowViewport ? 0.18 : 0.27)) return "MERGE";
  if (roll < 0.62) return "CROSS";
  return "DRIFT";
}

function activityCount({ root, landmarks, lightMode, narrowViewport }) {
  const blockCount = root.querySelectorAll("h2, h3, pre, figure, blockquote").length;
  const heightTerm = Math.max(root.scrollHeight, window.innerHeight) / 1450;
  const structureTerm = Math.sqrt(landmarks.length + blockCount * 0.35) * 0.62;
  const raw = Math.round(0.6 + heightTerm + structureTerm);

  if (lightMode) return clamp(raw, 1, 4);
  return clamp(raw, 2, narrowViewport ? 8 : 12);
}

export function planSectionEvents({ root, identity, tileSize, contentRect, lightMode = false }) {
  const random = new Random(hashString(`${identity}:sections:${Math.round(window.innerWidth / 80)}`));
  const narrowViewport = window.innerWidth < 720;
  const widthInCells = Math.max(1, Math.ceil(window.innerWidth / tileSize));
  const exclusion = {
    left: clamp(Math.floor(contentRect.left / tileSize), 0, widthInCells - 1),
    right: clamp(Math.ceil(contentRect.right / tileSize), 0, widthInCells - 1),
  };
  const landmarks = collectLandmarks(root);
  const count = activityCount({ root, landmarks, lightMode, narrowViewport });
  const anchors = chooseAnchors({ root, landmarks, count, random });
  const scale = (lightMode ? 0.62 : 1) * (narrowViewport ? 0.58 : 1);

  const descriptors = anchors.map((anchorY, index) => {
    const kind = eventKind(random, lightMode, narrowViewport);
    const side = random.chance(0.5) ? "left" : "right";
    const seedY = Math.round(anchorY / tileSize);
    const common = {
      id: `event-${index}`,
      kind,
      anchorY,
      growthDistance: random.integer(300, 500),
      opacity: lightMode ? 0.62 : 0.78,
    };

    if (kind === "MERGE") {
      const geometry = mergeGeometry(side, seedY, exclusion, widthInCells, random);
      return {
        ...common,
        ...geometry,
        tileBudgets: [
          Math.max(22, Math.round(random.integer(48, 76) * scale)),
          Math.max(22, Math.round(random.integer(48, 76) * scale)),
        ],
      };
    }

    const seed = sideSeed(side, seedY, exclusion, widthInCells, random);
    if (kind === "CROSS") {
      return {
        ...common,
        seeds: [seed],
        target: oppositeTarget(side, seedY, widthInCells, random),
        tileBudget: Math.max(42, Math.round(random.integer(112, 166) * scale)),
      };
    }

    const horizontal = side === "left" ? 1 : -1;
    const directions = [
      { x: horizontal, y: random.chance(0.5) ? -0.42 : 0.42 },
      { x: horizontal * 0.28, y: random.chance(0.5) ? -1 : 1 },
      { x: horizontal * 0.75, y: 0.12 },
    ];

    return {
      ...common,
      seeds: [seed],
      direction: random.pick(directions),
      tileBudget: Math.max(28, Math.round(random.integer(58, 98) * scale)),
    };
  });

  return Object.freeze({ descriptors: Object.freeze(descriptors), exclusion, widthInCells });
}
