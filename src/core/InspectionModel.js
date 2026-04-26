import { VoxelType } from './Building3DGenerator.js';

export const InspectionColorMode = Object.freeze({
    ZERO: 'ZERO',
    HEIGHT_GRADIENT: 'HEIGHT_GRADIENT',
    DISTANCE_TO_CENTERS: 'DISTANCE_TO_CENTERS',
    DENSITY: 'DENSITY',
    FAR: 'FAR'
});

export const InspectionLabelConfig = Object.freeze({
    shapeTypes: {
        PODIUM_TOWER: 'Podium + Tower Mixed Use',
        UNIFORM_MIXED: 'Uniform Mid-Rise Block',
        HORIZONTAL_ZONES: 'Horizontal Program Bands',
        VERTICAL_ZONES: 'Stacked Vertical Mixed Use',
        COURTYARD_BLOCK: 'Courtyard Block',
        LARGE_SPLIT_COURTYARD: 'Split-Courtyard Low Density Mass',
        STEPPED_MASSING: 'Staggered Mid Density Mass',
        FOREST_C_MIXED: 'Forest-Facing C-Block Mixed Use',
        FOREST_C_VILLA: 'Forest-Facing C-Villa Cluster',
        UNKNOWN: 'Custom Typology'
    },
    programs: {
        VISIT: 'Retail',
        WORK: 'Work',
        LIVE: 'Live',
        TRANSITION: 'Amenity',
        NONE: 'Undefined'
    }
});

const ProgramCostFactor = Object.freeze({
    VISIT: 1.2,
    WORK: 1.1,
    LIVE: 1.0,
    TRANSITION: 0.85,
    NONE: 1.0
});

const BASE_COST_PER_M3_ABSTRACT = 1.0;

function clamp01(v) {
    return Math.max(0, Math.min(1, v));
}

function polygonArea2D(points) {
    if (!points || points.length < 3) return 0;
    let area = 0;
    for (let i = 0; i < points.length; i++) {
        const j = (i + 1) % points.length;
        area += points[i].x * points[j].y - points[j].x * points[i].y;
    }
    return Math.abs(area) * 0.5;
}

function getRange(values) {
    const finite = values.filter((v) => Number.isFinite(v));
    if (finite.length === 0) return { min: 0, max: 1 };
    let min = Infinity;
    let max = -Infinity;
    for (const v of finite) {
        if (v < min) min = v;
        if (v > max) max = v;
    }
    if (Math.abs(max - min) < 1e-8) max = min + 1e-8;
    return { min, max };
}

function normalizeToRange(value, range, invert = false) {
    if (!Number.isFinite(value)) return 0;
    const denom = Math.max(1e-8, range.max - range.min);
    let t = clamp01((value - range.min) / denom);
    if (invert) t = 1.0 - t;
    return Math.round(t * 100) / 100;
}

function hslToHexNumber(h, s, l) {
    const hh = ((h % 360) + 360) % 360;
    const ss = clamp01(s / 100);
    const ll = clamp01(l / 100);

    const c = (1 - Math.abs(2 * ll - 1)) * ss;
    const x = c * (1 - Math.abs(((hh / 60) % 2) - 1));
    const m = ll - c / 2;
    let r = 0, g = 0, b = 0;

    if (hh < 60) { r = c; g = x; b = 0; }
    else if (hh < 120) { r = x; g = c; b = 0; }
    else if (hh < 180) { r = 0; g = c; b = x; }
    else if (hh < 240) { r = 0; g = x; b = c; }
    else if (hh < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    const rr = Math.round((r + m) * 255);
    const gg = Math.round((g + m) * 255);
    const bb = Math.round((b + m) * 255);

    return (rr << 16) | (gg << 8) | bb;
}

function colorFromMode(mode, value, ranges) {
    if (!mode || mode === InspectionColorMode.ZERO) return null;

    let hue = 200;
    let invert = false;
    let range = ranges.heightM;

    if (mode === InspectionColorMode.HEIGHT_GRADIENT) {
        hue = 44;
        range = ranges.heightM;
        invert = false;
    } else if (mode === InspectionColorMode.DISTANCE_TO_CENTERS) {
        hue = 206;
        range = ranges.distanceToCenterM;
        invert = true; // closer to center = brighter
    } else if (mode === InspectionColorMode.DENSITY) {
        hue = 152;
        range = ranges.density;
        invert = false;
    } else if (mode === InspectionColorMode.FAR) {
        hue = 288;
        range = ranges.far;
        invert = false;
    }

    const t = normalizeToRange(value, range, invert);
    const lightness = 18 + (t * 62);
    return hslToHexNumber(hue, 72, lightness);
}

function dominantProgramFromAreas(programAreas) {
    let bestType = VoxelType.NONE;
    let bestArea = -1;
    for (const [type, area] of Object.entries(programAreas)) {
        if (area > bestArea) {
            bestArea = area;
            bestType = type;
        }
    }
    return bestType;
}

function friendlyShapeType(shapeType) {
    return InspectionLabelConfig.shapeTypes[shapeType] || InspectionLabelConfig.shapeTypes.UNKNOWN;
}

function friendlyProgramType(programType) {
    return InspectionLabelConfig.programs[programType] || InspectionLabelConfig.programs.NONE;
}

function buildProgramMixLabel(programAreas) {
    const ranked = Object.entries(programAreas || {})
        .filter(([k, v]) => k !== VoxelType.NONE && Number.isFinite(v) && v > 1e-6)
        .sort((a, b) => b[1] - a[1]);
    if (ranked.length === 0) return InspectionLabelConfig.programs.NONE;
    return ranked.map(([k]) => friendlyProgramType(k)).join(' + ');
}

export function buildInspectionModel({
    parcelSubdivider,
    footprintGenerator,
    building3DGenerator,
    unitToMeters = 10.0,
    areaUnitToM2 = 100.0,
    mapColorMode = InspectionColorMode.ZERO,
    buildingColorMode = InspectionColorMode.ZERO
} = {}) {
    const plotMetricsById = {};
    const footprintMetricsById = {};
    const footprintsByPlotId = {};
    const plotIdByFootprintId = {};
    const mapColorByPlotId = {};
    const buildingColorByFootprintId = {};

    const plots = (parcelSubdivider?.allPlots || []).filter((p) => !p.isOpenSpace);
    const footprints = footprintGenerator?.buildingFootprints || [];
    const voxels = building3DGenerator?.building3DVoxels || [];

    const plotById = new Map(plots.map((p) => [p.id, p]));
    const voxelsByFootprintId = new Map();
    for (const v of voxels) {
        const id = v.parentFootprintId;
        if (!voxelsByFootprintId.has(id)) voxelsByFootprintId.set(id, []);
        voxelsByFootprintId.get(id).push(v);
    }

    for (const plot of plots) {
        const areaM2 = (plot.area || 0) * areaUnitToM2;
        plotMetricsById[plot.id] = {
            plotId: plot.id,
            areaM2,
            gfaM2: 0,
            voxelVolumeM3: 0,
            maxHeightM: 0,
            maxFloors: 0,
            distanceToCenterM: Infinity,
            coverageRatio: 0,
            density: 0,
            far: 0,
            dominantProgram: VoxelType.NONE,
            programMixLabel: InspectionLabelConfig.programs.NONE,
            shapeTypes: [],
            shapeTypeFriendlyList: [],
            footprintCount: 0
        };
        footprintsByPlotId[plot.id] = [];
    }

    for (const footprint of footprints) {
        const plot = plotById.get(footprint.parentPlotId);
        if (!plot) continue;

        const plotAreaM2 = Math.max(1e-6, (plot.area || 0) * areaUnitToM2);
        const footprintAreaM2 = Math.max(0, (footprint.area || 0) * areaUnitToM2);
        const footprintVoxels = voxelsByFootprintId.get(footprint.id) || [];

        let gfaM2 = 0;
        let voxelVolumeM3 = 0;
        let maxTopWorld = 0;
        let maxLevel = -1;
        const programAreas = {
            [VoxelType.VISIT]: 0,
            [VoxelType.WORK]: 0,
            [VoxelType.LIVE]: 0,
            [VoxelType.TRANSITION]: 0,
            [VoxelType.NONE]: 0
        };

        for (const v of footprintVoxels) {
            const plateAreaWorld = v.shapePolygon && v.shapePolygon.length >= 3
                ? polygonArea2D(v.shapePolygon)
                : Math.max(0, (v.dimensions?.x || 0) * (v.dimensions?.y || 0));
            const plateAreaM2 = plateAreaWorld * areaUnitToM2;
            gfaM2 += plateAreaM2;
            const voxelHeightWorld = Math.max(0, v.dimensions?.z || 0);
            const voxelHeightM = voxelHeightWorld * unitToMeters;
            const volumeM3 = plateAreaM2 * voxelHeightM;
            voxelVolumeM3 += volumeM3;
            const top = (v.position?.z || 0) + (v.dimensions?.z || 0);
            if (top > maxTopWorld) maxTopWorld = top;
            maxLevel = Math.max(maxLevel, Number.isFinite(v.level) ? v.level : 0);
            const t = v.type || VoxelType.NONE;
            if (!Object.prototype.hasOwnProperty.call(programAreas, t)) programAreas[t] = 0;
            programAreas[t] += plateAreaM2;
        }

        const floors = maxLevel >= 0 ? (maxLevel + 1) : 0;
        const heightM = maxTopWorld * unitToMeters;
        const nearestAttractorWorld = building3DGenerator?.getNearestAttractorDistance?.(footprint) ?? Infinity;
        const distanceToCenterM = Number.isFinite(nearestAttractorWorld) ? nearestAttractorWorld * unitToMeters : Infinity;
        const dominantProgram = dominantProgramFromAreas(programAreas);
        const strategyName = footprint.generatedStrategy || 'UNKNOWN';
        const strategyLabel = friendlyShapeType(strategyName);
        const far = gfaM2 / plotAreaM2;
        const density = footprintAreaM2 / plotAreaM2;
        const token = footprintVoxels.length;
        let estimatedVoxelCost = 0;
        for (const [programType, areaM2] of Object.entries(programAreas)) {
            if (!Number.isFinite(areaM2) || areaM2 <= 0) continue;
            const factor = ProgramCostFactor[programType] ?? ProgramCostFactor.NONE;
            estimatedVoxelCost += areaM2 * factor;
        }
        estimatedVoxelCost *= BASE_COST_PER_M3_ABSTRACT * 0.1;
        const programMixLabel = buildProgramMixLabel(programAreas);

        footprintMetricsById[footprint.id] = {
            footprintId: footprint.id,
            plotId: plot.id,
            shapeType: strategyName,
            shapeTypeFriendly: strategyLabel,
            dominantProgram,
            dominantProgramFriendly: friendlyProgramType(dominantProgram),
            programMixLabel,
            floors,
            gfaM2,
            voxelVolumeM3,
            voxelTokenCount: token,
            estimatedVoxelCost,
            heightM,
            distanceToCenterM,
            density,
            far,
            areaM2: footprintAreaM2
        };

        plotIdByFootprintId[footprint.id] = plot.id;
        footprintsByPlotId[plot.id].push(footprint.id);

        const agg = plotMetricsById[plot.id];
        agg.gfaM2 += gfaM2;
        agg.voxelVolumeM3 += voxelVolumeM3;
        agg.maxHeightM = Math.max(agg.maxHeightM, heightM);
        agg.maxFloors = Math.max(agg.maxFloors, floors);
        agg.distanceToCenterM = Math.min(agg.distanceToCenterM, distanceToCenterM);
        agg.coverageRatio += density;
        agg.footprintCount += 1;
        if (!agg.shapeTypes.includes(strategyName)) agg.shapeTypes.push(strategyName);
        if (!agg.shapeTypeFriendlyList.includes(strategyLabel)) agg.shapeTypeFriendlyList.push(strategyLabel);
        if (dominantProgram !== VoxelType.NONE) {
            if (!agg._programAreas) agg._programAreas = {};
            agg._programAreas[dominantProgram] = (agg._programAreas[dominantProgram] || 0) + gfaM2;
        }
        if (!agg._tokenCount) agg._tokenCount = 0;
        agg._tokenCount += token;
        if (!agg._estimatedVoxelCost) agg._estimatedVoxelCost = 0;
        agg._estimatedVoxelCost += estimatedVoxelCost;
    }

    const heightVals = [];
    const distVals = [];
    const densityVals = [];
    const farVals = [];

    for (const metric of Object.values(plotMetricsById)) {
        if (metric.footprintCount > 0) {
            const area = Math.max(1e-6, metric.areaM2);
            metric.coverageRatio = clamp01(metric.coverageRatio);
            metric.density = metric.coverageRatio;
            metric.far = metric.gfaM2 / area;
            metric.dominantProgram = dominantProgramFromAreas(metric._programAreas || {});
            metric.dominantProgramFriendly = friendlyProgramType(metric.dominantProgram);
            metric.programMixLabel = buildProgramMixLabel(metric._programAreas || {});
            metric.voxelTokenCount = metric._tokenCount || 0;
            metric.estimatedVoxelCost = metric._estimatedVoxelCost || 0;
        } else {
            metric.coverageRatio = 0;
            metric.density = 0;
            metric.far = 0;
            metric.maxHeightM = 0;
            metric.maxFloors = 0;
            metric.distanceToCenterM = Infinity;
            metric.dominantProgram = VoxelType.NONE;
            metric.dominantProgramFriendly = InspectionLabelConfig.programs.NONE;
            metric.programMixLabel = InspectionLabelConfig.programs.NONE;
            metric.voxelTokenCount = 0;
            metric.estimatedVoxelCost = 0;
        }
        delete metric._programAreas;
        delete metric._tokenCount;
        delete metric._estimatedVoxelCost;

        heightVals.push(metric.maxHeightM);
        if (Number.isFinite(metric.distanceToCenterM)) distVals.push(metric.distanceToCenterM);
        densityVals.push(metric.density);
        farVals.push(metric.far);
    }

    const ranges = {
        heightM: getRange(heightVals),
        distanceToCenterM: getRange(distVals),
        density: getRange(densityVals),
        far: getRange(farVals)
    };

    for (const [plotIdText, metric] of Object.entries(plotMetricsById)) {
        const plotId = Number(plotIdText);
        let metricValue = 0;
        if (mapColorMode === InspectionColorMode.HEIGHT_GRADIENT) metricValue = metric.maxHeightM;
        else if (mapColorMode === InspectionColorMode.DISTANCE_TO_CENTERS) metricValue = metric.distanceToCenterM;
        else if (mapColorMode === InspectionColorMode.DENSITY) metricValue = metric.density;
        else if (mapColorMode === InspectionColorMode.FAR) metricValue = metric.far;
        const c = colorFromMode(mapColorMode, metricValue, ranges);
        if (c !== null) mapColorByPlotId[plotId] = c;
    }

    for (const [footprintIdText, metric] of Object.entries(footprintMetricsById)) {
        const footprintId = Number(footprintIdText);
        let metricValue = 0;
        if (buildingColorMode === InspectionColorMode.HEIGHT_GRADIENT) metricValue = metric.heightM;
        else if (buildingColorMode === InspectionColorMode.DISTANCE_TO_CENTERS) metricValue = metric.distanceToCenterM;
        else if (buildingColorMode === InspectionColorMode.DENSITY) metricValue = metric.density;
        else if (buildingColorMode === InspectionColorMode.FAR) metricValue = metric.far;
        const c = colorFromMode(buildingColorMode, metricValue, ranges);
        if (c !== null) buildingColorByFootprintId[footprintId] = c;
    }

    return {
        plotMetricsById,
        footprintMetricsById,
        footprintsByPlotId,
        plotIdByFootprintId,
        mapColorByPlotId,
        buildingColorByFootprintId,
        ranges,
        labelConfig: InspectionLabelConfig
    };
}
