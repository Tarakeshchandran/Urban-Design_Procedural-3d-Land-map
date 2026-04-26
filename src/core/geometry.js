export const EPS = 1e-6;

export function pt(x = 0, y = 0) {
  return { x, y };
}

export function add(a, b) {
  return { x: a.x + b.x, y: a.y + b.y };
}

export function sub(a, b) {
  return { x: a.x - b.x, y: a.y - b.y };
}

export function mul(a, s) {
  return { x: a.x * s, y: a.y * s };
}

export function dot(a, b) {
  return a.x * b.x + a.y * b.y;
}

export function cross(a, b) {
  return a.x * b.y - a.y * b.x;
}

export function mag(a) {
  return Math.hypot(a.x, a.y);
}

export function normalize(a) {
  const m = mag(a);
  if (m < EPS) return { x: 1, y: 0 };
  return { x: a.x / m, y: a.y / m };
}

export function distance(a, b) {
  return Math.hypot(a.x - b.x, a.y - b.y);
}

export function centroid(poly) {
  if (!poly.length) return { x: 0, y: 0 };
  let cx = 0;
  let cy = 0;
  let a = 0;
  for (let i = 0; i < poly.length; i += 1) {
    const j = (i + 1) % poly.length;
    const c = poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    a += c;
    cx += (poly[i].x + poly[j].x) * c;
    cy += (poly[i].y + poly[j].y) * c;
  }
  a *= 0.5;
  if (Math.abs(a) < EPS) {
    const s = poly.reduce((acc, p) => ({ x: acc.x + p.x, y: acc.y + p.y }), { x: 0, y: 0 });
    return { x: s.x / poly.length, y: s.y / poly.length };
  }
  return { x: cx / (6 * a), y: cy / (6 * a) };
}

export function polygonArea(poly) {
  let a = 0;
  for (let i = 0; i < poly.length; i += 1) {
    const j = (i + 1) % poly.length;
    a += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
  }
  return Math.abs(a) * 0.5;
}

export function pointInPolygon(p, poly) {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i, i += 1) {
    const pi = poly[i];
    const pj = poly[j];
    const intersects = ((pi.y > p.y) !== (pj.y > p.y))
      && (p.x < ((pj.x - pi.x) * (p.y - pi.y)) / ((pj.y - pi.y) || EPS) + pi.x);
    if (intersects) inside = !inside;
  }
  return inside;
}

export function bounds(poly) {
  let minX = Number.POSITIVE_INFINITY;
  let minY = Number.POSITIVE_INFINITY;
  let maxX = Number.NEGATIVE_INFINITY;
  let maxY = Number.NEGATIVE_INFINITY;
  for (const p of poly) {
    if (p.x < minX) minX = p.x;
    if (p.y < minY) minY = p.y;
    if (p.x > maxX) maxX = p.x;
    if (p.y > maxY) maxY = p.y;
  }
  return { minX, minY, maxX, maxY, width: maxX - minX, height: maxY - minY };
}

export function segmentClosestPoint(p, a, b) {
  const ab = sub(b, a);
  const l2 = dot(ab, ab);
  if (l2 < EPS) return a;
  const t = Math.max(0, Math.min(1, dot(sub(p, a), ab) / l2));
  return add(a, mul(ab, t));
}

export function distancePointToPolyline(p, points) {
  if (points.length < 2) return Number.POSITIVE_INFINITY;
  let minD = Number.POSITIVE_INFINITY;
  for (let i = 0; i < points.length - 1; i += 1) {
    const c = segmentClosestPoint(p, points[i], points[i + 1]);
    const d = distance(p, c);
    if (d < minD) minD = d;
  }
  return minD;
}

export function lineSegmentIntersection(a, b, c, d) {
  const r = sub(b, a);
  const s = sub(d, c);
  const denom = cross(r, s);
  const qp = sub(c, a);

  if (Math.abs(denom) < EPS) return null;

  const t = cross(qp, s) / denom;
  const u = cross(qp, r) / denom;
  if (t >= -EPS && t <= 1 + EPS && u >= -EPS && u <= 1 + EPS) {
    return add(a, mul(r, t));
  }
  return null;
}

export function splitPolygonByLine(poly, lineA, lineB) {
  if (poly.length < 3) return [];
  const intersections = [];
  const edgeInfo = [];

  for (let i = 0; i < poly.length; i += 1) {
    const j = (i + 1) % poly.length;
    const hit = lineSegmentIntersection(lineA, lineB, poly[i], poly[j]);
    if (hit) {
      const duplicate = intersections.some((p) => distance(p, hit) < 1e-4);
      if (!duplicate) {
        intersections.push(hit);
        edgeInfo.push({ point: hit, edgeIndex: i });
      }
    }
  }

  if (intersections.length < 2) return [];
  const p1 = edgeInfo[0];
  const p2 = edgeInfo[1];

  const poly1 = [p1.point];
  let idx = (p1.edgeIndex + 1) % poly.length;
  while (idx !== (p2.edgeIndex + 1) % poly.length) {
    poly1.push(poly[idx]);
    idx = (idx + 1) % poly.length;
  }
  poly1.push(p2.point);

  const poly2 = [p2.point];
  idx = (p2.edgeIndex + 1) % poly.length;
  while (idx !== (p1.edgeIndex + 1) % poly.length) {
    poly2.push(poly[idx]);
    idx = (idx + 1) % poly.length;
  }
  poly2.push(p1.point);

  if (polygonArea(poly1) < EPS || polygonArea(poly2) < EPS) return [];
  return [cleanupPolygon(poly1), cleanupPolygon(poly2)];
}

export function cleanupPolygon(poly) {
  const cleaned = [];
  for (const p of poly) {
    if (!cleaned.length || distance(cleaned[cleaned.length - 1], p) > 1e-4) cleaned.push(p);
  }
  if (cleaned.length > 2 && distance(cleaned[0], cleaned[cleaned.length - 1]) < 1e-4) cleaned.pop();
  return cleaned;
}

export function offsetPolygonTowardCentroid(poly, inset) {
  const c = centroid(poly);
  const out = [];
  for (const p of poly) {
    const v = sub(c, p);
    const m = mag(v);
    if (m < EPS) {
      out.push({ ...p });
      continue;
    }
    const move = Math.min(inset, m * 0.4);
    out.push(add(p, mul(normalize(v), move)));
  }
  return cleanupPolygon(out);
}

export function longestEdgeDirection(poly) {
  if (poly.length < 2) return { x: 1, y: 0 };
  let best = { x: 1, y: 0 };
  let bestLen = -1;
  for (let i = 0; i < poly.length; i += 1) {
    const j = (i + 1) % poly.length;
    const e = sub(poly[j], poly[i]);
    const l = mag(e);
    if (l > bestLen) {
      bestLen = l;
      best = normalize(e);
    }
  }
  return best;
}

export function rotatePointAround(point, center, angleRad) {
  const dx = point.x - center.x;
  const dy = point.y - center.y;
  const ca = Math.cos(angleRad);
  const sa = Math.sin(angleRad);
  return {
    x: center.x + dx * ca - dy * sa,
    y: center.y + dx * sa + dy * ca,
  };
}
