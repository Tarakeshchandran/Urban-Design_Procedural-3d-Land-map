import { pt, distance, centroid, polygonArea, splitPolygonByLine, sub, add, mul, normalize, dot } from './geometry.js';
import { ParcelCategory } from './SiteGeneration.js';

export const SubdivisionLevel = {
    NO_SUBDIVISION: 0,
    SINGLE_DIVISION: 1,
    DOUBLE_DIVISION: 2,
    QUADRUPLE_DIVISION: 4
};

export const OpenSpaceType = {
    NONE: 0,
    CENTRAL_PARK: 1,
    PLAZA: 2,
    COURTYARD: 3,
    LINEAR_PARK: 4,
    POCKET_PARK: 5,
    GREEN_BUFFER: 6,
    GREEN_ROOF: 7
};

export class ParcelSubdivider {
    constructor(siteGen, parcelAnalyzer, subdivScale = 1.0, greenRatio = 1.0, subdivTargets = null) {
        this.siteGen = siteGen;
        this.parcelAnalyzer = parcelAnalyzer;
        this.allPlots = [];
        this.allOpenSpaces = [];
        this.categoryParameters = {};
        this.nextPlotId = 0;
        this.nextOpenSpaceId = 0;
        this.maxPlotAspectRatio = 2.5;
        
        this.initializeSubdivisionParameters(subdivScale, greenRatio, subdivTargets);
    }
    
    initializeSubdivisionParameters(subdivScale, greenRatio, subdivTargets) {
        this.categoryParameters[ParcelCategory.LARGE_PARCEL] = {
            maxDivisions: 6,
            minPlotArea: (subdivTargets && subdivTargets.large) ? subdivTargets.large * subdivScale : 25.0 * subdivScale,
            openSpaceRatio: 0.25 * greenRatio,
            preferredOpenSpaceType: OpenSpaceType.CENTRAL_PARK
        };
        
        this.categoryParameters[ParcelCategory.MEDIUM_PARCEL] = {
            maxDivisions: 6,
            minPlotArea: (subdivTargets && subdivTargets.medium) ? subdivTargets.medium * subdivScale : 40.0 * subdivScale,
            openSpaceRatio: 0.15 * greenRatio,
            preferredOpenSpaceType: OpenSpaceType.COURTYARD
        };
        
        this.categoryParameters[ParcelCategory.SMALL_PARCEL] = {
            maxDivisions: 6,
            minPlotArea: (subdivTargets && subdivTargets.small) ? subdivTargets.small * subdivScale : 25.0 * subdivScale,
            openSpaceRatio: 0.05 * greenRatio,
            preferredOpenSpaceType: OpenSpaceType.POCKET_PARK
        };
    }
    
    subdivideAllParcels() {
        console.log("=== SUBDIVIDING ALL PARCELS ===");
        if (!this.siteGen || !this.parcelAnalyzer) return;
        
        const parcels = this.siteGen.getParcels();
        this.allPlots = [];
        this.allOpenSpaces = [];
        this.nextPlotId = 0;
        this.nextOpenSpaceId = 0;
        
        for (const parcel of parcels) {
            const resultPlots = this.subdivideParcel(parcel);
            
            for (let plot of resultPlots) {
                plot.id = this.nextPlotId++;
                if (plot.isOpenSpace) {
                    // This entire plot was designated as a park during subdivision
                    let os = {
                        id: this.nextOpenSpaceId++,
                        parentPlotId: plot.id,
                        type: plot.openSpaceType,
                        boundary: [...plot.boundary],
                        center: plot.center,
                        area: plot.area
                    };
                    this.allOpenSpaces.push(os);
                } else if (this.shouldGenerateOpenSpace(plot)) {
                    this.generateOpenSpaceInPlot(plot);
                }
                this.calculateBuildableArea(plot);
                this.allPlots.push(plot);
            }
        }
        
        console.log(`Subdivision complete. Generated ${this.allPlots.length} plots from ${parcels.length} parcels.`);
    }
    
    subdivideParcel(parcel) {
        // Fallback to searching analyzer plots to find original classification
        const analyzedPlotInfo = this.parcelAnalyzer.analyzedPlots.find(p => p.parentParcelId === parcel.id);
        const category = analyzedPlotInfo ? analyzedPlotInfo.category : ParcelCategory.SMALL_PARCEL;
        const params = this.categoryParameters[category];
        
        let initialPlot = this.createPlotFromParcel(parcel, 0, 0, category);
        let finalPlots = [];
        
        this.recursiveSubdivide(initialPlot, params, 0, category, finalPlots);
        finalPlots = this.enforceMaxAspectRatio(finalPlots, params, category);
        
        // Ensure they have right data
        for (let i = 0; i < finalPlots.length; i++) {
            finalPlots[i].plotIndex = i;
        }
        return finalPlots;
    }

    calculatePlotAspectRatio(boundary) {
        if (!boundary || boundary.length < 3) return 1.0;

        let longestEdgeLen = 0.0;
        let majorAxis = pt(1, 0);
        for (let i = 0; i < boundary.length; i++) {
            const j = (i + 1) % boundary.length;
            const edge = sub(boundary[j], boundary[i]);
            const len = distance(boundary[i], boundary[j]);
            if (len > longestEdgeLen) {
                longestEdgeLen = len;
                majorAxis = normalize(edge);
            }
        }

        const minorAxis = pt(-majorAxis.y, majorAxis.x);
        let minMajor = Infinity, maxMajor = -Infinity;
        let minMinor = Infinity, maxMinor = -Infinity;

        for (const p of boundary) {
            const major = dot(p, majorAxis);
            const minor = dot(p, minorAxis);
            if (major < minMajor) minMajor = major;
            if (major > maxMajor) maxMajor = major;
            if (minor < minMinor) minMinor = minor;
            if (minor > maxMinor) maxMinor = minor;
        }

        const majorSpan = Math.max(0.001, maxMajor - minMajor);
        const minorSpan = Math.max(0.001, maxMinor - minMinor);
        return Math.max(majorSpan / minorSpan, minorSpan / majorSpan);
    }

    getLongAxis(boundary) {
        if (!boundary || boundary.length < 3) return pt(1, 0);
        let longestEdgeLen = 0.0;
        let majorAxis = pt(1, 0);
        for (let i = 0; i < boundary.length; i++) {
            const j = (i + 1) % boundary.length;
            const edge = sub(boundary[j], boundary[i]);
            const len = distance(boundary[i], boundary[j]);
            if (len > longestEdgeLen) {
                longestEdgeLen = len;
                majorAxis = normalize(edge);
            }
        }
        return majorAxis;
    }

    splitElongatedPlot(plot, category) {
        const longAxis = this.getLongAxis(plot.boundary);
        // Split along the short direction so elongated plots become shorter.
        const splitAxis = pt(-longAxis.y, longAxis.x);
        const extend = 1000.0;
        const splitStart = sub(plot.center, mul(splitAxis, extend));
        const splitEnd = add(plot.center, mul(splitAxis, extend));
        const splitPolys = splitPolygonByLine(plot.boundary, splitStart, splitEnd);
        if (splitPolys.length !== 2) return null;

        const p1 = this.createPlotFromBoundary(plot, splitPolys[0], (plot.subdivisionLevel || 0) + 1, category);
        const p2 = this.createPlotFromBoundary(plot, splitPolys[1], (plot.subdivisionLevel || 0) + 1, category);
        p1.subdivisionAxis = splitAxis;
        p2.subdivisionAxis = splitAxis;
        return [p1, p2];
    }

    enforceMaxAspectRatio(plots, params, category) {
        const queue = [...plots];
        const result = [];
        const maxIterations = 5000;
        let iterations = 0;

        while (queue.length > 0 && iterations < maxIterations) {
            iterations++;
            const plot = queue.shift();
            if (!plot || plot.isOpenSpace) {
                if (plot) result.push(plot);
                continue;
            }

            const ratio = this.calculatePlotAspectRatio(plot.boundary);
            const canSplitByArea = plot.area >= params.minPlotArea * 1.5;
            const canSplitByDepth = (plot.subdivisionLevel || 0) < (params.maxDivisions + 3);

            if (ratio > this.maxPlotAspectRatio && canSplitByArea && canSplitByDepth) {
                const split = this.splitElongatedPlot(plot, category);
                if (split) {
                    // Avoid creating unusably small slivers.
                    const minChildArea = Math.min(split[0].area, split[1].area);
                    if (minChildArea >= params.minPlotArea * 0.6) {
                        queue.push(split[0], split[1]);
                        continue;
                    }
                }
            }

            result.push(plot);
        }

        // Failsafe in case we hit the iteration cap.
        while (queue.length > 0) result.push(queue.shift());
        return result;
    }
    
    recursiveSubdivide(plotInfo, params, currentDivisions, category, finalPlots) {
        if (currentDivisions >= params.maxDivisions || plotInfo.area < params.minPlotArea * 2) {
            finalPlots.push(plotInfo);
            return;
        }
        
        // Need to check if splitting would violate road frontage
        const axis = this.calculateOptimalSubdivisionAxis({ center: plotInfo.center, boundary: plotInfo.boundary });
        const extend = 1000.0;
        const splitStart = sub(plotInfo.center, mul(axis, extend));
        const splitEnd = add(plotInfo.center, mul(axis, extend));
        
        const splitPolys = splitPolygonByLine(plotInfo.boundary, splitStart, splitEnd);
        if (splitPolys.length !== 2) {
            finalPlots.push(plotInfo);
            return;
        }
        
        let p1 = this.createPlotFromBoundary(plotInfo, splitPolys[0], currentDivisions + 1, category);
        let p2 = this.createPlotFromBoundary(plotInfo, splitPolys[1], currentDivisions + 1, category);
        
        // Deep division road frontage check
        if (currentDivisions >= 2) {
            if (!this.hasRoadFrontage(p1) || !this.hasRoadFrontage(p2)) {
                finalPlots.push(plotInfo);
                return;
            }
        }
        
        p1.subdivisionAxis = axis;
        p2.subdivisionAxis = axis;
        
        // Randomly designate open space
        if (Math.random() < params.openSpaceRatio * 0.5) {
            this.designateAsOpenSpace(Math.random() < 0.5 ? p1 : p2, params.preferredOpenSpaceType);
        }
        
        // Recurse
        this.recursiveSubdivide(p1, params, currentDivisions + 1, category, finalPlots);
        this.recursiveSubdivide(p2, params, currentDivisions + 1, category, finalPlots);
    }
    
    hasRoadFrontage(plot) {
        for (const seg of this.parcelAnalyzer.roadSegments) {
            if (seg.getDistanceToPoint(plot.center) < 30.0) { // Rough check
                return true;
            }
        }
        return false;
    }
    
    performSingleDivision(parcel, axis, category) {
        const center = parcel.center;
        const extend = 100.0;
        const splitStart = sub(center, mul(axis, extend));
        const splitEnd = add(center, mul(axis, extend));
        
        const splitPolys = splitPolygonByLine(parcel.boundary, splitStart, splitEnd);
        if (splitPolys.length !== 2) {
            return [this.createPlotFromParcel(parcel, 0, 0, category)];
        }
        
        let plots = [];
        for (let i = 0; i < splitPolys.length; i++) {
            let p = this.createPlotFromBoundary(parcel, splitPolys[i], 1, category);
            p.subdivisionAxis = axis;
            if (i === 0 && parcel.area >= 40.0 && Math.random() < 0.05) {
                this.designateAsOpenSpace(p, OpenSpaceType.POCKET_PARK);
            }
            plots.push(p);
        }
        return plots;
    }
    
    performDoubleDivisionWithOpenSpace(parcel, axis, params, category) {
        const firstSplit = this.performSingleDivision(parcel, axis, category);
        if (firstSplit.length === 1 && firstSplit[0].subdivisionLevel === 0) return firstSplit;
        
        let createCourtyard = false;
        let courtyardIndex = 0;
        if (category === ParcelCategory.MEDIUM_PARCEL || category === ParcelCategory.LARGE_PARCEL) {
            if (Math.random() < params.openSpaceRatio * 1.5) {
                createCourtyard = true;
                courtyardIndex = Math.random() < 0.5 ? 0 : 1;
            }
        }
        
        const perpAxis = pt(-axis.y, axis.x);
        let finalPlots = [];
        for (let i = 0; i < firstSplit.length; i++) {
            const plotInfo = firstSplit[i];
            
            const pStart = sub(plotInfo.center, mul(perpAxis, 100.0));
            const pEnd = add(plotInfo.center, mul(perpAxis, 100.0));
            const subPolys = splitPolygonByLine(plotInfo.boundary, pStart, pEnd);
            
            if (subPolys.length === 2) {
                for (let j = 0; j < subPolys.length; j++) {
                    let fp = this.createPlotFromBoundary(parcel, subPolys[j], 2, category);
                    fp.subdivisionAxis = axis;
                    if (createCourtyard && i === courtyardIndex && j === 0) {
                        this.designateAsOpenSpace(fp, OpenSpaceType.COURTYARD);
                    }
                    finalPlots.push(fp);
                }
            } else {
                plotInfo.subdivisionLevel = 2;
                finalPlots.push(plotInfo);
            }
        }
        return finalPlots.length > 0 ? finalPlots : firstSplit;
    }
    
    performQuadrupleDivisionWithOpenSpace(parcel, axis, params, category) {
        let createCentralPark = (category === ParcelCategory.LARGE_PARCEL && parcel.area >= 100.0 && Math.random() < 0.7);
        
        const doubleSplit = this.performDoubleDivisionWithOpenSpace(parcel, axis, params, category);
        
        let finalPlots = [];
        let cx = 0, cy = 0, cCount = 0;
        
        const maxArea = (parcel.area / 4.0) * 1.5;
        for (let i = 0; i < doubleSplit.length; i++) {
            const p = doubleSplit[i];
            if (p.area > maxArea && !p.isOpenSpace) {
                const localAxis = this.chooseLocalSubdivisionAxis(p);
                const sStart = sub(p.center, mul(localAxis, 100.0));
                const sEnd = add(p.center, mul(localAxis, 100.0));
                const subPolys = splitPolygonByLine(p.boundary, sStart, sEnd);
                if (subPolys.length === 2) {
                    for(let j=0; j<2; j++) {
                        let fp = this.createPlotFromBoundary(parcel, subPolys[j], 4, category);
                        fp.subdivisionAxis = axis;
                        cx += fp.center.x; cy += fp.center.y; cCount++;
                        finalPlots.push(fp);
                    }
                } else {
                    p.subdivisionLevel = 4;
                    cx += p.center.x; cy += p.center.y; cCount++;
                    finalPlots.push(p);
                }
            } else {
                p.subdivisionLevel = 4;
                cx += p.center.x; cy += p.center.y; cCount++;
                finalPlots.push(p);
            }
        }
        
        if (createCentralPark && cCount > 0) {
            const avgCenter = pt(cx/cCount, cy/cCount);
            let minDist = Infinity;
            let closestIdx = -1;
            for (let i = 0; i < finalPlots.length; i++) {
                let d = distance(avgCenter, finalPlots[i].center);
                if (d < minDist && !finalPlots[i].isOpenSpace) {
                    minDist = d;
                    closestIdx = i;
                }
            }
            if (closestIdx !== -1) {
                this.designateAsOpenSpace(finalPlots[closestIdx], OpenSpaceType.CENTRAL_PARK);
            }
        }
        return finalPlots;
    }
    
    // Geometry utils
    calculateOptimalSubdivisionAxis(parcel) {
        // Extrapolate from road analyzer if it existed.
        const adjRoads = this.parcelAnalyzer.getAdjacentRoads(parcel);
        if (adjRoads && adjRoads.length > 0) {
            return adjRoads[0].segment.direction;
        }
        // Fallback to calculating the plot's largest axis dynamically
        if (parcel.boundary.length > 2) {
            let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
            for (let ptIter of parcel.boundary) {
                if (ptIter.x < minX) minX = ptIter.x;
                if (ptIter.x > maxX) maxX = ptIter.x;
                if (ptIter.y < minY) minY = ptIter.y;
                if (ptIter.y > maxY) maxY = ptIter.y;
            }
            return (maxX - minX > maxY - minY) ? pt(1, 0) : pt(0, 1);
        }
        return pt(1, 0);
    }
    
    chooseLocalSubdivisionAxis(plot) {
        let major = pt(1, 0);
        if (plot.boundary.length > 2) {
            let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
            for (let ptIter of plot.boundary) {
                if (ptIter.x < minX) minX = ptIter.x;
                if (ptIter.x > maxX) maxX = ptIter.x;
                if (ptIter.y < minY) minY = ptIter.y;
                if (ptIter.y > maxY) maxY = ptIter.y;
            }
            major = (maxX - minX > maxY - minY) ? pt(1, 0) : pt(0, 1);
        }
        let rndAng = (Math.random() * 30 - 15) * Math.PI / 180;
        return pt(major.x * Math.cos(rndAng) - major.y * Math.sin(rndAng),
                  major.x * Math.sin(rndAng) + major.y * Math.cos(rndAng));
    }
    
    // Plots definition
    createPlotFromParcel(parcel, level, index, category) {
        return {
            parentParcelId: parcel.id,
            boundary: parcel.boundary,
            center: parcel.center,
            area: parcel.area,
            originalCategory: category,
            subdivisionLevel: level,
            plotIndex: index,
            isOpenSpace: false,
            openSpaceType: OpenSpaceType.NONE,
            openSpaceRatio: 0.0,
            requiredSetback: 0.5,
            buildableArea: parcel.area,
            hasMajorRoadFrontage: false
        };
    }
    
    createPlotFromBoundary(parcel, bnd, level, category) {
        let p = {
            parentParcelId: parcel.id,
            boundary: bnd,
            center: centroid(bnd),
            area: polygonArea(bnd),
            originalCategory: category,
            subdivisionLevel: level,
            plotIndex: 0,
            isOpenSpace: false,
            openSpaceType: OpenSpaceType.NONE,
            openSpaceRatio: 0.0,
            requiredSetback: 0.5,
            buildableArea: 0,
            hasMajorRoadFrontage: false
        };
        return p;
    }
    
    // Open spaces configuration
    shouldGenerateOpenSpace(plot) {
        const params = this.categoryParameters[plot.originalCategory];
        return Math.random() < params.openSpaceRatio;
    }
    
    designateAsOpenSpace(plot, type) {
        plot.isOpenSpace = true;
        plot.openSpaceType = type;
        plot.openSpaceRatio = 1.0;
        plot.buildableArea = 0;
    }
    
    generateOpenSpaceInPlot(plot) {
        const params = this.categoryParameters[plot.originalCategory];
        let osType = params.preferredOpenSpaceType;
        
        let os = {
            id: this.nextOpenSpaceId++,
            parentPlotId: plot.id,
            type: osType,
            boundary: []
        };
        
        const openSpacePercent = params.openSpaceRatio;
        const scaleFactor = Math.sqrt(openSpacePercent);
        for (const ptIter of plot.boundary) {
            os.boundary.push(add(plot.center, mul(sub(ptIter, plot.center), scaleFactor)));
        }
        
        os.center = centroid(os.boundary);
        os.area = polygonArea(os.boundary);
        plot.openSpaceRatio = openSpacePercent;
        
        this.allOpenSpaces.push(os);
    }
    
    calculateBuildableArea(plot) {
        if (plot.isOpenSpace) {
            plot.buildableArea = 0;
            return;
        }
        
        plot.requiredSetback = 0.5;
        if (plot.originalCategory === ParcelCategory.LARGE_PARCEL) plot.requiredSetback = 1.0;
        else if (plot.originalCategory === ParcelCategory.MEDIUM_PARCEL) plot.requiredSetback = 0.7;
        
        if (plot.area < 50.0) plot.requiredSetback *= 0.5;
        
        // Rough buildable formulation based on setbacks.
        let sideLen = Math.sqrt(plot.area);
        let reduced = Math.max(0.0, sideLen - (plot.requiredSetback * 2));
        plot.buildableArea = reduced * reduced;
        plot.buildableArea *= (1.0 - plot.openSpaceRatio);
        plot.buildableArea = Math.max(0, Math.min(plot.buildableArea, plot.area * 0.9));
    }
}
