import { pt, distance, normalize, sub, add, mul, dot } from './geometry.js';

export const RoadClassification = Object.freeze({
    PRIMARY_ARTERIAL: 0,
    SECONDARY_ARTERIAL: 1,
    COLLECTOR_ROAD: 2,
    LOCAL_STREET: 3,
    SERVICE_ROAD: 4,
    PEDESTRIAN_PATH: 5
});

export const IntersectionType = Object.freeze({
    T_JUNCTION: 0,
    CROSS_INTERSECTION: 1,
    Y_JUNCTION: 2,
    ROUNDABOUT: 3,
    COMPLEX_INTERSECTION: 4,
    DEAD_END: 5
});

export const NetworkPattern = Object.freeze({
    GRID_PATTERN: 0,
    RADIAL_PATTERN: 1,
    ORGANIC_PATTERN: 2,
    MIXED_PATTERN: 3,
    LINEAR_PATTERN: 4
});

export class AnalyzedRoadSegment {
    constructor() {
        this.start = pt(0, 0);
        this.end = pt(0, 0);
        this.direction = pt(1, 0);
        this.perpendicular = pt(0, 1);
        this.width = 6.0;
        this.length = 0;
        this.classification = RoadClassification.LOCAL_STREET;
        this.name = "";
        this.curveId = -1;
        this.curvature = 0;
        this.importance = 0;
        this.isDeadEnd = false;
        this.isConnector = false;
        this.trafficCapacity = 100;
        this.connectedSegments = [];
        this.intersectionIds = [];
    }

    calculateBasicProperties() {
        this.length = distance(this.start, this.end);
        if (this.length > 0.001) {
            this.direction = normalize(sub(this.end, this.start));
            this.perpendicular = pt(-this.direction.y, this.direction.x);
        }
    }

    getAngleDegrees() {
        return Math.atan2(this.direction.y, this.direction.x) * 180.0 / Math.PI;
    }

    getMidpoint() {
        return pt((this.start.x + this.end.x) * 0.5, (this.start.y + this.end.y) * 0.5);
    }

    getDistanceToPoint(point) {
        const ab = sub(this.end, this.start);
        const ap = sub(point, this.start);

        const abSquared = ab.x * ab.x + ab.y * ab.y;
        if (abSquared === 0) return distance(point, this.start);

        const t = Math.max(0.0, Math.min(1.0, (ap.x * ab.x + ap.y * ab.y) / abSquared));
        const projection = add(this.start, mul(ab, t));

        return distance(point, projection);
    }
}

export class RoadIntersection {
    constructor() {
        this.id = -1;
        this.location = pt(0, 0);
        this.type = IntersectionType.CROSS_INTERSECTION;
        this.connectedRoadIds = [];
        this.roadAngles = [];
        this.importance = 0;
        this.isMajorIntersection = false;
        this.averageRoadWidth = 6.0;
        this.intersectionRadius = 10.0;
        this.isGoodForSubdivision = false;
        this.subdivisionInfluence = 0;
    }

    calculateProperties() {
        if (this.connectedRoadIds.length > 0) {
            this.averageRoadWidth /= this.connectedRoadIds.length;
        }

        this.intersectionRadius = this.averageRoadWidth * 0.5 + 5.0;

        if (this.connectedRoadIds.length === 1) {
            this.type = IntersectionType.DEAD_END;
        } else if (this.connectedRoadIds.length === 3) {
            this.type = IntersectionType.T_JUNCTION;
        } else if (this.connectedRoadIds.length === 4) {
            this.type = IntersectionType.CROSS_INTERSECTION;
        } else if (this.connectedRoadIds.length > 4) {
            this.type = IntersectionType.COMPLEX_INTERSECTION;
        }
    }
}

export class NetworkAnalysis {
    constructor() {
        this.dominantPattern = NetworkPattern.MIXED_PATTERN;
        this.gridRegularity = 0;
        this.connectivity = 0;
        this.density = 0;
        this.averageBlockSize = 100;
        this.primaryDirection = pt(1, 0);
        this.secondaryDirection = pt(0, 1);
        this.directionalBias = 0;
    }
}

export class RoadAnalyzer {
    constructor(siteGeneration) {
        this.siteGen = siteGeneration;
        this.analyzedSegments = [];
        this.intersections = [];
        this.networkAnalysis = new NetworkAnalysis();

        // Parameters
        this.INTERSECTION_TOLERANCE = 1.0;
        this.PARALLEL_TOLERANCE = 15.0;
        this.MAJOR_ROAD_WIDTH_THRESHOLD = 1.0;
        this.CURVATURE_THRESHOLD = 0.1;
        this.GRID_ALIGNMENT_TOLERANCE = 20.0;

        if (this.siteGen) {
            this.analyzeNetwork();
        }
    }

    analyzeNetwork() {
        console.log("=== ANALYZING ROAD NETWORK ===");
        this.buildAnalyzedRoadSegments();
        this.analyzeIntersections();
        this.analyzeNetworkPattern();
        this.calculateSegmentImportance();
        console.log("Road network analysis complete.");
    }

    buildAnalyzedRoadSegments() {
        if (!this.siteGen) return;
        this.analyzedSegments = [];

        const majorRoads = this.siteGen.getMajorRoads();
        for (let i = 0; i < majorRoads.length; i++) {
            this.processRoadForAnalysis(majorRoads[i], i, true);
        }

        const localRoads = this.siteGen.getLocalRoads();
        for (let i = 0; i < localRoads.length; i++) {
            this.processRoadForAnalysis(localRoads[i], majorRoads.length + i, false);
        }

        for (let segment of this.analyzedSegments) {
            this.calculateSegmentCurvature(segment);
            segment.calculateBasicProperties();
        }

        console.log(`Built ${this.analyzedSegments.length} analyzed road segments`);
    }

    processRoadForAnalysis(road, roadId, isMajor) {
        if (road.points.length < 2) return;

        const classification = isMajor ?
            (road.width >= this.MAJOR_ROAD_WIDTH_THRESHOLD ? RoadClassification.PRIMARY_ARTERIAL : RoadClassification.SECONDARY_ARTERIAL) :
            this.classifyRoad(road);

        for (let i = 0; i < road.points.length - 1; i++) {
            let segment = new AnalyzedRoadSegment();
            segment.start = road.points[i];
            segment.end = road.points[i + 1];
            segment.width = road.width;
            segment.classification = classification;
            segment.name = road.name || "";
            segment.curveId = roadId;
            segment.calculateBasicProperties();

            this.analyzedSegments.push(segment);
        }
    }

    classifyRoad(road) {
        if (road.width >= 1.5) { // scaled from 15.0
            return RoadClassification.PRIMARY_ARTERIAL;
        } else if (road.width >= 1.0) { // scaled from 10.0
            return RoadClassification.SECONDARY_ARTERIAL;
        } else if (road.width >= 0.7) { // scaled from 7.0
            return RoadClassification.COLLECTOR_ROAD;
        } else if (road.width >= 0.4) { // scaled from 4.0
            return RoadClassification.LOCAL_STREET;
        } else {
            return RoadClassification.SERVICE_ROAD;
        }
    }

    calculateSegmentCurvature(segment) {
        segment.curvature = 0.0;
    }

    analyzeIntersections() {
        this.intersections = [];
        let potentialIntersections = [];

        for (let i = 0; i < this.analyzedSegments.length; i++) {
            for (let j = i + 1; j < this.analyzedSegments.length; j++) {
                let intersection = pt(0, 0);
                if (this.calculateIntersection(this.analyzedSegments[i], this.analyzedSegments[j], intersection)) {
                    potentialIntersections.push({ r1: i, r2: j, pt: intersection });
                }
            }
        }

        let processed = new Array(potentialIntersections.length).fill(false);
        let intersectionId = 0;

        for (let idx = 0; idx < potentialIntersections.length; idx++) {
            if (processed[idx]) continue;

            let pi = potentialIntersections[idx];
            let intersection = new RoadIntersection();
            intersection.id = intersectionId++;
            intersection.location = pi.pt;
            intersection.connectedRoadIds.push(pi.r1, pi.r2);

            for (let idx2 = idx + 1; idx2 < potentialIntersections.length; idx2++) {
                if (processed[idx2]) continue;
                let pi2 = potentialIntersections[idx2];

                if (distance(pi.pt, pi2.pt) < this.INTERSECTION_TOLERANCE) {
                    intersection.connectedRoadIds.push(pi2.r1, pi2.r2);
                    processed[idx2] = true;
                }
            }

            intersection.connectedRoadIds = [...new Set(intersection.connectedRoadIds)];
            this.calculateIntersectionProperties(intersection);
            this.intersections.push(intersection);
            processed[idx] = true;
        }

        this.updateSegmentIntersectionReferences();
        console.log(`Found ${this.intersections.length} road intersections`);
    }

    calculateIntersection(seg1, seg2, intersectionObj) {
        const x1 = seg1.start.x, y1 = seg1.start.y;
        const x2 = seg1.end.x, y2 = seg1.end.y;
        const x3 = seg2.start.x, y3 = seg2.start.y;
        const x4 = seg2.end.x, y4 = seg2.end.y;

        const denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

        if (Math.abs(denom) < 0.001) {
            return false;
        }

        const t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        const u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

        if (t >= -0.1 && t <= 1.1 && u >= -0.1 && u <= 1.1) {
            intersectionObj.x = x1 + t * (x2 - x1);
            intersectionObj.y = y1 + t * (y2 - y1);
            return true;
        }

        return false;
    }

    calculateIntersectionProperties(intersection) {
        intersection.calculateProperties();
        let importance = 0;
        intersection.averageRoadWidth = 0;

        for (let roadId of intersection.connectedRoadIds) {
            if (roadId >= 0 && roadId < this.analyzedSegments.length) {
                const segment = this.analyzedSegments[roadId];
                switch (segment.classification) {
                    case RoadClassification.PRIMARY_ARTERIAL: importance += 3.0; break;
                    case RoadClassification.SECONDARY_ARTERIAL: importance += 2.0; break;
                    case RoadClassification.COLLECTOR_ROAD: importance += 1.5; break;
                    default: importance += 1.0; break;
                }
                intersection.averageRoadWidth += segment.width;

                const toIntersection = sub(intersection.location, segment.getMidpoint());
                const angle = Math.atan2(toIntersection.y, toIntersection.x) * 180.0 / Math.PI;
                intersection.roadAngles.push(angle);
            }
        }

        intersection.importance = importance / intersection.connectedRoadIds.length;
        intersection.averageRoadWidth /= intersection.connectedRoadIds.length;
        intersection.isMajorIntersection = (intersection.importance >= 2.0);

        intersection.isGoodForSubdivision = (intersection.type === IntersectionType.CROSS_INTERSECTION ||
            intersection.type === IntersectionType.T_JUNCTION) &&
            intersection.isMajorIntersection;

        intersection.subdivisionInfluence = intersection.importance * 5.0;
    }

    updateSegmentIntersectionReferences() {
        for (let segIdx = 0; segIdx < this.analyzedSegments.length; segIdx++) {
            let segment = this.analyzedSegments[segIdx];
            for (let intIdx = 0; intIdx < this.intersections.length; intIdx++) {
                const intersection = this.intersections[intIdx];
                if (intersection.connectedRoadIds.includes(segIdx)) {
                    segment.intersectionIds.push(intIdx);
                }
            }
        }
    }

    analyzeNetworkPattern() {
        this.calculateGridAlignment();
        this.calculateConnectivity();
        this.calculateNetworkDensity();
        this.determineNetworkPattern();
        console.log("Network pattern analysis complete");
    }

    calculateGridAlignment() {
        let angleHistogram = {};
        for (const segment of this.analyzedSegments) {
            if (segment.classification <= RoadClassification.COLLECTOR_ROAD) {
                let angle = segment.getAngleDegrees();
                while (angle < 0) angle += 180;
                while (angle >= 180) angle -= 180;

                let angleBin = Math.floor(angle / 10) * 10;
                angleHistogram[angleBin] = (angleHistogram[angleBin] || 0) + 1;
            }
        }

        let angleFreqs = Object.entries(angleHistogram).map(([k, v]) => [parseInt(k), v]);
        angleFreqs.sort((a, b) => b[1] - a[1]);

        if (angleFreqs.length > 0) {
            const primaryAngle = angleFreqs[0][0] * Math.PI / 180.0;
            this.networkAnalysis.primaryDirection = pt(Math.cos(primaryAngle), Math.sin(primaryAngle));

            if (angleFreqs.length > 1) {
                const secondaryAngle = angleFreqs[1][0] * Math.PI / 180.0;
                this.networkAnalysis.secondaryDirection = pt(Math.cos(secondaryAngle), Math.sin(secondaryAngle));

                const dotProduct = Math.abs(this.networkAnalysis.primaryDirection.x * this.networkAnalysis.secondaryDirection.x +
                    this.networkAnalysis.primaryDirection.y * this.networkAnalysis.secondaryDirection.y);

                if (dotProduct < 0.2) {
                    this.networkAnalysis.gridRegularity = (angleFreqs[0][1] + angleFreqs[1][1]) / this.analyzedSegments.length;
                } else {
                    this.networkAnalysis.secondaryDirection = pt(-this.networkAnalysis.primaryDirection.y, this.networkAnalysis.primaryDirection.x);
                    this.networkAnalysis.gridRegularity = angleFreqs[0][1] / this.analyzedSegments.length;
                }
            } else {
                this.networkAnalysis.secondaryDirection = pt(-this.networkAnalysis.primaryDirection.y, this.networkAnalysis.primaryDirection.x);
                this.networkAnalysis.gridRegularity = angleFreqs[0][1] / this.analyzedSegments.length;
            }
        } else {
            this.networkAnalysis.primaryDirection = pt(1, 0);
            this.networkAnalysis.secondaryDirection = pt(0, 1);
            this.networkAnalysis.gridRegularity = 0;
        }

        this.networkAnalysis.directionalBias = this.networkAnalysis.gridRegularity;
    }

    calculateConnectivity() {
        if (this.intersections.length === 0) {
            this.networkAnalysis.connectivity = 0;
            return;
        }

        let totalConnections = 0;
        for (const intersection of this.intersections) {
            totalConnections += intersection.connectedRoadIds.length;
        }

        this.networkAnalysis.connectivity = totalConnections / this.intersections.length;
    }

    calculateNetworkDensity() {
        if (this.analyzedSegments.length === 0) {
            this.networkAnalysis.density = 0;
            return;
        }

        let totalLength = 0;
        let minX = this.analyzedSegments[0].start.x, maxX = minX;
        let minY = this.analyzedSegments[0].start.y, maxY = minY;

        for (const segment of this.analyzedSegments) {
            totalLength += segment.length;
            minX = Math.min(minX, Math.min(segment.start.x, segment.end.x));
            maxX = Math.max(maxX, Math.max(segment.start.x, segment.end.x));
            minY = Math.min(minY, Math.min(segment.start.y, segment.end.y));
            maxY = Math.max(maxY, Math.max(segment.start.y, segment.end.y));
        }

        const area = (maxX - minX) * (maxY - minY);
        this.networkAnalysis.density = area > 0 ? totalLength / area : 0;

        let avgIntersectionSpacing = 0;
        let spacingCount = 0;

        for (let i = 0; i < this.intersections.length; i++) {
            for (let j = 0; j < this.intersections.length; j++) {
                if (i !== j) {
                    let d = distance(this.intersections[i].location, this.intersections[j].location);
                    if (d < 30) {
                        avgIntersectionSpacing += d;
                        spacingCount++;
                    }
                }
            }
        }

        if (spacingCount > 0) {
            this.networkAnalysis.averageBlockSize = avgIntersectionSpacing / spacingCount;
        } else {
            this.networkAnalysis.averageBlockSize = 100;
        }
    }

    determineNetworkPattern() {
        if (this.networkAnalysis.gridRegularity > 0.6) {
            this.networkAnalysis.dominantPattern = NetworkPattern.GRID_PATTERN;
        } else if (this.networkAnalysis.connectivity > 3.5) {
            this.networkAnalysis.dominantPattern = NetworkPattern.ORGANIC_PATTERN;
        } else {
            this.networkAnalysis.dominantPattern = NetworkPattern.MIXED_PATTERN;
        }
    }

    calculateSegmentImportance() {
        for (let segment of this.analyzedSegments) {
            segment.importance = 1.0;
            if (segment.classification <= RoadClassification.SECONDARY_ARTERIAL) {
                segment.importance += 2.0;
            }
        }
    }
}
