import { clamp } from "./random.js";

function parseHexColor(value) {
  const normalized = value.replace("#", "");
  if (!/^[0-9a-f]{6}$/i.test(normalized)) {
    throw new TypeError(`unsupported color: ${value}`);
  }

  return [
    Number.parseInt(normalized.slice(0, 2), 16),
    Number.parseInt(normalized.slice(2, 4), 16),
    Number.parseInt(normalized.slice(4, 6), 16),
    255,
  ];
}

export function triangleForPixel(x, y, pixelSize) {
  if (x < 0 || y < 0 || x >= pixelSize || y >= pixelSize) {
    throw new RangeError("pixel lies outside the tile");
  }

  const center = pixelSize / 2;
  const deltaX = x + 0.5 - center;
  const deltaY = y + 0.5 - center;

  if (Math.abs(deltaX) > Math.abs(deltaY)) {
    return deltaX > 0 ? "east" : "west";
  }

  return deltaY > 0 ? "south" : "north";
}

/**
 * Builds an opaque pixel sprite. Every pixel is assigned to exactly one edge
 * triangle, so diagonal joins and four-cell intersections cannot expose the
 * page background through an antialiased seam.
 */
function rasterizeTile(tile, field, pixelSize) {
  const surface = document.createElement("canvas");
  surface.width = pixelSize;
  surface.height = pixelSize;

  const context = surface.getContext("2d", { alpha: true });
  const image = context.createImageData(pixelSize, pixelSize);
  const colors = {
    north: parseHexColor(field.color(tile.north)),
    east: parseHexColor(field.color(tile.east)),
    south: parseHexColor(field.color(tile.south)),
    west: parseHexColor(field.color(tile.west)),
  };

  for (let y = 0; y < pixelSize; y += 1) {
    for (let x = 0; x < pixelSize; x += 1) {
      const edge = triangleForPixel(x, y, pixelSize);
      const color = colors[edge];
      const offset = (y * pixelSize + x) * 4;
      image.data[offset] = color[0];
      image.data[offset + 1] = color[1];
      image.data[offset + 2] = color[2];
      image.data[offset + 3] = color[3];
    }
  }

  context.putImageData(image, 0, 0);
  return { surface, image };
}

export class TileRenderer {
  constructor(canvas, edgeField) {
    this.canvas = canvas;
    this.context = canvas.getContext("2d", { alpha: true, desynchronized: true });
    this.edgeField = edgeField;
    this.spriteCache = new Map();
    this.width = 0;
    this.height = 0;
    this.pixelRatio = 1;
    this.tileSize = 18;
    this.contentRect = null;
  }

  configure({ width, height, tileSize, contentRect }) {
    this.width = Math.max(1, Math.round(width));
    this.height = Math.max(1, Math.round(height));
    this.pixelRatio = clamp(window.devicePixelRatio || 1, 1, 2);
    this.tileSize = tileSize;
    this.contentRect = contentRect;

    const pixelWidth = Math.round(this.width * this.pixelRatio);
    const pixelHeight = Math.round(this.height * this.pixelRatio);
    if (this.canvas.width !== pixelWidth || this.canvas.height !== pixelHeight) {
      this.canvas.width = pixelWidth;
      this.canvas.height = pixelHeight;
    }

    this.canvas.style.width = `${this.width}px`;
    this.canvas.style.height = `${this.height}px`;
    this.context.setTransform(this.pixelRatio, 0, 0, this.pixelRatio, 0, 0);
    this.context.imageSmoothingEnabled = false;
    this.spriteCache.clear();
  }

  clear() {
    this.context.clearRect(0, 0, this.width, this.height);
  }

  #spriteFor(tile) {
    const key = `${tile.north}:${tile.east}:${tile.south}:${tile.west}`;
    const cached = this.spriteCache.get(key);
    if (cached) {
      return cached;
    }

    const pixelSize = Math.max(2, Math.round(this.tileSize * this.pixelRatio));
    const sprite = rasterizeTile(tile, this.edgeField, pixelSize);
    this.spriteCache.set(key, sprite);
    return sprite;
  }

  #contentAlpha(x, baseOpacity) {
    if (!this.contentRect) {
      return baseOpacity;
    }

    const center = x + this.tileSize / 2;
    const softMargin = 72;
    const leftDistance = this.contentRect.left - center;
    const rightDistance = center - this.contentRect.right;

    if (leftDistance <= 0 && rightDistance <= 0) {
      return baseOpacity * 0.1;
    }

    const distance = Math.max(leftDistance, rightDistance);
    if (distance >= softMargin) {
      return baseOpacity;
    }

    const mix = clamp(distance / softMargin, 0, 1);
    return baseOpacity * (0.1 + mix * 0.9);
  }

  draw(tile, documentX, viewportY, opacity) {
    const screenX = Math.round(documentX * this.pixelRatio) / this.pixelRatio;
    const screenY = Math.round(viewportY * this.pixelRatio) / this.pixelRatio;
    const alpha = this.#contentAlpha(screenX, opacity);

    if (alpha <= 0.01) {
      return;
    }

    const sprite = this.#spriteFor(tile);
    this.context.globalAlpha = alpha;
    this.context.drawImage(sprite.surface, screenX, screenY, this.tileSize, this.tileSize);
    this.context.globalAlpha = 1;
  }

  assertOpaqueSprite(tile) {
    const sprite = this.#spriteFor(tile);
    let opaque = true;

    for (let index = 3; index < sprite.image.data.length; index += 4) {
      if (sprite.image.data[index] !== 255) {
        opaque = false;
        break;
      }
    }

    console.assert(opaque, "Wang tile sprite contains a transparent seam", tile);
    if (!opaque) {
      throw new Error("Wang tile rasterization left an uncovered pixel");
    }
  }
}
