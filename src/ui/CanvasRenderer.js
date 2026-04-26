import { CurveType, ParcelCategory } from '../core/SiteGeneration.js';
import { RoadHierarchy, SiteEdgeType, SiteCornerType } from '../core/ParcelAnalyzer.js';
import { OpenSpaceType } from '../core/ParcelSubdivider.js';

export class CanvasRenderer {
  constructor(canvasElement) {
    this.canvas = canvasElement;
    this.ctx = this.canvas.getContext('2d');
    this.panX = 0;
    this.panY = 0;
    this.scale = 1;
    this.minScale = 0.05;
    this.maxScale = 500;
    this.isPanning = false;
    this.dragMoved = false;
    this.lastMouseX = 0;
    this.lastMouseY = 0;
    this.locationPin = null;
    this.onLocationPin = null;
    this.navigatorPose = null;
    this.resizeCanvas();
    window.addEventListener('resize', () => this.resizeCanvas());
    this.setupInteractions();
  }

  resizeCanvas() {
    const parent = this.canvas.parentElement;
    if (parent && (this.canvas.width !== parent.clientWidth || this.canvas.height !== parent.clientHeight)) {
      this.canvas.width = parent.clientWidth;
      this.canvas.height = parent.clientHeight;
    }
  }

  clear() {
    this.resizeCanvas();
    this.ctx.fillStyle = '#f2f2f2'; // Background 0.95 equivalent
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
  }

  setupInteractions() {
    this.canvas.addEventListener('mousedown', (e) => {
      if (e.button !== 0) return;
      this.isPanning = true;
      this.dragMoved = false;
      this.lastMouseX = e.clientX;
      this.lastMouseY = e.clientY;
      this.canvas.style.cursor = 'grabbing';
    });

    window.addEventListener('mousemove', (e) => {
      if (!this.isPanning) return;
      const dx = e.clientX - this.lastMouseX;
      const dy = e.clientY - this.lastMouseY;
      this.lastMouseX = e.clientX;
      this.lastMouseY = e.clientY;
      if (Math.abs(dx) > 1 || Math.abs(dy) > 1) this.dragMoved = true;
      this.panX += dx;
      this.panY += dy;
    });

    window.addEventListener('mouseup', () => {
      if (!this.isPanning) return;
      this.isPanning = false;
      this.canvas.style.cursor = 'default';
    });

    this.canvas.addEventListener('click', (e) => {
      if (this.dragMoved) return;
      const world = this.screenToWorld(e.offsetX, e.offsetY);
      this.locationPin = world;
      if (this.onLocationPin) this.onLocationPin(world);
    });

    this.canvas.addEventListener('wheel', (e) => {
      e.preventDefault();
      const zoomFactor = Math.exp(-e.deltaY * 0.0015);
      const oldScale = this.scale;
      const newScale = Math.max(this.minScale, Math.min(this.maxScale, oldScale * zoomFactor));
      if (Math.abs(newScale - oldScale) < 1e-8) return;

      // Zoom toward cursor point
      const wx = (e.offsetX - this.panX) / oldScale;
      const wy = (e.offsetY - this.panY) / oldScale;
      this.scale = newScale;
      this.panX = e.offsetX - wx * newScale;
      this.panY = e.offsetY - wy * newScale;
    }, { passive: false });
  }

  setLocationPinCallback(callback) {
    this.onLocationPin = callback;
  }

  setPinnedLocation(point) {
    this.locationPin = point;
  }

  setNavigatorPose(pose) {
    this.navigatorPose = pose ? { ...pose } : null;
  }

  screenToWorld(screenX, screenY) {
    return {
      x: (screenX - this.panX) / this.scale,
      y: (screenY - this.panY) / this.scale
    };
  }

  // Set transform so everything fits, simple auto-fit based on boundaries
  autoFit(curves) {
    this.resizeCanvas();
    if (!curves || curves.length === 0) return;
    
    let minX = Infinity, minY = Infinity;
    let maxX = -Infinity, maxY = -Infinity;

    for (const c of curves) {
      for (const p of c.points) {
        if (p.x < minX) minX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.x > maxX) maxX = p.x;
        if (p.y > maxY) maxY = p.y;
      }
    }

    const padding = 50;
    const width = maxX - minX || 1;
    const height = maxY - minY || 1;

    const scaleX = (this.canvas.width - padding * 2) / width;
    const scaleY = (this.canvas.height - padding * 2) / height;
    
    this.scale = Math.min(scaleX, scaleY);
    this.panX = (this.canvas.width - width * this.scale) / 2 - minX * this.scale;
    this.panY = (this.canvas.height - height * this.scale) / 2 - minY * this.scale;
  }

  drawSite(siteGen, parcelAnalyzer, roadAnalyzer, parcelSubdivider, footprintGenerator, options = {}) {
    this.clear();
    
    this.ctx.save();
    this.ctx.translate(this.panX, this.panY);
    this.ctx.scale(this.scale, this.scale);

    this.drawGrid(5.0);

    if (options.showCurves && siteGen) {
      this.drawCurves(siteGen);
    }
    
    if (options.showParcels && siteGen && !options.showSubdividedPlots && !options.showBuildingFootprints) {
      this.drawParcels(siteGen.getParcels());
    }

    if (options.showCenters && siteGen) {
      this.drawCenters(siteGen.getParcels());
    }

    if (parcelAnalyzer) {
      if (options.showAnalysis) this.drawAnalysisResults(parcelAnalyzer);
      if (options.showRoadSegments) this.drawRoadSegments(parcelAnalyzer);
      if (options.showEdgeConditions) this.drawEdgeConditions(parcelAnalyzer);
      if (options.showCornerConditions) this.drawCornerConditions(parcelAnalyzer);
      if (options.showRoadFrontages) this.drawRoadFrontages(parcelAnalyzer);
    }

    if (roadAnalyzer) {
      if (options.showAnalyzedRoads) this.drawAnalyzedRoads(roadAnalyzer);
      if (options.showIntersections) this.drawIntersections(roadAnalyzer);
      if (options.showNetworkPattern) this.drawNetworkPattern(roadAnalyzer);
    }

    if (parcelSubdivider && options.showSubdividedPlots) {
        this.drawSubdividedPlots(parcelSubdivider);
        this.drawOpenSpaces(parcelSubdivider);
    }

    if (footprintGenerator && options.showBuildingFootprints) {
        if (!options.showSubdividedPlots) {
           this.drawSubdividedPlots(parcelSubdivider);
           this.drawOpenSpaces(parcelSubdivider);
        }
        this.drawBuildingFootprints(footprintGenerator);
    }

    this.drawLocationPin();
    this.drawNavigatorCone();

    this.ctx.restore();
  }

  drawLocationPin() {
    if (!this.locationPin) return;
    const pinSize = Math.max(0.6, 8 / this.scale);
    this.ctx.strokeStyle = '#ff2a2a';
    this.ctx.fillStyle = 'rgba(255, 42, 42, 0.2)';
    this.ctx.lineWidth = Math.max(0.12, 1.5 / this.scale);
    this.ctx.beginPath();
    this.ctx.arc(this.locationPin.x, this.locationPin.y, pinSize, 0, Math.PI * 2);
    this.ctx.fill();
    this.ctx.stroke();

    this.ctx.beginPath();
    this.ctx.moveTo(this.locationPin.x - pinSize * 1.4, this.locationPin.y);
    this.ctx.lineTo(this.locationPin.x + pinSize * 1.4, this.locationPin.y);
    this.ctx.moveTo(this.locationPin.x, this.locationPin.y - pinSize * 1.4);
    this.ctx.lineTo(this.locationPin.x, this.locationPin.y + pinSize * 1.4);
    this.ctx.stroke();
  }

  drawNavigatorCone() {
    if (!this.navigatorPose || !this.navigatorPose.active) return;
    const x = this.navigatorPose.x;
    const y = this.navigatorPose.y;
    const yaw = this.navigatorPose.yaw || 0;

    const radius = Math.max(1.2, 42 / this.scale);
    const halfFov = Math.PI / 8; // 45° total cone
    const a0 = yaw - halfFov;
    const a1 = yaw + halfFov;

    this.ctx.fillStyle = 'rgba(44, 156, 255, 0.22)';
    this.ctx.strokeStyle = 'rgba(44, 156, 255, 0.9)';
    this.ctx.lineWidth = Math.max(0.12, 1.5 / this.scale);

    this.ctx.beginPath();
    this.ctx.moveTo(x, y);
    this.ctx.arc(x, y, radius, a0, a1);
    this.ctx.closePath();
    this.ctx.fill();
    this.ctx.stroke();

    // Heading ray
    const hx = x + Math.sin(yaw) * radius * 1.15;
    const hy = y + Math.cos(yaw) * radius * 1.15;
    this.ctx.beginPath();
    this.ctx.moveTo(x, y);
    this.ctx.lineTo(hx, hy);
    this.ctx.stroke();

    // Navigator center marker
    const markerR = Math.max(0.5, 5 / this.scale);
    this.ctx.fillStyle = '#2c9cff';
    this.ctx.beginPath();
    this.ctx.arc(x, y, markerR, 0, Math.PI * 2);
    this.ctx.fill();
    this.ctx.strokeStyle = '#ffffff';
    this.ctx.lineWidth = Math.max(0.1, 1.0 / this.scale);
    this.ctx.stroke();
  }

  drawGrid(size) {
    this.ctx.strokeStyle = '#e6e6e6';
    this.ctx.lineWidth = 1.0 / this.scale;
    const viewWidth = this.canvas.width / this.scale;
    const viewHeight = this.canvas.height / this.scale;
    
    this.ctx.beginPath();
    for (let x = -1000; x < 1000; x += size) {
        this.ctx.moveTo(x, -1000);
        this.ctx.lineTo(x, 1000);
    }
    for (let y = -1000; y < 1000; y += size) {
        this.ctx.moveTo(-1000, y);
        this.ctx.lineTo(1000, y);
    }
    this.ctx.stroke();
  }

  drawSmoothCurve(curve, strokeStyle, lineWidth) {
    if (curve.points.length < 2) return;
    this.ctx.beginPath();
    this.ctx.moveTo(curve.points[0].x, curve.points[0].y);
    for (let i = 1; i < curve.points.length; i++) {
        this.ctx.lineTo(curve.points[i].x, curve.points[i].y);
    }
    if (curve.isClosed) {
        this.ctx.closePath();
    }
    this.ctx.strokeStyle = strokeStyle;
    // Scale line width inverse to view scale so lines don't get too thick
    this.ctx.lineWidth = lineWidth / this.scale; 
    this.ctx.stroke();
  }

  drawCurves(siteGen) {
    // drawing major roads
    for (const road of siteGen.getMajorRoads()) {
      this.drawSmoothCurve(road, '#FF0000', 4.0);
    }
    // drawing local roads
    for (const road of siteGen.getLocalRoads()) {
      this.drawSmoothCurve(road, '#808080', 2.0);
    }
    // drawing water
    for (const water of siteGen.getWaterBoundaries()) {
      this.drawSmoothCurve(water, '#00B2FF', 3.0);
    }
    // drawing park
    for (const park of siteGen.getParkBoundaries()) {
      this.drawSmoothCurve(park, '#00CC00', 2.0);
    }
    // drawing site
    for (const site of siteGen.getSiteBoundaries()) {
      this.drawSmoothCurve(site, '#000000', 2.0);
    }
    // drawing tram
    for (const tram of siteGen.getTramLines()) {
      this.drawSmoothCurve(tram, '#FF00FF', 2.5);
    }
    // drawing railway
    for (const rail of siteGen.getRailways()) {
      this.drawSmoothCurve(rail, '#000000', 3.0);
    }
    // drawing forest
    for (const forest of siteGen.getForestBoundaries()) {
      this.drawSmoothCurve(forest, '#228B22', 2.0);
    }
    // drawing parcels
    for (const parcel of siteGen.getParcelBoundaries()) {
      this.drawSmoothCurve(parcel, '#CCCCCC', 1.0);
    }
  }

  drawParcels(parcels) {
    for (const parcel of parcels) {
      if (parcel.boundary.length < 3) continue;
      this.ctx.beginPath();
      this.ctx.moveTo(parcel.boundary[0].x, parcel.boundary[0].y);
      for (let i = 1; i < parcel.boundary.length; i++) {
        this.ctx.lineTo(parcel.boundary[i].x, parcel.boundary[i].y);
      }
      this.ctx.closePath();
      
      this.ctx.strokeStyle = '#222222';
      this.ctx.lineWidth = 1.0 / this.scale;
      this.ctx.stroke();

      this.ctx.fillStyle = 'rgba(200, 200, 200, 0.2)';
      this.ctx.fill();
    }
  }

  drawCenters(parcels) {
    for (const parcel of parcels) {
      if (!parcel.center) continue;
      this.ctx.beginPath();
      this.ctx.arc(parcel.center.x, parcel.center.y, Math.max(0.5, 4 / this.scale), 0, Math.PI * 2);
      this.ctx.fillStyle = '#FF5500';
      this.ctx.fill();
    }
  }

  drawAnalysisResults(analyzer) {
    for (const plot of analyzer.analyzedPlots) {
      if (plot.boundary.length < 3) continue;
      
      if (plot.category === ParcelCategory.LARGE_PARCEL) this.ctx.fillStyle = 'rgba(0, 255, 0, 0.4)';
      else if (plot.category === ParcelCategory.MEDIUM_PARCEL) this.ctx.fillStyle = 'rgba(255, 255, 0, 0.4)';
      else if (plot.category === ParcelCategory.SMALL_PARCEL) this.ctx.fillStyle = 'rgba(255, 0, 0, 0.4)';
      else continue;

      this.ctx.beginPath();
      this.ctx.moveTo(plot.boundary[0].x, plot.boundary[0].y);
      for (let i = 1; i < plot.boundary.length; i++) {
        this.ctx.lineTo(plot.boundary[i].x, plot.boundary[i].y);
      }
      this.ctx.closePath();
      this.ctx.fill();
    }
  }

  drawRoadSegments(analyzer) {
    for (const seg of analyzer.roadSegments) {
       switch(seg.hierarchy) {
           case RoadHierarchy.MAJOR_ARTERIAL: 
               this.ctx.strokeStyle = '#FF0000'; 
               this.ctx.lineWidth = 1.0 / this.scale; break;
           case RoadHierarchy.MINOR_ARTERIAL: 
               this.ctx.strokeStyle = '#CC6600'; 
               this.ctx.lineWidth = 0.3 / this.scale; break;
           case RoadHierarchy.COLLECTOR: 
               this.ctx.strokeStyle = '#999900'; 
               this.ctx.lineWidth = 0.2 / this.scale; break;
           case RoadHierarchy.LOCAL_STREET: 
               this.ctx.strokeStyle = '#808080'; 
               this.ctx.lineWidth = 0.1 / this.scale; break;
       }
       this.ctx.beginPath();
       this.ctx.moveTo(seg.start.x, seg.start.y);
       this.ctx.lineTo(seg.end.x, seg.end.y);
       this.ctx.stroke();

       const midX = (seg.start.x + seg.end.x)*0.5;
       const midY = (seg.start.y + seg.end.y)*0.5;
       const arrX = midX + seg.direction.x * (seg.width * 0.05);
       const arrY = midY + seg.direction.y * (seg.width * 0.05);

       this.ctx.strokeStyle = '#0000FF';
       this.ctx.lineWidth = 0.5 / this.scale;
       this.ctx.beginPath();
       this.ctx.moveTo(midX, midY);
       this.ctx.lineTo(arrX, arrY);
       this.ctx.stroke();
    }
  }

  drawEdgeConditions(analyzer) {
      for (const plot of analyzer.analyzedPlots) {
          if (plot.edgeCondition === SiteEdgeType.INTERNAL_EDGE) continue;
          
          switch(plot.edgeCondition) {
              case SiteEdgeType.WATER_EDGE: this.ctx.strokeStyle = '#0000FF'; break;
              case SiteEdgeType.PARK_EDGE: this.ctx.strokeStyle = '#00CC00'; break;
              case SiteEdgeType.STREET_EDGE: this.ctx.strokeStyle = '#999999'; break;
              case SiteEdgeType.MIXED_EDGE: this.ctx.strokeStyle = '#FF00FF'; break;
          }
          this.ctx.lineWidth = 1.0 / this.scale;
          this.ctx.beginPath();
          this.ctx.moveTo(plot.boundary[0].x, plot.boundary[0].y);
          for (let i = 1; i < plot.boundary.length; i++) {
             this.ctx.lineTo(plot.boundary[i].x, plot.boundary[i].y);
          }
          this.ctx.closePath();
          this.ctx.stroke();
      }
  }

  drawCornerConditions(analyzer) {
      for (const plot of analyzer.analyzedPlots) {
          if (plot.cornerCondition === SiteCornerType.NO_CORNER) continue;

          switch(plot.cornerCondition) {
              case SiteCornerType.WATER_CORNER: this.ctx.fillStyle = '#0000FF'; break;
              case SiteCornerType.PARK_CORNER: this.ctx.fillStyle = '#00CC00'; break;
              case SiteCornerType.PLAZA_CORNER: this.ctx.fillStyle = '#FF8000'; break;
              case SiteCornerType.STREET_CORNER: this.ctx.fillStyle = '#CCCCCC'; break;
              case SiteCornerType.MIXED_CORNER: this.ctx.fillStyle = '#FF00FF'; break;
          }

          const markerSize = 0.8;
          this.ctx.fillRect(plot.center.x - markerSize, plot.center.y - markerSize, markerSize*2, markerSize*2);
      }
  }

  drawRoadFrontages(analyzer) {
      for (const plot of analyzer.analyzedPlots) {
          if (!plot.adjacentRoads || plot.adjacentRoads.length === 0) continue;
          
          let primaryRoad = plot.adjacentRoads.find(r => r.isPrimaryFrontage);
          if (!primaryRoad) continue;

          this.ctx.strokeStyle = '#00FFFF';
          this.ctx.lineWidth = 0.2 / this.scale;
          this.ctx.beginPath();
          this.ctx.moveTo(plot.center.x, plot.center.y);
          this.ctx.lineTo(plot.primaryStreetFrontage.x, plot.primaryStreetFrontage.y);
          this.ctx.stroke();

          this.ctx.fillStyle = '#FF00FF';
          const ptSize = 0.6;
          this.ctx.beginPath();
          this.ctx.arc(plot.primaryStreetFrontage.x, plot.primaryStreetFrontage.y, ptSize, 0, Math.PI*2);
          this.ctx.fill();
      }
  }

  drawSubdividedPlots(subdivider) {
      for (const plot of subdivider.allPlots) {
          if (plot.isOpenSpace) continue;
          
          switch(plot.subdivisionLevel) {
              case 0: this.ctx.fillStyle = 'rgba(200, 200, 200, 0.4)'; break;
              case 1: this.ctx.fillStyle = 'rgba(0, 200, 200, 0.4)'; break;
              case 2: this.ctx.fillStyle = 'rgba(0, 200, 0, 0.4)'; break;
              case 4: this.ctx.fillStyle = 'rgba(200, 0, 200, 0.4)'; break;
              default: this.ctx.fillStyle = 'rgba(255, 255, 0, 0.4)'; break;
          }

          if (plot.boundary.length >= 3) {
              this.ctx.beginPath();
              this.ctx.moveTo(plot.boundary[0].x, plot.boundary[0].y);
              for (let i = 1; i < plot.boundary.length; i++) {
                 this.ctx.lineTo(plot.boundary[i].x, plot.boundary[i].y);
              }
              this.ctx.closePath();
              this.ctx.fill();

              this.ctx.strokeStyle = '#000000';
              this.ctx.lineWidth = 1.0 / this.scale;
              this.ctx.stroke();
          }
      }
  }

  drawOpenSpaces(subdivider) {
      for (const space of subdivider.allOpenSpaces) {
          switch(space.type) {
              case OpenSpaceType.CENTRAL_PARK: this.ctx.fillStyle = 'rgba(0, 200, 0, 0.7)'; break;
              case OpenSpaceType.PLAZA: this.ctx.fillStyle = 'rgba(230, 230, 150, 0.7)'; break;
              case OpenSpaceType.COURTYARD: this.ctx.fillStyle = 'rgba(150, 200, 255, 0.7)'; break;
              case OpenSpaceType.LINEAR_PARK: this.ctx.fillStyle = 'rgba(100, 200, 100, 0.7)'; break;
              case OpenSpaceType.POCKET_PARK: this.ctx.fillStyle = 'rgba(150, 230, 150, 0.7)'; break;
              case OpenSpaceType.GREEN_BUFFER: this.ctx.fillStyle = 'rgba(180, 200, 180, 0.7)'; break;
              default: this.ctx.fillStyle = 'rgba(200, 200, 200, 0.7)'; break;
          }

          if (space.boundary.length >= 3) {
              this.ctx.beginPath();
              this.ctx.moveTo(space.boundary[0].x, space.boundary[0].y);
              for (let i = 1; i < space.boundary.length; i++) {
                 this.ctx.lineTo(space.boundary[i].x, space.boundary[i].y);
              }
              this.ctx.closePath();
              this.ctx.fill();

              this.ctx.strokeStyle = '#008000';
              this.ctx.lineWidth = 2.0 / this.scale;
              this.ctx.stroke();
              
              if ([OpenSpaceType.CENTRAL_PARK, OpenSpaceType.POCKET_PARK, OpenSpaceType.LINEAR_PARK, OpenSpaceType.GREEN_BUFFER].includes(space.type)) {
                  this.drawTreeSymbols(space);
              }
          }
      }
  }

  drawTreeSymbols(space) {
      let numTrees = Math.floor(3 + space.area / 20.0);
      if (space.type === OpenSpaceType.GREEN_BUFFER) numTrees = 2;
      
      const treeRng = (seed) => {
         let x = Math.sin(seed++) * 10000;
         return x - Math.floor(x);
      };
      
      let seed = space.id * 100;
      for (let i = 0; i < numTrees; i++) {
          let radius = Math.sqrt(treeRng(seed++)) * Math.sqrt(space.area / Math.PI) * 0.8;
          let angle = treeRng(seed++) * 2.0 * Math.PI;
          
          let tx = space.center.x + radius * Math.cos(angle);
          let ty = space.center.y + radius * Math.sin(angle);
          
          let treeSize = 0.3 + treeRng(seed++) * 0.3;
          let gx = 0.4 + treeRng(seed++) * 0.2;
          
          this.ctx.fillStyle = `rgba(0, ${Math.floor(gx*255)}, 0, 0.8)`;
          this.ctx.beginPath();
          this.ctx.arc(tx, ty, treeSize * 5, 0, Math.PI*2);
          this.ctx.fill();
      }
  }

  drawBuildingFootprints(footprintGenerator) {
      for (const footprint of footprintGenerator.buildingFootprints) {
          if (footprint.boundary.length < 3) continue;

          this.ctx.beginPath();
          this.ctx.moveTo(footprint.boundary[0].x, footprint.boundary[0].y);
          for (let i = 1; i < footprint.boundary.length; i++) {
             this.ctx.lineTo(footprint.boundary[i].x, footprint.boundary[i].y);
          }
          this.ctx.closePath();
          
          this.ctx.fillStyle = 'rgba(50, 50, 50, 0.75)';
          this.ctx.fill();

          this.ctx.strokeStyle = '#111111';
          this.ctx.lineWidth = 1.0 / this.scale;
          this.ctx.stroke();

          // Draw Entrances
          this.ctx.fillStyle = '#FF3333';
          for (const ent of footprint.entrances) {
              const markerScale = 0.8;
              this.ctx.beginPath();
              this.ctx.arc(ent.x, ent.y, markerScale, 0, Math.PI*2);
              this.ctx.fill();
          }
      }
  }

  drawAnalyzedRoads(roadAnalyzer) {
      const RoadClassification = {
          PRIMARY_ARTERIAL: 0,
          SECONDARY_ARTERIAL: 1,
          COLLECTOR_ROAD: 2,
          LOCAL_STREET: 3,
          SERVICE_ROAD: 4
      };

      for (const segment of roadAnalyzer.analyzedSegments) {
          switch (segment.classification) {
              case RoadClassification.PRIMARY_ARTERIAL:
                  this.ctx.strokeStyle = '#FF0000';
                  this.ctx.lineWidth = 0.6 / this.scale;
                  break;
              case RoadClassification.SECONDARY_ARTERIAL:
                  this.ctx.strokeStyle = '#CC3300';
                  this.ctx.lineWidth = 0.4 / this.scale;
                  break;
              case RoadClassification.COLLECTOR_ROAD:
                  this.ctx.strokeStyle = '#996600';
                  this.ctx.lineWidth = 0.3 / this.scale;
                  break;
              case RoadClassification.LOCAL_STREET:
                  this.ctx.strokeStyle = '#808080';
                  this.ctx.lineWidth = 0.2 / this.scale;
                  break;
              case RoadClassification.SERVICE_ROAD:
                  this.ctx.strokeStyle = '#B3B3B3';
                  this.ctx.lineWidth = 0.1 / this.scale;
                  break;
              default:
                  this.ctx.strokeStyle = '#4D4D4D';
                  this.ctx.lineWidth = 0.1 / this.scale;
                  break;
          }

          this.ctx.beginPath();
          this.ctx.moveTo(segment.start.x, segment.start.y);
          this.ctx.lineTo(segment.end.x, segment.end.y);
          this.ctx.stroke();

          if (segment.classification <= RoadClassification.COLLECTOR_ROAD) {
              const mid = segment.getMidpoint();
              const arrowLength = segment.width * 0.8;
              const arrowEnd = {
                  x: mid.x + segment.direction.x * arrowLength,
                  y: mid.y + segment.direction.y * arrowLength
              };

              this.ctx.strokeStyle = '#0000FF';
              this.ctx.lineWidth = 0.2 / this.scale;
              this.ctx.beginPath();
              this.ctx.moveTo(mid.x, mid.y);
              this.ctx.lineTo(arrowEnd.x, arrowEnd.y);
              this.ctx.stroke();

              if (segment.classification <= RoadClassification.SECONDARY_ARTERIAL) {
                  const perpEnd = {
                      x: mid.x + segment.perpendicular.x * (arrowLength * 0.6),
                      y: mid.y + segment.perpendicular.y * (arrowLength * 0.6)
                  };
                  this.ctx.strokeStyle = '#00CC00';
                  this.ctx.lineWidth = 0.15 / this.scale;
                  this.ctx.beginPath();
                  this.ctx.moveTo(mid.x, mid.y);
                  this.ctx.lineTo(perpEnd.x, perpEnd.y);
                  this.ctx.stroke();
              }
          }
      }
  }

  drawIntersections(roadAnalyzer) {
      const IntersectionType = {
          T_JUNCTION: 0,
          CROSS_INTERSECTION: 1,
          Y_JUNCTION: 2,
          ROUNDABOUT: 3,
          COMPLEX_INTERSECTION: 4,
          DEAD_END: 5
      };

      for (const intersection of roadAnalyzer.intersections) {
          const intensity = Math.min(1.0, intersection.importance);
          const i255 = Math.floor(intensity * 255);
          
          switch (intersection.type) {
              case IntersectionType.CROSS_INTERSECTION:
                  this.ctx.fillStyle = `rgb(${i255}, 0, ${i255})`;
                  break;
              case IntersectionType.T_JUNCTION:
                  this.ctx.fillStyle = `rgb(0, ${i255}, ${i255})`;
                  break;
              case IntersectionType.Y_JUNCTION:
                  this.ctx.fillStyle = `rgb(${i255}, ${i255}, 0)`;
                  break;
              case IntersectionType.COMPLEX_INTERSECTION:
                  this.ctx.fillStyle = `rgb(${i255}, 0, 0)`;
                  break;
              case IntersectionType.DEAD_END:
                  this.ctx.fillStyle = '#808080';
                  break;
              default:
                  this.ctx.fillStyle = '#4D4D4D';
                  break;
          }

          const markerSize = intersection.intersectionRadius * 0.1;
          this.ctx.fillRect(
              intersection.location.x - markerSize,
              intersection.location.y - markerSize,
              markerSize * 2,
              markerSize * 2
          );

          this.ctx.fillStyle = '#000000';
          this.ctx.font = `${10 / this.scale}px Helvetica`;
          this.ctx.fillText(
              intersection.id.toString(),
              intersection.location.x + markerSize,
              intersection.location.y + markerSize
          );
      }
  }

  drawNetworkPattern(roadAnalyzer) {
      const NetworkPattern = {
          GRID_PATTERN: 0,
          RADIAL_PATTERN: 1,
          ORGANIC_PATTERN: 2,
          MIXED_PATTERN: 3,
          LINEAR_PATTERN: 4
      };

      const networkAnalysis = roadAnalyzer.networkAnalysis;

      if (networkAnalysis.dominantPattern === NetworkPattern.GRID_PATTERN) {
          const center = { x: 0, y: 0 };
          const extent = 20.0;

          this.ctx.lineWidth = 0.2 / this.scale;

          // Primary direction lines
          this.ctx.strokeStyle = '#00FF00';
          for (let i = -5; i <= 5; i++) {
              const offset = {
                  x: networkAnalysis.primaryDirection.x * (i * 5.0),
                  y: networkAnalysis.primaryDirection.y * (i * 5.0)
              };
              const start = {
                  x: center.x + offset.x - networkAnalysis.secondaryDirection.x * extent,
                  y: center.y + offset.y - networkAnalysis.secondaryDirection.y * extent
              };
              const end = {
                  x: center.x + offset.x + networkAnalysis.secondaryDirection.x * extent,
                  y: center.y + offset.y + networkAnalysis.secondaryDirection.y * extent
              };
              this.ctx.beginPath();
              this.ctx.moveTo(start.x, start.y);
              this.ctx.lineTo(end.x, end.y);
              this.ctx.stroke();
          }

          // Secondary direction lines
          this.ctx.strokeStyle = '#0000FF';
          for (let i = -5; i <= 5; i++) {
              const offset = {
                  x: networkAnalysis.secondaryDirection.x * (i * 5.0),
                  y: networkAnalysis.secondaryDirection.y * (i * 5.0)
              };
              const start = {
                  x: center.x + offset.x - networkAnalysis.primaryDirection.x * extent,
                  y: center.y + offset.y - networkAnalysis.primaryDirection.y * extent
              };
              const end = {
                  x: center.x + offset.x + networkAnalysis.primaryDirection.x * extent,
                  y: center.y + offset.y + networkAnalysis.primaryDirection.y * extent
              };
              this.ctx.beginPath();
              this.ctx.moveTo(start.x, start.y);
              this.ctx.lineTo(end.x, end.y);
              this.ctx.stroke();
          }
      }
  }

  exportHighResPNG(siteGen, parcelAnalyzer, roadAnalyzer, parcelSubdivider, footprintGenerator, options = {}, scaleFactor = 3) {
    const sf = Math.max(1, Math.floor(scaleFactor));
    const originalCanvas = this.canvas;
    const originalCtx = this.ctx;
    const originalPanX = this.panX;
    const originalPanY = this.panY;
    const originalScale = this.scale;

    const offscreen = document.createElement('canvas');
    offscreen.width = Math.max(1, originalCanvas.width * sf);
    offscreen.height = Math.max(1, originalCanvas.height * sf);

    this.canvas = offscreen;
    this.ctx = offscreen.getContext('2d');
    this.panX = originalPanX * sf;
    this.panY = originalPanY * sf;
    this.scale = originalScale * sf;

    this.drawSite(siteGen, parcelAnalyzer, roadAnalyzer, parcelSubdivider, footprintGenerator, options);

    const dataUrl = offscreen.toDataURL('image/png');

    this.canvas = originalCanvas;
    this.ctx = originalCtx;
    this.panX = originalPanX;
    this.panY = originalPanY;
    this.scale = originalScale;

    return dataUrl;
  }
}
