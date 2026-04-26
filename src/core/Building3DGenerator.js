import { centroid, pt, add, sub, mul, normalize, dot, splitPolygonByLine, polygonArea, pointInPolygon } from './geometry.js';
import { ParcelCategory } from './SiteGeneration.js';
import { SiteEdgeType, SiteCornerType } from './ParcelAnalyzer.js';

export const VoxelType = Object.freeze({
    VISIT: 'VISIT',
    WORK: 'WORK',
    LIVE: 'LIVE',
    TRANSITION: 'TRANSITION',
    NONE: 'NONE'
});

export const BuildingStrategy = Object.freeze({
    PODIUM_TOWER: 'PODIUM_TOWER',
    UNIFORM_MIXED: 'UNIFORM_MIXED',
    HORIZONTAL_ZONES: 'HORIZONTAL_ZONES',
    VERTICAL_ZONES: 'VERTICAL_ZONES',
    COURTYARD_BLOCK: 'COURTYARD_BLOCK',
    LARGE_SPLIT_COURTYARD: 'LARGE_SPLIT_COURTYARD',
    STEPPED_MASSING: 'STEPPED_MASSING',
    FOREST_C_MIXED: 'FOREST_C_MIXED',
    FOREST_C_VILLA: 'FOREST_C_VILLA'
});

export class Building3DGenerator {
    constructor(footprintGen, parcelSubdivider, parcelAnalyzer, roadAnalyzer, heightScale = 1.0, voxelScale = 1.0) {
        this.footprintGenerator = footprintGen;
        this.parcelSubdivider = parcelSubdivider;
        this.parcelAnalyzer = parcelAnalyzer;
        this.roadAnalyzer = roadAnalyzer;
        this.building3DVoxels = [];
        this.nextVoxelId = 0;
        this.heightScale = heightScale;
        this.voxelScale = voxelScale;
        
        this.attractorPoints = [];
        this.attractorRange = 220.0;
        this.maxSiteDistance = 400.0; // Approximation for distance normalization
        this.forestPriorityDistance = 5.0;
        this.forestTerraceStep = 0.22;
        this.worldUnitMeters = 10.0;
        this.attractorPeakHeightMeters = 260.0;
        this.attractorLowHeightMeters = 80.0;
        // Bezier-style S-curve controls for attractor distance height falloff.
        // 0..1 where lower c1 + higher c2 gives a gentler peak/valley transition.
        this.heightFalloffBezierC1 = 0.08;
        this.heightFalloffBezierC2 = 0.92;

        this.programRequirements = {
            [VoxelType.VISIT]: { minFloorHeight: 0.4, maxFloorHeight: 0.6, sizeMultiplier: 1.0 },
            [VoxelType.WORK]: { minFloorHeight: 0.35, maxFloorHeight: 0.45, sizeMultiplier: 0.9 },
            [VoxelType.LIVE]: { minFloorHeight: 0.3, maxFloorHeight: 0.4, sizeMultiplier: 0.8 },
            [VoxelType.TRANSITION]: { minFloorHeight: 0.25, maxFloorHeight: 0.8, sizeMultiplier: 1.1 }
        };

        this.densityLimits = {
            [ParcelCategory.LARGE_PARCEL]: { maxBuildingHeight: 60.0, maxFloors: 120, multiplier: 1.6 },
            [ParcelCategory.MEDIUM_PARCEL]: { maxBuildingHeight: 25.0, maxFloors: 60, multiplier: 1.2 },
            [ParcelCategory.SMALL_PARCEL]: { maxBuildingHeight: 10.0, maxFloors: 25, multiplier: 1.0 }
        };

        this.buildingScaleFactor = 0.6;
        this.buildingSetback = 0.3;
        this.defaultFloorHeight = 0.4;
    }

    generateAll3DBuildings() {
        console.log("=== GENERATING 3D BUILDINGS ===");
        if (!this.footprintGenerator) return;

        this.building3DVoxels = [];
        this.nextVoxelId = 0;

        const footprints = this.footprintGenerator.buildingFootprints;
        for (const footprint of footprints) {
            let voxels = this.generate3DBuildingFromFootprint(footprint);
            for (let v of voxels) {
                // Apply typology-based and global voxel size scaling
                let scaledDims = this.applyVoxelSizeAdjustment(v.dimensions, v.type);
                
                // Adjust position to keep voxel centered after scaling
                v.position.x += (v.dimensions.x - scaledDims.x) / 2;
                v.position.y += (v.dimensions.y - scaledDims.y) / 2;
                v.dimensions = scaledDims;

                v.id = this.nextVoxelId++;
                this.building3DVoxels.push(v);
            }
        }
        
        console.log(`Generated ${this.building3DVoxels.length} 3D Voxels`);
    }

    calculateTotalGFA(unitScale = 0.1) {
        let totalInternalArea = 0;
        
        for (const voxel of this.building3DVoxels) {
            if (voxel.shapePolygon && voxel.shapePolygon.length >= 3) {
                totalInternalArea += polygonArea(voxel.shapePolygon);
            } else {
                totalInternalArea += voxel.dimensions.x * voxel.dimensions.y;
            }
        }
        
        // 1 internal unit = 1/unitScale meters
        // 1 internal unit area = (1/unitScale)^2 square meters
        const areaFactor = Math.pow(1 / unitScale, 2);
        return totalInternalArea * areaFactor;
    }

    getPlotFromFootprint(footprint) {
        return this.parcelSubdivider.allPlots.find(p => p.id === footprint.parentPlotId);
    }

    generate3DBuildingFromFootprint(footprint) {
        const plot = this.getPlotFromFootprint(footprint);
        if (!plot) return [];

        const strategy = this.determineBuildingStrategy(plot, footprint);
        footprint.generatedStrategy = strategy;
        
        console.log(`Building for footprint ${footprint.id} using strategy: ${strategy}`);
        
        let voxels = [];
        switch (strategy) {
            case BuildingStrategy.FOREST_C_MIXED:
                voxels = this.generateForestCMixedBuilding(footprint, plot);
                break;
            case BuildingStrategy.FOREST_C_VILLA:
                voxels = this.generateForestCVillaBuilding(footprint, plot);
                break;
            case BuildingStrategy.PODIUM_TOWER:
                voxels = this.stackAbovePodium(footprint, plot);
                break;
            case BuildingStrategy.UNIFORM_MIXED:
                voxels = this.generateUniformMixedBuilding(footprint, plot);
                break;
            case BuildingStrategy.VERTICAL_ZONES:
                voxels = this.generateVerticalZonesBuilding(footprint, plot);
                break;
            case BuildingStrategy.COURTYARD_BLOCK:
                voxels = this.generateCourtyardBuilding(footprint, plot);
                break;
            case BuildingStrategy.LARGE_SPLIT_COURTYARD:
                voxels = this.generateLargeSplitCourtyardBuilding(footprint, plot);
                break;
            case BuildingStrategy.STEPPED_MASSING:
                voxels = this.generateSteppedBuilding(footprint, plot);
                break;
            default:
                voxels = this.stackAbovePodium(footprint, plot);
        }
        
        return voxels;
    }

    determineBuildingStrategy(plot, footprint) {
        if (this.isForestAbutting(footprint)) {
            if (this.isWithinAttractorRange(footprint)) return BuildingStrategy.FOREST_C_MIXED;
            return BuildingStrategy.FOREST_C_VILLA;
        }

        if (footprint.hasWaterFrontage || footprint.hasParkFrontage) return BuildingStrategy.STEPPED_MASSING;
        if (plot.originalCategory === ParcelCategory.SMALL_PARCEL && footprint.hasMajorRoadFrontage) return BuildingStrategy.PODIUM_TOWER;
        if (plot.originalCategory === ParcelCategory.SMALL_PARCEL) return BuildingStrategy.PODIUM_TOWER;
        if (plot.originalCategory === ParcelCategory.LARGE_PARCEL) return BuildingStrategy.LARGE_SPLIT_COURTYARD;
        if (plot.originalCategory === ParcelCategory.MEDIUM_PARCEL) return BuildingStrategy.VERTICAL_ZONES;
        return BuildingStrategy.PODIUM_TOWER;
    }

    isForestAbutting(footprint) {
        if (!footprint?.hasForestFrontage) return false;
        const d = footprint.forestDistance ?? Infinity;
        return d <= this.forestPriorityDistance;
    }

    getEffectiveAttractorRange() {
        if (this.attractorRange && this.attractorRange > 0) return this.attractorRange;
        return Math.max(120.0, this.maxSiteDistance * 0.65);
    }

    getNearestAttractorDistance(footprint) {
        if (!this.attractorPoints || this.attractorPoints.length === 0) return Infinity;
        let minDistance = Infinity;
        for (const attractor of this.attractorPoints) {
            const dx = footprint.center.x - attractor.x;
            const dy = footprint.center.y - attractor.y;
            const d = Math.sqrt(dx * dx + dy * dy);
            if (d < minDistance) minDistance = d;
        }
        return minDistance;
    }

    getAttractorWeight(attractor) {
        if (!attractor) return 1.0;
        if (typeof attractor.weight === 'number' && Number.isFinite(attractor.weight)) {
            return Math.max(0.1, Math.min(1.0, attractor.weight));
        }
        if (attractor.isPrimary) return 1.0;
        return 0.6;
    }

    isWithinAttractorRange(footprint) {
        const nearest = this.getNearestAttractorDistance(footprint);
        return nearest <= this.getEffectiveAttractorRange();
    }

    calculateScaledBuildingDimensions(footprint) {
        if (footprint.boundary.length < 3) return { x: 2, y: 2, z: 0.4 };
        
        let orientation = footprint.primaryOrientation || pt(1, 0);
        if (this.roadAnalyzer && this.roadAnalyzer.networkAnalysis && this.roadAnalyzer.networkAnalysis.primaryDirection) {
            orientation = this.roadAnalyzer.networkAnalysis.primaryDirection;
        }
        
        let perp = pt(-orientation.y, orientation.x);
        
        let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
        for (let pt of footprint.boundary) {
            let projX = pt.x * orientation.x + pt.y * orientation.y;
            let projY = pt.x * perp.x + pt.y * perp.y;
            
            minX = Math.min(minX, projX);
            maxX = Math.max(maxX, projX);
            minY = Math.min(minY, projY);
            maxY = Math.max(maxY, projY);
        }
        
        let fw = maxX - minX;
        let fd = maxY - minY;
        
        let bw = Math.max((fw - 2 * this.buildingSetback) * this.buildingScaleFactor, 1.0);
        let bd = Math.max((fd - 2 * this.buildingSetback) * this.buildingScaleFactor, 1.0);
        
        bw = Math.min(bw, fw * 0.95);
        bd = Math.min(bd, fd * 0.95);
        
        // Base dimensions, to be scaled per typology
        return { x: bw, y: bd, z: this.defaultFloorHeight };
    }

    applyVoxelSizeAdjustment(dimensions, programType) {
        const typeMultiplier = this.programRequirements[programType] ? this.programRequirements[programType].sizeMultiplier : 1.0;
        const totalScale = typeMultiplier * this.voxelScale;
        
        // Return new scaled dimensions, keeping them centered
        return {
            x: dimensions.x * totalScale,
            y: dimensions.y * totalScale,
            z: dimensions.z // Z is scaled via heightScale logic
        };
    }

    generatePodium(plot, footprint, programType) {
        let orientation = footprint.primaryOrientation || pt(1, 0);
        if (this.roadAnalyzer && this.roadAnalyzer.networkAnalysis && this.roadAnalyzer.networkAnalysis.primaryDirection) {
            orientation = this.roadAnalyzer.networkAnalysis.primaryDirection;
        }

        let podium = {
            type: programType,
            parentFootprintId: footprint.id,
            position: { x: 0, y: 0, z: 0 },
            dimensions: { x: 1, y: 1, z: this.programRequirements[programType].maxFloorHeight },
            orientation: orientation,
            rotationAngle: 0,
            level: 0,
            shapePolygon: plot.boundary
        };
        
        return podium;
    }

    calculateHeightGradientMultiplier(plot, footprint) {
        // Deprecated parcel-size multiplier: height is now fully attractor-distance driven.
        const targetHeight = this.getHeightLimit(footprint);
        const baselineHeight = 14.0;
        return Math.max(0.2, targetHeight / baselineHeight);
    }

    metersToWorldUnits(meters) {
        return meters / Math.max(this.worldUnitMeters, 0.001);
    }

    evaluateBezierSCurve01(t) {
        const clamped = Math.max(0, Math.min(1, t));
        const u = 1 - clamped;
        const c1 = Math.max(0, Math.min(1, this.heightFalloffBezierC1));
        const c2 = Math.max(0, Math.min(1, this.heightFalloffBezierC2));
        // Cubic Bezier y(t) with P0=0, P1=c1, P2=c2, P3=1
        return (3 * u * u * clamped * c1) + (3 * u * clamped * clamped * c2) + (clamped * clamped * clamped);
    }

    getHeightLimit(footprint) {
        const peakHeight = this.metersToWorldUnits(this.attractorPeakHeightMeters);
        const lowHeight = this.metersToWorldUnits(this.attractorLowHeightMeters);
        const maxH = Math.max(peakHeight, lowHeight);
        const minH = Math.min(peakHeight, lowHeight);

        if (!this.attractorPoints || this.attractorPoints.length === 0) {
            return Math.max(1.5, maxH * this.heightScale);
        }

        const range = Math.max(1e-4, this.getEffectiveAttractorRange());
        let bestHeight = minH;
        for (const attractor of this.attractorPoints) {
            const dx = footprint.center.x - attractor.x;
            const dy = footprint.center.y - attractor.y;
            const distanceToAttractor = Math.sqrt(dx * dx + dy * dy);
            const t = Math.min(1.0, distanceToAttractor / range);
            const easedT = this.evaluateBezierSCurve01(t);
            const localPeak = Math.max(minH, maxH * this.getAttractorWeight(attractor));

            // Bezier-style S-curve falloff:
            // closest to attractor = peak, farthest = low, with gentler transitions.
            const localHeight = localPeak + (minH - localPeak) * easedT;
            if (localHeight > bestHeight) bestHeight = localHeight;
        }
        return Math.max(1.5, bestHeight * this.heightScale);
    }

    calculateWorkFloors(plot, footprint) {
        const targetHeight = this.getHeightLimit(footprint);
        const workShare = 0.32;
        return Math.max(1, Math.floor((targetHeight * workShare) / this.programRequirements[VoxelType.WORK].minFloorHeight));
    }

    calculateLiveFloors(plot, footprint, remainingHeight) {
        const targetHeight = this.getHeightLimit(footprint);
        const liveShare = 0.5;
        let desiredFloors = Math.floor((targetHeight * liveShare) / this.programRequirements[VoxelType.LIVE].minFloorHeight);
        let maxPossible = Math.floor(remainingHeight / this.programRequirements[VoxelType.LIVE].minFloorHeight);
        return Math.max(1, Math.min(desiredFloors, maxPossible));
    }

    getPodiumTowerAnchor(footprint) {
        const center = footprint.center;
        let target = null;

        if (
            footprint.cornerType === SiteCornerType.PARK_CORNER ||
            footprint.cornerType === SiteCornerType.PLAZA_CORNER ||
            footprint.cornerType === SiteCornerType.STREET_CORNER ||
            footprint.cornerType === SiteCornerType.MIXED_CORNER
        ) {
            target = footprint.cornerAnchor || null;
        }

        if (!target && this.roadAnalyzer?.intersections?.length > 0) {
            let best = null;
            for (const inter of this.roadAnalyzer.intersections) {
                const d = Math.sqrt(
                    (inter.location.x - center.x) * (inter.location.x - center.x) +
                    (inter.location.y - center.y) * (inter.location.y - center.y)
                );
                if (d > 14.0) continue;
                if (!best || inter.importance > best.importance || (inter.importance === best.importance && d < best.distance)) {
                    best = { point: inter.location, importance: inter.importance, distance: d };
                }
            }
            if (best) target = best.point;
        }

        if (!target) return center;

        const shift = sub(target, center);
        // Pull tower toward corner feature without placing centroid exactly on boundary.
        return add(center, mul(shift, 0.65));
    }

    translatePolygon(poly, dx, dy) {
        return poly.map((p) => ({ x: p.x + dx, y: p.y + dy }));
    }

    isPolygonInsideContainer(candidatePoly, containerPoly) {
        if (!candidatePoly || !containerPoly || candidatePoly.length < 3 || containerPoly.length < 3) return false;
        for (const p of candidatePoly) {
            if (!pointInPolygon(p, containerPoly)) return false;
        }
        return true;
    }

    getContainedShiftedTowerPolygon(outerPoly, scale, targetCenter = null) {
        const getShiftedCandidate = (currentScale) => {
            const base = this.scalePolygonAroundCentroid(outerPoly, currentScale);
            if (!targetCenter) return base;

            const c = centroid(base);
            const shiftX = targetCenter.x - c.x;
            const shiftY = targetCenter.y - c.y;
            if (Math.abs(shiftX) < 1e-6 && Math.abs(shiftY) < 1e-6) return base;

            // Binary search max translation factor that still keeps polygon inside footprint.
            let lo = 0.0;
            let hi = 1.0;
            let best = base;
            for (let i = 0; i < 18; i++) {
                const t = (lo + hi) * 0.5;
                const candidate = this.translatePolygon(base, shiftX * t, shiftY * t);
                if (this.isPolygonInsideContainer(candidate, outerPoly)) {
                    best = candidate;
                    lo = t;
                } else {
                    hi = t;
                }
            }
            return best;
        };

        // Primary attempt at requested scale.
        let candidate = getShiftedCandidate(scale);
        if (this.isPolygonInsideContainer(candidate, outerPoly)) return candidate;

        // Secondary fallback for tight parcels: progressively shrink until fully contained.
        let currentScale = scale;
        for (let i = 0; i < 20; i++) {
            currentScale *= 0.92;
            if (currentScale < 0.2) break;
            candidate = getShiftedCandidate(currentScale);
            if (this.isPolygonInsideContainer(candidate, outerPoly)) return candidate;
        }

        // Final conservative fallback.
        return getShiftedCandidate(0.2);
    }

    stackAbovePodium(footprint, plot) {
        let voxels = [];
        let heightLimit = this.getHeightLimit(footprint);
        let currentHeight = 0;
        let currentLevel = 0;

        // 1. Generate Retail Podium (2 floors wide)
        for (let i = 0; i < 2; i++) {
            let pVoxel = this.generatePodium(plot, footprint, VoxelType.VISIT);
            pVoxel.position.z = currentHeight;
            pVoxel.level = currentLevel++;
            voxels.push(pVoxel);
            currentHeight += pVoxel.dimensions.z;
        }

        // 2. Tower footprint as contained polygon (thicker than before), with corner-biased anchor clamped inside parcel.
        const towerCenter = this.getPodiumTowerAnchor(footprint);
        const containerPoly = footprint.boundary && footprint.boundary.length >= 3 ? footprint.boundary : plot.boundary;
        const workPoly = this.getContainedShiftedTowerPolygon(containerPoly, 0.62, towerCenter);
        const transitionPoly = this.getContainedShiftedTowerPolygon(containerPoly, 0.60, towerCenter);
        const livePoly = this.getContainedShiftedTowerPolygon(containerPoly, 0.56, towerCenter);

        // 3. Generate Work Floors
        let workFloors = this.calculateWorkFloors(plot, footprint);
        for (let i = 0; i < workFloors && currentHeight < heightLimit; i++) {
            let v = this.createPolygonVoxel(
                VoxelType.WORK,
                footprint.id,
                workPoly,
                currentHeight,
                currentLevel++,
                voxels[0].orientation || pt(1, 0)
            );
            voxels.push(v);
            currentHeight += v.dimensions.z;
        }

        // 4. Generate Transition Floor
        if (currentHeight < heightLimit - 1.0) {
            let v = this.createPolygonVoxel(
                VoxelType.TRANSITION,
                footprint.id,
                transitionPoly,
                currentHeight,
                currentLevel++,
                voxels[0].orientation || pt(1, 0)
            );
            v.dimensions.z = this.programRequirements[VoxelType.TRANSITION].maxFloorHeight;
            voxels.push(v);
            currentHeight += v.dimensions.z;
        }

        // 5. Generate Live Floors
        let liveFloors = this.calculateLiveFloors(plot, footprint, heightLimit - currentHeight);
        for (let i = 0; i < liveFloors && currentHeight < heightLimit; i++) {
            let v = this.createPolygonVoxel(
                VoxelType.LIVE,
                footprint.id,
                livePoly,
                currentHeight,
                currentLevel++,
                voxels[0].orientation || pt(1, 0)
            );
            voxels.push(v);
            currentHeight += v.dimensions.z;
        }

        return voxels;
    }

    computePolygonBounds(poly) {
        let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
        for (const p of poly) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }
        return {
            minX,
            maxX,
            minY,
            maxY,
            width: Math.max(0.1, maxX - minX),
            depth: Math.max(0.1, maxY - minY)
        };
    }

    createPolygonVoxel(type, footprintId, shapePolygon, z, level, orientation = pt(1, 0)) {
        const b = this.computePolygonBounds(shapePolygon);
        return {
            type,
            parentFootprintId: footprintId,
            position: { x: b.minX, y: b.minY, z },
            dimensions: { x: b.width, y: b.depth, z: this.programRequirements[type].minFloorHeight },
            orientation,
            rotationAngle: Math.atan2(orientation.y, orientation.x),
            level,
            shapePolygon
        };
    }

    scalePolygonAroundCentroid(poly, scale) {
        const c = centroid(poly);
        const clampedScale = Math.max(0.4, Math.min(1.0, scale));
        return poly.map((p) => ({
            x: c.x + (p.x - c.x) * clampedScale,
            y: c.y + (p.y - c.y) * clampedScale
        }));
    }

    projectionRange(poly, axis) {
        let minP = Infinity;
        let maxP = -Infinity;
        for (const p of poly) {
            const proj = dot(p, axis);
            if (proj < minP) minP = proj;
            if (proj > maxP) maxP = proj;
        }
        return { min: minP, max: maxP };
    }

    splitPolygonIntoZoneBands(poly, orientation, numZones) {
        if (!poly || poly.length < 3 || numZones <= 1) return [poly];
        const axis = normalize(orientation || pt(1, 0));
        const perp = pt(-axis.y, axis.x);
        const range = this.projectionRange(poly, perp);
        const span = Math.max(0.001, range.max - range.min);
        const extend = 5000.0;

        const cutValues = [];
        for (let i = 1; i < numZones; i++) {
            cutValues.push(range.min + (span * i) / numZones);
        }

        let pieces = [poly];
        for (const cut of cutValues) {
            let didSplit = false;
            const next = [];
            for (const piece of pieces) {
                if (didSplit) {
                    next.push(piece);
                    continue;
                }
                const pieceRange = this.projectionRange(piece, perp);
                if (cut <= pieceRange.min || cut >= pieceRange.max) {
                    next.push(piece);
                    continue;
                }
                const lineCenter = mul(perp, cut);
                const lineA = sub(lineCenter, mul(axis, extend));
                const lineB = add(lineCenter, mul(axis, extend));
                const split = splitPolygonByLine(piece, lineA, lineB);
                if (split.length === 2) {
                    next.push(split[0], split[1]);
                    didSplit = true;
                } else {
                    next.push(piece);
                }
            }
            pieces = next;
        }

        const filtered = pieces.filter((p) => p && p.length >= 3 && polygonArea(p) > 0.2);
        filtered.sort((a, b) => dot(centroid(a), perp) - dot(centroid(b), perp));
        return filtered.length > 0 ? filtered : [poly];
    }

    createCourtyardRingBands(outerPoly, innerScale = 0.72) {
        if (!outerPoly || outerPoly.length < 3) return [];
        const innerPoly = this.scalePolygonAroundCentroid(outerPoly, innerScale);
        if (!innerPoly || innerPoly.length !== outerPoly.length) return [];

        const bands = [];
        for (let i = 0; i < outerPoly.length; i++) {
            const j = (i + 1) % outerPoly.length;
            const band = [outerPoly[i], outerPoly[j], innerPoly[j], innerPoly[i]];
            if (polygonArea(band) > 0.05) bands.push(band);
        }
        return bands;
    }

    seededRandom(seed) {
        const x = Math.sin(seed * 12.9898 + 78.233) * 43758.5453123;
        return x - Math.floor(x);
    }

    createBridgePolygonBetweenWings(wingA, wingB, bridgeWidth, lengthFactor = 0.55) {
        if (!wingA || !wingB || wingA.length < 3 || wingB.length < 3) return null;
        const cA = centroid(wingA);
        const cB = centroid(wingB);
        const d = sub(cB, cA);
        const len = Math.sqrt(d.x * d.x + d.y * d.y);
        if (len < 0.4) return null;

        const dir = normalize(d);
        const lat = pt(-dir.y, dir.x);
        const halfLength = Math.max(0.25, len * lengthFactor * 0.5);
        const halfWidth = Math.max(0.18, bridgeWidth * 0.5);
        const mid = pt((cA.x + cB.x) * 0.5, (cA.y + cB.y) * 0.5);

        return [
            add(add(mid, mul(dir, halfLength)), mul(lat, halfWidth)),
            add(add(mid, mul(dir, halfLength)), mul(lat, -halfWidth)),
            add(add(mid, mul(dir, -halfLength)), mul(lat, -halfWidth)),
            add(add(mid, mul(dir, -halfLength)), mul(lat, halfWidth))
        ];
    }

    generateUniformMixedBuilding(footprint, plot) {
        let voxels = [];
        let heightLimit = this.getHeightLimit(footprint);
        const basePoly = footprint.boundary && footprint.boundary.length >= 3 ? footprint.boundary : null;
        if (!basePoly) return voxels;

        let currentHeight = 0;
        let currentLevel = 0;

        let orientation = footprint.primaryOrientation || pt(1, 0);

        // Phase 1 parcel-hugging slabs.
        for (let i = 0; i < 2 && currentHeight < heightLimit; i++) {
            const poly = this.scalePolygonAroundCentroid(basePoly, 1.0);
            const v = this.createPolygonVoxel(VoxelType.VISIT, footprint.id, poly, currentHeight, currentLevel++, orientation);
            voxels.push(v);
            currentHeight += v.dimensions.z;
        }

        let workFloors = this.calculateWorkFloors(plot, footprint);
        for (let i = 0; i < workFloors && currentHeight < heightLimit; i++) {
            const poly = this.scalePolygonAroundCentroid(basePoly, 0.9);
            const v = this.createPolygonVoxel(VoxelType.WORK, footprint.id, poly, currentHeight, currentLevel++, orientation);
            voxels.push(v);
            currentHeight += v.dimensions.z;
        }

        let liveFloors = this.calculateLiveFloors(plot, footprint, heightLimit - currentHeight);
        for (let i = 0; i < liveFloors && currentHeight < heightLimit; i++) {
            const poly = this.scalePolygonAroundCentroid(basePoly, 0.78);
            const v = this.createPolygonVoxel(VoxelType.LIVE, footprint.id, poly, currentHeight, currentLevel++, orientation);
            voxels.push(v);
            currentHeight += v.dimensions.z;
        }

        return voxels;
    }

    generateVerticalZonesBuilding(footprint, plot) {
        let voxels = [];
        let heightLimit = this.getHeightLimit(footprint);
        const basePoly = footprint.boundary && footprint.boundary.length >= 3 ? footprint.boundary : null;
        if (!basePoly) return voxels;
        
        let orientation = footprint.primaryOrientation || pt(1, 0);
        
        // Split into 2 or 3 zones
        const polyArea = polygonArea(basePoly);
        let numZones = polyArea > 16 ? 3 : 2;
        const zones = this.splitPolygonIntoZoneBands(basePoly, orientation, numZones);

        for (let zone = 0; zone < zones.length; zone++) {
            const zonePoly = zones[zone];
            let currentHeight = 0;
            let currentLevel = 0;
            
            // Stagger the heights: zone 0 is tallest, zone 1 is shorter, etc.
            let zoneHeightScale = 1.0 - (zone * 0.25);
            let workFloors = Math.max(1, Math.floor(this.calculateWorkFloors(plot, footprint) * zoneHeightScale));
            let liveFloors = Math.max(1, Math.floor(this.calculateLiveFloors(plot, footprint, heightLimit) * zoneHeightScale));

            // 1. Visit (Base, 1 floor) as exact zone polygon.
            {
                const v = this.createPolygonVoxel(
                    VoxelType.VISIT,
                    footprint.id,
                    this.scalePolygonAroundCentroid(zonePoly, 1.0),
                    currentHeight,
                    currentLevel++,
                    orientation
                );
                voxels.push(v);
                currentHeight += v.dimensions.z;
            }

            // 2. Work (Middle)
            for (let i = 0; i < workFloors && currentHeight < heightLimit; i++) {
                const v = this.createPolygonVoxel(
                    VoxelType.WORK,
                    footprint.id,
                    this.scalePolygonAroundCentroid(zonePoly, 0.92),
                    currentHeight,
                    currentLevel++,
                    orientation
                );
                voxels.push(v);
                currentHeight += v.dimensions.z;
            }

            // 3. Live (Top)
            for (let i = 0; i < liveFloors && currentHeight < heightLimit; i++) {
                const v = this.createPolygonVoxel(
                    VoxelType.LIVE,
                    footprint.id,
                    this.scalePolygonAroundCentroid(zonePoly, 0.84),
                    currentHeight,
                    currentLevel++,
                    orientation
                );
                voxels.push(v);
                currentHeight += v.dimensions.z;
            }
        }
        
        return voxels;
    }

    generateCourtyardBuilding(footprint, plot) {
        let voxels = [];
        let heightLimit = this.getHeightLimit(footprint);
        const basePoly = footprint.boundary && footprint.boundary.length >= 3 ? footprint.boundary : null;
        if (!basePoly) return voxels;
        
        let currentHeight = 0;
        let currentLevel = 0;
        let orientation = footprint.primaryOrientation || pt(1, 0);

        const addRing = (type, floor) => {
            const ringBands = this.createCourtyardRingBands(basePoly, 0.72);
            if (ringBands.length === 0) return;
            let zh = this.programRequirements[type].minFloorHeight;
            for (const band of ringBands) {
                const v = this.createPolygonVoxel(type, footprint.id, band, currentHeight, floor, orientation);
                voxels.push(v);
            }
            currentHeight += zh;
        };

        // 1. Visit (1 floor)
        addRing(VoxelType.VISIT, currentLevel++);

        // 2. Work (Middle)
        let workFloors = this.calculateWorkFloors(plot, footprint);
        for (let i = 0; i < workFloors && currentHeight < heightLimit; i++) {
            addRing(VoxelType.WORK, currentLevel++);
        }

        // 3. Live (Top)
        let liveFloors = this.calculateLiveFloors(plot, footprint, heightLimit - currentHeight);
        for (let i = 0; i < liveFloors && currentHeight < heightLimit; i++) {
            addRing(VoxelType.LIVE, currentLevel++);
        }

        return voxels;
    }

    generateLargeSplitCourtyardBuilding(footprint, plot) {
        const voxels = [];
        const basePoly = footprint.boundary && footprint.boundary.length >= 3 ? footprint.boundary : null;
        if (!basePoly) return voxels;

        const orientation = footprint.primaryOrientation || pt(1, 0);
        const wings = this.splitPolygonIntoZoneBands(basePoly, orientation, 2);
        if (wings.length < 2) {
            return this.generateCourtyardBuilding(footprint, plot);
        }

        const wingA = wings[0];
        const wingB = wings[1];
        const baseArea = polygonArea(basePoly);
        const seedBase = (footprint.id + 1) * 97 + Math.round(baseArea * 10);

        // Variant controls (stable per footprint): cut-throughs, bridge cadence, terrace strength.
        const cutA = Math.floor(this.seededRandom(seedBase + 11) * 4);
        const cutB = Math.floor(this.seededRandom(seedBase + 23) * 4);
        const bridgeEvery = this.seededRandom(seedBase + 37) > 0.5 ? 3 : 4;
        const terraceStrength = 0.12 + this.seededRandom(seedBase + 53) * 0.18;
        const baseInnerScale = 0.68 + this.seededRandom(seedBase + 71) * 0.08;

        const heightLimit = this.getHeightLimit(footprint);
        let currentHeight = 0;
        let currentLevel = 0;

        const visitFloors = 2;
        const workFloors = Math.max(2, Math.floor(this.calculateWorkFloors(plot, footprint) * 0.85));
        const transitionFloors = 1;
        const liveFloors = Math.max(3, Math.floor(this.calculateLiveFloors(plot, footprint, heightLimit) * 0.65));
        const totalFloors = visitFloors + workFloors + transitionFloors + liveFloors;

        for (let floor = 0; floor < totalFloors && currentHeight < heightLimit; floor++) {
            let type = VoxelType.LIVE;
            if (floor < visitFloors) type = VoxelType.VISIT;
            else if (floor < visitFloors + workFloors) type = VoxelType.WORK;
            else if (floor < visitFloors + workFloors + transitionFloors) type = VoxelType.TRANSITION;

            const floorHeight = this.programRequirements[type].minFloorHeight;
            const floorRatio = floor / Math.max(totalFloors - 1, 1);

            // Terrace variant: progressively step in upper floors.
            const wingScale = Math.max(0.72, 1.0 - floorRatio * terraceStrength);
            const ringInnerScale = Math.min(0.9, baseInnerScale + floorRatio * 0.08);
            const scaledWingA = this.scalePolygonAroundCentroid(wingA, wingScale);
            const scaledWingB = this.scalePolygonAroundCentroid(wingB, wingScale);
            const ringA = this.createCourtyardRingBands(scaledWingA, ringInnerScale);
            const ringB = this.createCourtyardRingBands(scaledWingB, ringInnerScale);

            // Cut-through variant: remove one ring panel at lower levels.
            const applyCutThrough = floor < 2;
            for (let i = 0; i < ringA.length; i++) {
                if (applyCutThrough && i === cutA % Math.max(1, ringA.length)) continue;
                voxels.push(this.createPolygonVoxel(type, footprint.id, ringA[i], currentHeight, currentLevel, orientation));
            }
            for (let i = 0; i < ringB.length; i++) {
                if (applyCutThrough && i === cutB % Math.max(1, ringB.length)) continue;
                voxels.push(this.createPolygonVoxel(type, footprint.id, ringB[i], currentHeight, currentLevel, orientation));
            }

            // Bridge variant: add occasional connectors between split wings.
            if (floor >= 1 && floor % bridgeEvery === 0) {
                const bridgeWidth = Math.max(0.45, Math.min(1.8, Math.sqrt(baseArea) * 0.06));
                const bridgePoly = this.createBridgePolygonBetweenWings(scaledWingA, scaledWingB, bridgeWidth, 0.5);
                if (bridgePoly && polygonArea(bridgePoly) > 0.08) {
                    voxels.push(
                        this.createPolygonVoxel(
                            VoxelType.TRANSITION,
                            footprint.id,
                            bridgePoly,
                            currentHeight,
                            currentLevel,
                            orientation
                        )
                    );
                }
            }

            currentHeight += floorHeight;
            currentLevel++;
        }

        return voxels;
    }

    generateSteppedBuilding(footprint, plot) {
        let voxels = [];
        let baseDims = this.calculateScaledBuildingDimensions(footprint);
        let heightLimit = this.getHeightLimit(footprint);
        
        let basePos = {
            x: footprint.center.x - baseDims.x / 2,
            y: footprint.center.y - baseDims.y / 2
        };
        
        let currentHeight = 0;
        let currentLevel = 0;

        let workFloors = this.calculateWorkFloors(plot, footprint);
        let liveFloors = this.calculateLiveFloors(plot, footprint, heightLimit);
        let totalFloors = 1 + workFloors + liveFloors;

        for (let floor = 0; floor < totalFloors && currentHeight < heightLimit; floor++) {
            // Reduction step: every 3 floors, reduce size by 10%
            let reductionStep = Math.floor(floor / 3);
            let reduction = Math.max(0.3, 1.0 - (reductionStep * 0.15));
            
            let fw = baseDims.x * reduction;
            let fd = baseDims.y * reduction;
            
            let type = (floor === 0) ? VoxelType.VISIT : (floor <= workFloors ? VoxelType.WORK : VoxelType.LIVE);
            let zh = this.programRequirements[type].minFloorHeight;

            voxels.push({
                type, parentFootprintId: footprint.id,
                dimensions: { x: fw, y: fd, z: zh },
                position: { 
                    x: basePos.x + (baseDims.x - fw) / 2, 
                    y: basePos.y + (baseDims.y - fd) / 2, 
                    z: currentHeight 
                },
                orientation: footprint.primaryOrientation || pt(1, 0),
                rotationAngle: Math.atan2(footprint.primaryOrientation?.y || 0, footprint.primaryOrientation?.x || 1),
                level: currentLevel++
            });
            currentHeight += zh;
        }
        return voxels;
    }

    getForestFrame(footprint) {
        const forestDir = footprint?.forestDirection || pt(1, 0);
        const towardForest = pt(forestDir.x, forestDir.y);
        const awayFromForest = pt(-towardForest.x, -towardForest.y);
        const tangent = pt(-towardForest.y, towardForest.x);
        const rotationAngle = Math.atan2(tangent.y, tangent.x);
        return { towardForest, awayFromForest, tangent, rotationAngle };
    }

    createOrientedVoxel(type, footprintId, center, size, z, level, frame) {
        return {
            type,
            parentFootprintId: footprintId,
            dimensions: { x: size.x, y: size.y, z: size.z },
            position: { x: center.x - size.x / 2, y: center.y - size.y / 2, z },
            orientation: frame.tangent,
            rotationAngle: frame.rotationAngle,
            level
        };
    }

    addCShapeFloor(voxels, footprint, frame, baseDims, params) {
        const totalWidth = Math.max(baseDims.x * params.sizeScale, 2.4);
        const totalDepth = Math.max(baseDims.y * params.sizeScale, 2.0);
        const courtyardWidth = Math.max(totalWidth * 0.5, 0.9);
        const wingWidth = Math.max((totalWidth - courtyardWidth) * 0.5, 0.55);
        const backDepth = Math.max(totalDepth * 0.42, 0.55);
        const wingDepth = totalDepth;
        const terraceShift = params.terraceOffset;

        const shiftX = frame.awayFromForest.x * terraceShift;
        const shiftY = frame.awayFromForest.y * terraceShift;
        const center = footprint.center;

        const leftWingCenter = {
            x: center.x + frame.tangent.x * (-(totalWidth * 0.5 - wingWidth * 0.5)) + shiftX,
            y: center.y + frame.tangent.y * (-(totalWidth * 0.5 - wingWidth * 0.5)) + shiftY
        };
        const rightWingCenter = {
            x: center.x + frame.tangent.x * (totalWidth * 0.5 - wingWidth * 0.5) + shiftX,
            y: center.y + frame.tangent.y * (totalWidth * 0.5 - wingWidth * 0.5) + shiftY
        };
        const backBarCenter = {
            x: center.x + frame.awayFromForest.x * (totalDepth * 0.5 - backDepth * 0.5) + shiftX,
            y: center.y + frame.awayFromForest.y * (totalDepth * 0.5 - backDepth * 0.5) + shiftY
        };

        voxels.push(this.createOrientedVoxel(
            params.type,
            footprint.id,
            leftWingCenter,
            { x: wingWidth, y: wingDepth, z: params.floorHeight },
            params.z,
            params.level,
            frame
        ));
        voxels.push(this.createOrientedVoxel(
            params.type,
            footprint.id,
            rightWingCenter,
            { x: wingWidth, y: wingDepth, z: params.floorHeight },
            params.z,
            params.level,
            frame
        ));
        voxels.push(this.createOrientedVoxel(
            params.type,
            footprint.id,
            backBarCenter,
            { x: totalWidth, y: backDepth, z: params.floorHeight },
            params.z,
            params.level,
            frame
        ));
    }

    generateForestCMixedBuilding(footprint, plot) {
        const voxels = [];
        const heightLimit = this.getHeightLimit(footprint);
        const frame = this.getForestFrame(footprint);
        const baseDims = this.calculateScaledBuildingDimensions(footprint);

        let currentHeight = 0;
        let currentLevel = 0;

        const podiumFloors = 2;
        const workFloors = Math.max(2, Math.floor(this.calculateWorkFloors(plot, footprint) * 0.7));
        const transitionFloors = 1;
        const liveFloors = Math.max(3, Math.floor(this.calculateLiveFloors(plot, footprint, heightLimit) * 0.7));
        const totalFloors = podiumFloors + workFloors + transitionFloors + liveFloors;

        for (let floor = 0; floor < totalFloors && currentHeight < heightLimit; floor++) {
            let type = VoxelType.LIVE;
            if (floor < podiumFloors) type = VoxelType.VISIT;
            else if (floor < podiumFloors + workFloors) type = VoxelType.WORK;
            else if (floor < podiumFloors + workFloors + transitionFloors) type = VoxelType.TRANSITION;

            const floorHeight = this.programRequirements[type].minFloorHeight;
            const floorRatio = floor / Math.max(totalFloors - 1, 1);
            this.addCShapeFloor(voxels, footprint, frame, baseDims, {
                type,
                floorHeight,
                z: currentHeight,
                level: currentLevel++,
                // Upper floors move farther from the forest, creating stepped terraces.
                terraceOffset: floorRatio * baseDims.y * this.forestTerraceStep,
                sizeScale: Math.max(0.65, 1.0 - floorRatio * 0.18)
            });
            currentHeight += floorHeight;
        }

        return voxels;
    }

    generateForestCVillaBuilding(footprint, plot) {
        const voxels = [];
        const frame = this.getForestFrame(footprint);
        const baseDims = this.calculateScaledBuildingDimensions(footprint);
        const heightLimit = this.getHeightLimit(footprint);
        const villaFloors = Math.max(
            2,
            Math.min(6, Math.floor((heightLimit * 0.38) / this.programRequirements[VoxelType.LIVE].minFloorHeight))
        );

        let currentHeight = 0;
        for (let floor = 0; floor < villaFloors; floor++) {
            const floorHeight = this.programRequirements[VoxelType.LIVE].minFloorHeight;
            const floorRatio = floor / Math.max(villaFloors - 1, 1);
            this.addCShapeFloor(voxels, footprint, frame, baseDims, {
                type: VoxelType.LIVE,
                floorHeight,
                z: currentHeight,
                level: floor,
                terraceOffset: floorRatio * baseDims.y * this.forestTerraceStep * 0.85,
                sizeScale: Math.max(0.72, 1.0 - floorRatio * 0.12)
            });
            currentHeight += floorHeight;
        }

        return voxels;
    }

}
