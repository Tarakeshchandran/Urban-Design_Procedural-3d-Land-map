import { pt, distance, centroid, polygonArea, sub, add, mul, normalize, dot } from './geometry.js';
import { ParcelCategory } from './SiteGeneration.js';
import { SiteEdgeType, SiteCornerType, RoadHierarchy } from './ParcelAnalyzer.js';

export class BuildingFootprintGenerator {
    constructor(parcelSubdivider, parcelAnalyzer) {
        this.parcelSubdivider = parcelSubdivider;
        this.parcelAnalyzer = parcelAnalyzer;
        this.buildingFootprints = [];
        this.nextFootprintId = 0;

        // Default setbacks mapped from C++ logic
        this.defaultFrontSetback = 0.5;
        this.defaultRearSetback = 0.7;
        this.defaultSideSetback = 0.5;
        this.defaultWaterSetback = 1.0;
        this.defaultParkSetback = 0.8;
        
        // Coverage Limits
        this.maxCoverageRatio = 0.7;
        this.minCoverageRatio = 0.4;
    }

    generateAllFootprints() {
        console.log("=== GENERATING BUILDING FOOTPRINTS ===");
        if (!this.parcelSubdivider) return;
        
        this.buildingFootprints = [];
        this.nextFootprintId = 0;

        const plots = this.parcelSubdivider.allPlots;

        for (const plot of plots) {
            if (plot.isOpenSpace) continue; // Skip parks
            
            let footprint = this.generateFootprintForPlot(plot);
            
            if (footprint.boundary.length >= 3) {
                footprint.id = this.nextFootprintId++;
                this.buildingFootprints.push(footprint);
            }
        }
        
        console.log(`Generated ${this.buildingFootprints.length} building footprints.`);
    }

    generateFootprintForPlot(plot) {
        let footprint = {
            parentPlotId: plot.id,
            boundary: [],
            center: {x: 0, y: 0},
            area: 0,
            coverage: 0,
            primaryEdgeType: SiteEdgeType.INTERNAL_EDGE,
            cornerType: SiteCornerType.NO_CORNER,
            hasMajorRoadFrontage: false,
            hasWaterFrontage: false,
            hasParkFrontage: false,
            frontSetback: this.defaultFrontSetback,
            rearSetback: this.defaultRearSetback,
            sideSetback: this.defaultSideSetback,
            waterSetback: this.defaultWaterSetback,
            parkSetback: this.defaultParkSetback,
            primaryOrientation: {x: 1, y: 0},
            entrances: []
        };
        
        this.determineEdgeConditions(plot, footprint);
        this.calculateSetbacks(plot, footprint);
        this.generateBuildingBoundary(plot, footprint);
        
        footprint.center = centroid(footprint.boundary);
        footprint.area = polygonArea(footprint.boundary);
        if (plot.area > 0) footprint.coverage = footprint.area / plot.area;
        
        this.determinePrimaryOrientation(plot, footprint);
        this.generateEntrances(footprint);
        
        return footprint;
    }

    determineEdgeConditions(plot, footprint) {
        const analyzedPlot = this.parcelAnalyzer.analyzedPlots.find(p => p.parentParcelId === plot.parentParcelId);
        if (analyzedPlot) {
            footprint.primaryEdgeType = analyzedPlot.edgeCondition;
            footprint.cornerType = analyzedPlot.cornerCondition;
            
            footprint.hasWaterFrontage = (footprint.primaryEdgeType === SiteEdgeType.WATER_EDGE);
            footprint.hasParkFrontage = (footprint.primaryEdgeType === SiteEdgeType.PARK_EDGE);
            footprint.hasForestFrontage = (footprint.primaryEdgeType === SiteEdgeType.FOREST_EDGE);
            footprint.hasMajorRoadFrontage = (footprint.primaryEdgeType === SiteEdgeType.STREET_EDGE);
            
            if (footprint.hasForestFrontage && analyzedPlot.forestDirection) {
                footprint.forestDirection = analyzedPlot.forestDirection;
            }
            footprint.forestDistance = analyzedPlot.forestDistance ?? Infinity;
            footprint.forestClosestPoint = analyzedPlot.forestClosestPoint || null;
            footprint.cornerAnchor = analyzedPlot.cornerAnchor || null;
            
            if (footprint.primaryEdgeType === SiteEdgeType.MIXED_EDGE && analyzedPlot.adjacentRoads) {
                for (const road of analyzedPlot.adjacentRoads) {
                    if (road.segment.hierarchy === RoadHierarchy.MAJOR_ARTERIAL || road.segment.hierarchy === RoadHierarchy.MINOR_ARTERIAL) {
                        footprint.hasMajorRoadFrontage = true;
                        break;
                    }
                }
            }
        }
    }

    calculateSetbacks(plot, footprint) {
        switch (plot.originalCategory) {
            case ParcelCategory.LARGE_PARCEL:
                footprint.frontSetback *= 1.2;
                footprint.rearSetback *= 1.2;
                footprint.sideSetback *= 1.2;
                break;
            case ParcelCategory.SMALL_PARCEL:
                footprint.frontSetback *= 0.8;
                footprint.rearSetback *= 0.8;
                footprint.sideSetback *= 0.8;
                break;
        }

        if (footprint.hasMajorRoadFrontage) {
            footprint.frontSetback *= 0.8;
        }

        if (footprint.cornerType !== SiteCornerType.NO_CORNER) {
            if (footprint.cornerType === SiteCornerType.PLAZA_CORNER) footprint.frontSetback *= 1.5;
            else if (footprint.cornerType === SiteCornerType.STREET_CORNER) footprint.frontSetback *= 1.2;
            else footprint.frontSetback *= 1.1;
        }

        const variation = 0.9 + Math.random() * 0.2;
        footprint.frontSetback *= variation;
        footprint.rearSetback *= variation;
        footprint.sideSetback *= variation;
    }

    generateBuildingBoundary(plot, footprint) {
        // Since geometry checks are complex, apply uniform polygon offset scaling using setbacks
        const maxSetback = Math.max(footprint.frontSetback, footprint.rearSetback, footprint.sideSetback);
        footprint.boundary = this.generateUniformOffsetPolygon(plot.boundary, maxSetback);
        
        this.adjustCoverageIfNeeded(plot, footprint);
    }
    
    generateUniformOffsetPolygon(original, offset) {
        if (original.length < 3) return [];
        let center = centroid(original);
        let result = [];
        
        for (const pt of original) {
            let vec = sub(pt, center);
            let d = Math.sqrt(vec.x*vec.x + vec.y*vec.y);
            if (d < offset) continue;
            
            let scaleFactor = (d - offset) / d;
            result.push(add(center, mul(vec, scaleFactor)));
        }
        
        if (result.length < 3) {
            // Very small plot, fallback small box
            let s = 1.0;
            return [
                pt(center.x - s, center.y - s),
                pt(center.x + s, center.y - s),
                pt(center.x + s, center.y + s),
                pt(center.x - s, center.y + s)
            ];
        }
        return result;
    }

    adjustCoverageIfNeeded(plot, footprint) {
        footprint.area = polygonArea(footprint.boundary);
        let currentCoverage = footprint.area / plot.area;

        if (currentCoverage > this.maxCoverageRatio) {
            let scaleFactor = Math.sqrt(this.maxCoverageRatio / currentCoverage);
            this.scaleFootprint(footprint, scaleFactor);
        } else if (currentCoverage < this.minCoverageRatio && currentCoverage > 0) {
            let scaleFactor = Math.sqrt(this.minCoverageRatio / currentCoverage);
            this.scaleFootprint(footprint, scaleFactor);
        }
    }

    scaleFootprint(footprint, scaleFactor) {
        if (footprint.boundary.length < 3) return;
        let center = centroid(footprint.boundary);
        for (let i = 0; i < footprint.boundary.length; i++) {
            let dx = footprint.boundary[i].x - center.x;
            let dy = footprint.boundary[i].y - center.y;
            footprint.boundary[i].x = center.x + dx * scaleFactor;
            footprint.boundary[i].y = center.y + dy * scaleFactor;
        }
    }

    findPrimaryEdgeIndex(boundary) {
        if (boundary.length < 3) return 0;
        let maxLen = 0;
        let primaryIdx = 0;
        for (let i = 0; i < boundary.length; i++) {
            let j = (i + 1) % boundary.length;
            let d = distance(boundary[i], boundary[j]);
            if (d > maxLen) {
                maxLen = d;
                primaryIdx = i;
            }
        }
        return primaryIdx;
    }

    determinePrimaryOrientation(plot, footprint) {
        if (footprint.boundary.length >= 3) {
            let pIdx = this.findPrimaryEdgeIndex(footprint.boundary);
            let next = (pIdx + 1) % footprint.boundary.length;
            let d = normalize(sub(footprint.boundary[next], footprint.boundary[pIdx]));
            footprint.primaryOrientation = d;
            return;
        }
        footprint.primaryOrientation = pt(1, 0);
    }

    generateEntrances(footprint) {
        if (footprint.boundary.length < 3) return;
        
        let pIdx = this.findPrimaryEdgeIndex(footprint.boundary);
        let next = (pIdx + 1) % footprint.boundary.length;
        
        let entrance = pt(
            (footprint.boundary[pIdx].x + footprint.boundary[next].x) * 0.5,
            (footprint.boundary[pIdx].y + footprint.boundary[next].y) * 0.5
        );
        footprint.entrances.push(entrance);
    }
}
