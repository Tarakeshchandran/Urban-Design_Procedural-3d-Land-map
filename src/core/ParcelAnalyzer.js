import { pt, distance, segmentClosestPoint, lineSegmentIntersection, normalize, sub } from './geometry.js';
import { ParcelCategory } from './SiteGeneration.js';

export const RoadHierarchy = {
  MAJOR_ARTERIAL: 0,
  MINOR_ARTERIAL: 1,
  COLLECTOR: 2,
  LOCAL_STREET: 3
};

export const SiteEdgeType = {
  STREET_EDGE: 0,
  WATER_EDGE: 1,
  PARK_EDGE: 2,
  INTERNAL_EDGE: 3,
  MIXED_EDGE: 4,
  FOREST_EDGE: 5
};

export const SiteCornerType = {
  STREET_CORNER: 0,
  PLAZA_CORNER: 1,
  PARK_CORNER: 2,
  WATER_CORNER: 3,
  MIXED_CORNER: 4,
  NO_CORNER: 5
};

export class RoadSegment {
  constructor(start, end, width, hierarchy, name, curveId) {
    this.start = start;
    this.end = end;
    this.direction = normalize(sub(end, start));
    this.width = width || 6.0;
    this.hierarchy = hierarchy;
    this.name = name;
    this.curveId = curveId;
  }
  
  getLength() {
    return distance(this.start, this.end);
  }
  
  getMidpoint() {
    return pt((this.start.x + this.end.x) * 0.5, (this.start.y + this.end.y) * 0.5);
  }
  
  getDistanceToPoint(p) {
    const closest = segmentClosestPoint(p, this.start, this.end);
    return distance(p, closest);
  }
}

export class ParcelAnalyzer {
  constructor(siteGen) {
    this.siteGen = siteGen;
    this.roadSegments = [];
    this.analyzedPlots = [];
    
    // Distance thresholds (scaled by 0.1 logic implicitly matches C++ constants)
    this.WATER_EDGE_THRESHOLD = 5.0;
    this.PARK_EDGE_THRESHOLD = 3.0;
    this.FOREST_EDGE_THRESHOLD = 5.0;
    this.ROAD_ADJACENCY_THRESHOLD = 1.5;
    this.CORNER_DETECTION_ANGLE = 45.0;
    
    if (this.siteGen) {
      this.buildRoadSegments();
    }
  }
  
  buildRoadSegments() {
    this.roadSegments = [];
    
    const majorRoads = this.siteGen.getMajorRoads();
    for (let i = 0; i < majorRoads.length; i++) {
        this.processRoadCurve(majorRoads[i], RoadHierarchy.MAJOR_ARTERIAL, i);
    }
    
    const localRoads = this.siteGen.getLocalRoads();
    for (let i = 0; i < localRoads.length; i++) {
        this.processRoadCurve(localRoads[i], RoadHierarchy.LOCAL_STREET, majorRoads.length + i);
    }
    console.log(`Built ${this.roadSegments.length} road segments`);
  }
  
  processRoadCurve(curve, hierarchy, curveId) {
    if (curve.points.length < 2) return;
    for (let i=0; i < curve.points.length - 1; i++) {
        this.roadSegments.push(new RoadSegment(
            curve.points[i], 
            curve.points[i+1], 
            curve.width, 
            hierarchy, 
            curve.name, 
            curveId
        ));
    }
  }
  
  analyzeAllParcels() {
    console.log("=== ANALYZING ALL PARCELS ===");
    if (!this.siteGen) return;
    
    const parcels = this.siteGen.getParcels();
    this.analyzedPlots = [];
    
    for (let i = 0; i < parcels.length; i++) {
      const p = parcels[i];
      let plot = {
        id: i,
        parentParcelId: p.id,
        boundary: p.boundary,
        center: p.center,
        area: p.area,
        radius: p.radius,
        perimeterLength: 0,
        aspectRatio: 1.0
      };
      
      this.calculateBasicMetrics(plot);
      
      plot.category = this.classifyParcelBySize(plot);
      plot.edgeCondition = this.detectEdgeConditions(plot);
      plot.cornerCondition = this.detectCornerConditions(plot);
      plot.adjacentRoads = this.getAdjacentRoads(plot);
      plot.primaryStreetFrontage = this.getPrimaryStreetFrontage(plot);
      plot.streetFrontageLength = this.calculateTotalStreetFrontage(plot);
      plot.setbackDistance = this.calculateRequiredSetback(plot);
      
      this.analyzedPlots.push(plot);
    }
    console.log(`Analyzed ${this.analyzedPlots.length} parcels`);
  }
  
  calculateBasicMetrics(plot) {
    let pLen = 0;
    for (let i=0; i<plot.boundary.length; i++) {
      const j = (i + 1) % plot.boundary.length;
      pLen += distance(plot.boundary[i], plot.boundary[j]);
    }
    plot.perimeterLength = pLen;
    
    if (plot.boundary.length >= 3) {
      let minX=Infinity, maxX=-Infinity, minY=Infinity, maxY=-Infinity;
      for (const pt of plot.boundary) {
         if (pt.x < minX) minX = pt.x;
         if (pt.x > maxX) maxX = pt.x;
         if (pt.y < minY) minY = pt.y;
         if (pt.y > maxY) maxY = pt.y;
      }
      const w = maxX - minX;
      const h = maxY - minY;
      plot.aspectRatio = w / Math.max(h, 0.1);
    }
  }
  
  classifyParcelBySize(plot) {
    const largeThreshold = this.siteGen?.largeParcelThreshold ?? 100.0;
    const mediumThreshold = this.siteGen?.mediumParcelThreshold ?? 80.0;
    if (plot.area >= largeThreshold) return ParcelCategory.LARGE_PARCEL;
    else if (plot.area >= mediumThreshold) return ParcelCategory.MEDIUM_PARCEL;
    else return ParcelCategory.SMALL_PARCEL;
  }
  
  getDistanceToWater(plot) {
    const waterBoundaries = this.siteGen.getWaterBoundaries();
    let minD = 1000.0;
    for (const w of waterBoundaries) {
      for (let i=0; i < w.points.length-1; i++) {
         const d = distance(plot.center, segmentClosestPoint(plot.center, w.points[i], w.points[i+1]));
         if (d < minD) minD = d;
      }
      if (w.isClosed) {
         const d = distance(plot.center, segmentClosestPoint(plot.center, w.points[w.points.length-1], w.points[0]));
         if (d < minD) minD = d;
      }
    }
    return minD;
  }
  
  getDistanceToForest(plot) {
    const forestBoundaries = this.siteGen.getForestBoundaries();
    let minD = 1000.0;
    let closestPt = null;
    for (const f of forestBoundaries) {
      for (let i=0; i < f.points.length-1; i++) {
         const pt = segmentClosestPoint(plot.center, f.points[i], f.points[i+1]);
         const d = distance(plot.center, pt);
         if (d < minD) {
             minD = d;
             closestPt = pt;
         }
      }
      if (f.isClosed) {
         const pt = segmentClosestPoint(plot.center, f.points[f.points.length-1], f.points[0]);
         const d = distance(plot.center, pt);
         if (d < minD) {
             minD = d;
             closestPt = pt;
         }
      }
    }
    return { distance: minD, closestPt: closestPt };
  }

  getDistanceToPark(plot) {
    const parkBoundaries = this.siteGen.getParkBoundaries();
    let minD = 1000.0;
    for (const p of parkBoundaries) {
      for (let i=0; i < p.points.length-1; i++) {
         const d = distance(plot.center, segmentClosestPoint(plot.center, p.points[i], p.points[i+1]));
         if (d < minD) minD = d;
      }
      if (p.isClosed) {
         const d = distance(plot.center, segmentClosestPoint(plot.center, p.points[p.points.length-1], p.points[0]));
         if (d < minD) minD = d;
      }
    }
    return minD;
  }

  getClosestParkInfo(plot) {
    const parkBoundaries = this.siteGen.getParkBoundaries();
    let minD = 1000.0;
    let closestPt = null;
    let closestParkArea = 0;

    for (const p of parkBoundaries) {
      for (let i = 0; i < p.points.length - 1; i++) {
        const cp = segmentClosestPoint(plot.center, p.points[i], p.points[i + 1]);
        const d = distance(plot.center, cp);
        if (d < minD) {
          minD = d;
          closestPt = cp;
          closestParkArea = this.computeCurveArea(p);
        }
      }
      if (p.isClosed) {
        const cp = segmentClosestPoint(plot.center, p.points[p.points.length - 1], p.points[0]);
        const d = distance(plot.center, cp);
        if (d < minD) {
          minD = d;
          closestPt = cp;
          closestParkArea = this.computeCurveArea(p);
        }
      }
    }

    return { distance: minD, closestPt, parkArea: closestParkArea };
  }
  
  getDistanceToMajorRoad(plot) {
      let minD = 1000.0;
      for (const seg of this.roadSegments) {
          if (seg.hierarchy === RoadHierarchy.MAJOR_ARTERIAL) {
              const d = seg.getDistanceToPoint(plot.center);
              if (d < minD) minD = d;
          }
      }
      return minD;
  }
  
  detectEdgeConditions(plot) {
    const dWater = this.getDistanceToWater(plot);
    const dPark = this.getDistanceToPark(plot);
    const dRoad = this.getDistanceToMajorRoad(plot);
    const forestInfo = this.getDistanceToForest(plot);
    plot.forestDistance = forestInfo.distance;
    plot.forestClosestPoint = forestInfo.closestPt;
    
    let edges = [];
    if (forestInfo.distance < this.FOREST_EDGE_THRESHOLD) {
        edges.push(SiteEdgeType.FOREST_EDGE);
        plot.forestDirection = normalize(sub(forestInfo.closestPt, plot.center));
    }
    if (dWater < this.WATER_EDGE_THRESHOLD) edges.push(SiteEdgeType.WATER_EDGE);
    if (dPark < this.PARK_EDGE_THRESHOLD) edges.push(SiteEdgeType.PARK_EDGE);
    if (dRoad < this.ROAD_ADJACENCY_THRESHOLD) edges.push(SiteEdgeType.STREET_EDGE);
    
    if (edges.length === 0) return SiteEdgeType.INTERNAL_EDGE;
    if (edges.length === 1) return edges[0];
    
    // If mixed, prioritize forest
    if (edges.includes(SiteEdgeType.FOREST_EDGE)) return SiteEdgeType.FOREST_EDGE;
    return SiteEdgeType.MIXED_EDGE;
  }
  
  detectCornerConditions(plot) {
      let corners = [];
      const dWater = this.getDistanceToWater(plot);
      const parkInfo = this.getClosestParkInfo(plot);
      const dPark = parkInfo.distance;
      const highTrafficIntersection = this.getClosestHighTrafficIntersection(plot);
      
      const nearIntersection = !!highTrafficIntersection;
      if (nearIntersection) corners.push(SiteCornerType.STREET_CORNER);
      if (dWater < this.WATER_EDGE_THRESHOLD) corners.push(SiteCornerType.WATER_CORNER);
      if (dPark < this.PARK_EDGE_THRESHOLD) {
          if (parkInfo.parkArea > 0 && parkInfo.parkArea < 5000.0) corners.push(SiteCornerType.PLAZA_CORNER);
          else corners.push(SiteCornerType.PARK_CORNER);
      }

      let cornerType = SiteCornerType.NO_CORNER;
      if (corners.length === 1) cornerType = corners[0];
      else if (corners.length > 1) cornerType = SiteCornerType.MIXED_CORNER;

      // Corner anchor used by building generators to bias tower location to edge/corner features.
      plot.cornerAnchor = null;
      if (cornerType === SiteCornerType.PLAZA_CORNER || cornerType === SiteCornerType.PARK_CORNER) {
          plot.cornerAnchor = parkInfo.closestPt || null;
      } else if (cornerType === SiteCornerType.STREET_CORNER || cornerType === SiteCornerType.MIXED_CORNER) {
          plot.cornerAnchor = highTrafficIntersection ? highTrafficIntersection.point : (parkInfo.closestPt || null);
      }

      return cornerType;
  }

  getClosestHighTrafficIntersection(plot) {
      const INTERSECTION_THRESHOLD = 12.0;
      let best = null;

      for (let i = 0; i < this.roadSegments.length; i++) {
          for (let j = i + 1; j < this.roadSegments.length; j++) {
              const segA = this.roadSegments[i];
              const segB = this.roadSegments[j];
              const hit = lineSegmentIntersection(segA.start, segA.end, segB.start, segB.end);
              if (!hit) continue;

              const d = distance(plot.center, hit);
              if (d > INTERSECTION_THRESHOLD) continue;

              const trafficScore = (4 - segA.hierarchy) + (4 - segB.hierarchy);
              if (!best || trafficScore > best.trafficScore || (trafficScore === best.trafficScore && d < best.distance)) {
                  best = { point: hit, distance: d, trafficScore };
              }
          }
      }

      return best;
  }
  
  isNearRoadIntersection(plot) {
    const INTERSECTION_THRESHOLD = 10.0;
    for (let i=0; i<this.roadSegments.length; i++) {
        for (let j=i+1; j<this.roadSegments.length; j++) {
           const hit = lineSegmentIntersection(this.roadSegments[i].start, this.roadSegments[i].end, this.roadSegments[j].start, this.roadSegments[j].end);
           if (hit && distance(plot.center, hit) < INTERSECTION_THRESHOLD) {
               return true;
           }
        }
    }
    return false;
  }
  
  isNearPlaza(plot) {
    const parkBoundaries = this.siteGen.getParkBoundaries();
    for (const p of parkBoundaries) {
      let minD = 1000.0;
      for (let i=0; i < p.points.length-1; i++) {
         const d = distance(plot.center, segmentClosestPoint(plot.center, p.points[i], p.points[i+1]));
         if (d < minD) minD = d;
      }
      if (p.isClosed) {
         const d = distance(plot.center, segmentClosestPoint(plot.center, p.points[p.points.length-1], p.points[0]));
         if (d < minD) minD = d;
      }
      if (minD < this.PARK_EDGE_THRESHOLD) {
          // approx area
          let a = 0;
          for (let i=0; i<p.points.length; i++) {
              let j = (i+1)%p.points.length;
              a += p.points[i].x * p.points[j].y - p.points[j].x * p.points[i].y;
          }
          let area = Math.abs(a) * 0.5;
          if (area < 5000.0) return true;
      }
    }
    return false;
  }

  computeCurveArea(curve) {
    if (!curve || !curve.points || curve.points.length < 3) return 0;
    let a = 0;
    for (let i = 0; i < curve.points.length; i++) {
        const j = (i + 1) % curve.points.length;
        a += curve.points[i].x * curve.points[j].y - curve.points[j].x * curve.points[i].y;
    }
    return Math.abs(a) * 0.5;
  }
  
  getDistanceFromParcelToRoad(plot, road) {
      let minD = 1000.0;
      for (const pt of plot.boundary) {
          const d = road.getDistanceToPoint(pt);
          if (d < minD) minD = d;
      }
      return minD;
  }
  
  calculateFrontageLength(plot, road) {
      let fLen = 0;
      for (let i=0; i < plot.boundary.length; i++) {
          const j = (i+1)%plot.boundary.length;
          const d1 = road.getDistanceToPoint(plot.boundary[i]);
          const d2 = road.getDistanceToPoint(plot.boundary[j]);
          if (d1 < this.ROAD_ADJACENCY_THRESHOLD && d2 < this.ROAD_ADJACENCY_THRESHOLD) {
              fLen += distance(plot.boundary[i], plot.boundary[j]);
          }
      }
      return fLen;
  }
  
  getAdjacentRoads(plot) {
      let adj = [];
      for (const seg of this.roadSegments) {
          const dist = this.getDistanceFromParcelToRoad(plot, seg);
          if (dist < this.ROAD_ADJACENCY_THRESHOLD) {
             adj.push({
                 segment: seg,
                 distanceToParcel: dist,
                 frontageLength: this.calculateFrontageLength(plot, seg),
                 isPrimaryFrontage: false
             });
          }
      }
      
      adj.sort((a,b) => {
         if (a.segment.hierarchy !== b.segment.hierarchy) return a.segment.hierarchy - b.segment.hierarchy;
         if (Math.abs(a.frontageLength - b.frontageLength) > 5.0) return b.frontageLength - a.frontageLength;
         return a.distanceToParcel - b.distanceToParcel;
      });
      
      if (adj.length > 0) adj[0].isPrimaryFrontage = true;
      return adj;
  }
  
  getClosestPointOnParcelToRoad(plot, road) {
      let closest = plot.center;
      let minD = 1000.0;
      for (const pt of plot.boundary) {
          const d = road.getDistanceToPoint(pt);
          if (d < minD) {
              minD = d;
              closest = pt;
          }
      }
      return closest;
  }
  
  getPrimaryStreetFrontage(plot) {
      const adj = plot.adjacentRoads || this.getAdjacentRoads(plot);
      if (adj.length === 0) return plot.center;
      return this.getClosestPointOnParcelToRoad(plot, adj[0].segment);
  }
  
  calculateTotalStreetFrontage(plot) {
      let total = 0;
      for (const road of this.roadSegments) {
          total += this.calculateFrontageLength(plot, road);
      }
      return total;
  }
  
  calculateRequiredSetback(plot) {
      let base = 5.0;
      if (plot.category === ParcelCategory.LARGE_PARCEL) base = 1.0;
      else if (plot.category === ParcelCategory.MEDIUM_PARCEL) base = 0.7;
      else if (plot.category === ParcelCategory.SMALL_PARCEL) base = 0.5;
      
      if (plot.edgeCondition === SiteEdgeType.WATER_EDGE) base *= 1.5;
      else if (plot.edgeCondition === SiteEdgeType.PARK_EDGE) base *= 1.2;
      return base;
  }
}
