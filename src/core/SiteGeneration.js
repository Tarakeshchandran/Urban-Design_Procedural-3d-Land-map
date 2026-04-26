import { pt, centroid, polygonArea } from './geometry.js';

export const CurveType = {
  MAJOR_ROAD: 0,
  LOCAL_ROAD: 1,
  WATER_BOUNDARY: 2,
  PARK_BOUNDARY: 3,
  SITE_BOUNDARY: 4,
  PARCEL_BOUNDARY: 5,
  TRAM: 6,
  RAILWAY: 7,
  FOREST_BOUNDARY: 8
};

export const ParcelCategory = {
  LARGE_PARCEL: 0,
  MEDIUM_PARCEL: 1,
  SMALL_PARCEL: 2
};

export class SiteGeneration {
  constructor() {
    this.curves = [];
    this.majorRoads = [];
    this.localRoads = [];
    this.waterBoundaries = [];
    this.parkBoundaries = [];
    this.siteBoundaries = [];
    this.parcelBoundaries = [];
    this.tramLines = [];
    this.railways = [];
    this.forestBoundaries = [];
    
    this.parcelCenters = [];
    this.parcels = [];
    
    this.roadBuffer = 5.0;
    this.boundaryResolution = 100;
    this.AUTO_SCALE_FACTOR = 0.1;
    this.largeParcelThreshold = 100;
    this.mediumParcelThreshold = 80;
  }

  async importCurvesFromCSV(url) {
    console.log("=== IMPORTING CURVES FROM CSV ===");
    try {
      this.curves = [];
      const response = await fetch(url);
      if (!response.ok) {
          throw new Error(`Failed to load ${url}: ${response.statusText}`);
      }
      const text = await response.text();
      const lines = text.split('\n');
      for (const line of lines) {
        if (!line || line.startsWith('#')) continue;
        this.parseCurveLine(line.trim());
      }
      this.organizeCurvesByType();
      this.scaleAllCurves(this.AUTO_SCALE_FACTOR);
      console.log(`Imported ${this.curves.length} curves (auto-scaled by factor ${this.AUTO_SCALE_FACTOR})`);
      return true;
    } catch (err) {
      console.error(err);
      return false;
    }
  }

  parseCurveLine(line) {
    const tokens = line.split(',');
    if (tokens.length < 7) return false;
    
    const typeStr = tokens[0];
    let type;
    if (typeStr === 'major_road') type = CurveType.MAJOR_ROAD;
    else if (typeStr === 'local_road') type = CurveType.LOCAL_ROAD;
    else if (typeStr === 'water') type = CurveType.WATER_BOUNDARY;
    else if (typeStr === 'park') type = CurveType.PARK_BOUNDARY;
    else if (typeStr === 'site') type = CurveType.SITE_BOUNDARY;
    else if (typeStr === 'parcel') type = CurveType.PARCEL_BOUNDARY;
    else if (typeStr === 'tram') type = CurveType.TRAM;
    else if (typeStr === 'railway') type = CurveType.RAILWAY;
    else if (typeStr === 'forest') type = CurveType.FOREST_BOUNDARY;
    else return false;

    const name = tokens[1];
    const width = parseFloat(tokens[2]);
    const isClosed = tokens[3] === '1';

    const points = [];
    for (let i = 4; i < tokens.length - 1; i += 2) {
      const x = parseFloat(tokens[i]);
      const y = parseFloat(tokens[i+1]);
      if (!isNaN(x) && !isNaN(y)) {
        points.push(pt(x, y));
      }
    }

    if (points.length >= 2) {
      this.curves.push({ type, name, width, isClosed, points });
      return true;
    }
    return false;
  }

  organizeCurvesByType() {
    this.majorRoads = [];
    this.localRoads = [];
    this.waterBoundaries = [];
    this.parkBoundaries = [];
    this.siteBoundaries = [];
    this.parcelBoundaries = [];
    this.tramLines = [];
    this.railways = [];
    this.forestBoundaries = [];

    for (const curve of this.curves) {
      switch (curve.type) {
        case CurveType.MAJOR_ROAD: this.majorRoads.push(curve); break;
        case CurveType.LOCAL_ROAD: this.localRoads.push(curve); break;
        case CurveType.WATER_BOUNDARY: this.waterBoundaries.push(curve); break;
        case CurveType.PARK_BOUNDARY: this.parkBoundaries.push(curve); break;
        case CurveType.SITE_BOUNDARY: this.siteBoundaries.push(curve); break;
        case CurveType.PARCEL_BOUNDARY: this.parcelBoundaries.push(curve); break;
        case CurveType.TRAM: this.tramLines.push(curve); break;
        case CurveType.RAILWAY: this.railways.push(curve); break;
        case CurveType.FOREST_BOUNDARY: this.forestBoundaries.push(curve); break;
      }
    }
  }

  scaleAllCurves(factor) {
    for (const curve of this.curves) {
      for (const p of curve.points) {
        p.x *= factor;
        p.y *= factor;
      }
      curve.width *= factor;
    }
    // Need to re-organize as the arrays store references to the curves
    this.organizeCurvesByType();
  }

  generateParcelsFromBoundaries(largeThresh = 100, mediumThresh = 80) {
    console.log("=== GENERATING PARCELS FROM IMPORTED BOUNDARIES ===");
    this.largeParcelThreshold = largeThresh;
    this.mediumParcelThreshold = mediumThresh;
    this.parcels = [];
    for (let i = 0; i < this.parcelBoundaries.length; i++) {
        const boundary = this.parcelBoundaries[i];
        if (!boundary.isClosed || boundary.points.length < 3) continue;

        const area = polygonArea(boundary.points);
        let category = ParcelCategory.SMALL_PARCEL;
        if (area >= largeThresh) category = ParcelCategory.LARGE_PARCEL;
        else if (area >= mediumThresh) category = ParcelCategory.MEDIUM_PARCEL;

        this.parcels.push({
            id: i,
            boundary: boundary.points,
            area,
            category,
            center: centroid(boundary.points),
            radius: 20 * this.AUTO_SCALE_FACTOR
        });
    }
    console.log(`Generated ${this.parcels.length} parcels from ${this.parcelBoundaries.length} boundaries`);
  }

  getCurves() { return this.curves; }
  getMajorRoads() { return this.majorRoads; }
  getLocalRoads() { return this.localRoads; }
  getWaterBoundaries() { return this.waterBoundaries; }
  getParkBoundaries() { return this.parkBoundaries; }
  getSiteBoundaries() { return this.siteBoundaries; }
  getParcelBoundaries() { return this.parcelBoundaries; }
  getTramLines() { return this.tramLines; }
  getRailways() { return this.railways; }
  getForestBoundaries() { return this.forestBoundaries; }
  getParcelCenters() { return this.parcelCenters; }
  getParcels() { return this.parcels; }
}
