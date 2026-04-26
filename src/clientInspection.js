import './clientInspection.css';
import { SiteGeneration } from './core/SiteGeneration.js';
import { ParcelAnalyzer } from './core/ParcelAnalyzer.js';
import { RoadAnalyzer } from './core/RoadAnalyzer.js';
import { ParcelSubdivider } from './core/ParcelSubdivider.js';
import { BuildingFootprintGenerator } from './core/BuildingFootprintGenerator.js';
import { Building3DGenerator } from './core/Building3DGenerator.js';
import { ThreeRenderer } from './ui/ThreeRenderer.js';
import { InspectionColorMode, buildInspectionModel } from './core/InspectionModel.js';

const UNIT_TO_METERS = 10.0;
const AREA_UNIT_TO_M2 = UNIT_TO_METERS * UNIT_TO_METERS;
const RED_ATTRACTOR_PEAK_FACTOR = 1.0;
const BLUE_ATTRACTOR_PEAK_FACTOR = 0.6;
const BUILDING_COLOR_PROGRAM = 'PROGRAM_TYPE';

const DEFAULT_PARAMS = {
    largeThresh: 95,
    mediumThresh: 36,
    largeTarget: 25,
    mediumTarget: 40,
    smallTarget: 25,
    subdivScale: 1.0,
    greenRatio: 1.0,
    heightScale: 1.0,
    voxelSize: 1.0,
    attractorRange: 130,
    attractorPeakHeightM: 180,
    attractorLowHeightM: 20
};

const STATE = {
    mapColorMode: InspectionColorMode.ZERO,
    buildingColorMode: InspectionColorMode.ZERO,
    transparent: true,
    showPlots: true,
    firstPerson: false,
    selectedPlotId: null,
    selectedFootprintId: null
};

const PARAMS = {
    ...DEFAULT_PARAMS,
    attractors: []
};

let siteGen = null;
let parcelAnalyzer = null;
let roadAnalyzer = null;
let parcelSubdivider = null;
let footprintGenerator = null;
let building3DGenerator = null;
let threeRenderer = null;
let inspectionModel = null;
let activeRightTab = 'plot';

function setStatus(message) {
    const statusEl = document.getElementById('clientStatus');
    if (statusEl) statusEl.innerText = message;
}

function parseQuotedCSVLine(line) {
    const tokens = [];
    const regex = /"([^"]*)"/g;
    let match;
    while ((match = regex.exec(line)) !== null) tokens.push(match[1]);
    return tokens;
}

async function loadAttractorsFromCSV(url) {
    try {
        const response = await fetch(url);
        if (!response.ok) return false;
        const text = await response.text();
        const lines = text.split(/\r?\n/).filter((l) => l.trim().length > 0);
        if (lines.length <= 1) return false;

        const points = [];
        for (let i = 1; i < lines.length; i++) {
            const tokens = parseQuotedCSVLine(lines[i]);
            if (tokens.length < 5) continue;

            const layerName = tokens[1] || '';
            const pointText = tokens[4] || '';
            if (!pointText.includes(',')) continue;

            const coords = pointText.split(',').map((v) => parseFloat(v));
            if (coords.length < 2 || Number.isNaN(coords[0]) || Number.isNaN(coords[1])) continue;

            const isPrimary = /red_attractor/i.test(layerName);
            const worldScale = siteGen?.AUTO_SCALE_FACTOR ?? 0.1;
            points.push({
                x: coords[0] * worldScale,
                y: coords[1] * worldScale,
                isPrimary,
                weight: isPrimary ? RED_ATTRACTOR_PEAK_FACTOR : BLUE_ATTRACTOR_PEAK_FACTOR
            });
        }

        if (points.length === 0) return false;
        if (!points.some((p) => p.isPrimary)) {
            points[0].isPrimary = true;
            points[0].weight = RED_ATTRACTOR_PEAK_FACTOR;
            for (let i = 1; i < points.length; i++) {
                points[i].isPrimary = false;
                points[i].weight = BLUE_ATTRACTOR_PEAK_FACTOR;
            }
        }
        PARAMS.attractors = points;
        return true;
    } catch (err) {
        console.warn('Failed to load attractors CSV', err);
        return false;
    }
}

function ensureFallbackAttractors() {
    if (!siteGen || !siteGen.curves || siteGen.curves.length === 0) return;
    if (PARAMS.attractors.length > 0) return;

    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const curve of siteGen.curves) {
        for (const pt of curve.points) {
            minX = Math.min(minX, pt.x);
            maxX = Math.max(maxX, pt.x);
            minY = Math.min(minY, pt.y);
            maxY = Math.max(maxY, pt.y);
        }
    }
    const cx = (minX + maxX) * 0.5;
    const cy = (minY + maxY) * 0.5;
    PARAMS.attractors = [
        { x: cx, y: cy, isPrimary: true, weight: RED_ATTRACTOR_PEAK_FACTOR },
        { x: cx + (maxX - cx) * 0.5, y: cy + (maxY - cy) * 0.5, isPrimary: false, weight: BLUE_ATTRACTOR_PEAK_FACTOR },
        { x: cx - (cx - minX) * 0.5, y: cy + (maxY - cy) * 0.5, isPrimary: false, weight: BLUE_ATTRACTOR_PEAK_FACTOR }
    ];
}

function getSiteSpan() {
    if (!siteGen || !siteGen.curves || siteGen.curves.length === 0) return 400;
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const curve of siteGen.curves) {
        for (const p of curve.points) {
            minX = Math.min(minX, p.x);
            maxX = Math.max(maxX, p.x);
            minY = Math.min(minY, p.y);
            maxY = Math.max(maxY, p.y);
        }
    }
    return Math.max(maxX - minX, maxY - minY);
}

function applyClientVisualPreset() {
    if (!threeRenderer) return;
    threeRenderer.setViewModes({ technical: false, rendered: true });
    threeRenderer.scene.background.setHex(0x000000);
    if (threeRenderer.groundPlane?.material?.color) {
        threeRenderer.groundPlane.material.color.setHex(0x061316);
    }
    if (threeRenderer.gridHelper?.material) {
        threeRenderer.gridHelper.visible = true;
        threeRenderer.gridHelper.material.opacity = 0.1;
        threeRenderer.gridHelper.material.transparent = true;
    }
    if (threeRenderer.ambientLight) threeRenderer.ambientLight.intensity = 0.16;
    if (threeRenderer.dirLight) threeRenderer.dirLight.intensity = 1.2;
    if (threeRenderer.skyLight) threeRenderer.skyLight.intensity = 0.66;
    if (threeRenderer.rimLight) threeRenderer.rimLight.intensity = 0.25;
    // Client mode performance preset.
    if (threeRenderer.renderer) threeRenderer.renderer.shadowMap.enabled = false;
    if (threeRenderer.dirLight) threeRenderer.dirLight.castShadow = false;
    if (threeRenderer.groundPlane) threeRenderer.groundPlane.receiveShadow = false;
}

function rebuildInspectionModel() {
    if (!parcelSubdivider || !footprintGenerator || !building3DGenerator) {
        inspectionModel = null;
        return;
    }
    const buildingModeForMetrics = STATE.buildingColorMode === BUILDING_COLOR_PROGRAM
        ? InspectionColorMode.ZERO
        : STATE.buildingColorMode;
    inspectionModel = buildInspectionModel({
        parcelSubdivider,
        footprintGenerator,
        building3DGenerator,
        unitToMeters: UNIT_TO_METERS,
        areaUnitToM2: AREA_UNIT_TO_M2,
        mapColorMode: STATE.mapColorMode,
        buildingColorMode: buildingModeForMetrics
    });

    if (STATE.selectedPlotId !== null && !inspectionModel.plotMetricsById[STATE.selectedPlotId]) {
        STATE.selectedPlotId = null;
        STATE.selectedFootprintId = null;
    }
    if (STATE.selectedFootprintId !== null && !inspectionModel.footprintMetricsById[STATE.selectedFootprintId]) {
        STATE.selectedFootprintId = null;
    }
    if (STATE.selectedPlotId !== null && STATE.selectedFootprintId === null) {
        const ids = inspectionModel.footprintsByPlotId[STATE.selectedPlotId] || [];
        STATE.selectedFootprintId = ids.length > 0 ? ids[0] : null;
    }
}

function buildDefaultBluePlotColors() {
    const out = {};
    if (!inspectionModel) return out;
    for (const id of Object.keys(inspectionModel.plotMetricsById)) {
        out[Number(id)] = 0x0e8e92;
    }
    return out;
}

function buildDefaultBlueBuildingColors() {
    const out = {};
    if (!inspectionModel) return out;
    for (const id of Object.keys(inspectionModel.footprintMetricsById)) {
        out[Number(id)] = 0x26d8d8;
    }
    return out;
}

function formatMetric(v, decimals = 1) {
    if (!Number.isFinite(v)) return 'N/A';
    return Number(v).toFixed(decimals);
}

function updateInspectorPanel() {
    const titleEl = document.getElementById('inspectorTitle');
    const bodyEl = document.getElementById('inspectorBody');
    if (!titleEl || !bodyEl) return;

    if (!inspectionModel || STATE.selectedPlotId === null) {
        titleEl.innerText = 'Plot Inspector';
        bodyEl.innerText = 'Select a plot to inspect attributes.';
        return;
    }

    const plotMetric = inspectionModel.plotMetricsById[STATE.selectedPlotId];
    if (!plotMetric) {
        titleEl.innerText = 'Plot Inspector';
        bodyEl.innerText = 'No data for selected plot.';
        return;
    }

    const fpMetric = Number.isFinite(STATE.selectedFootprintId)
        ? inspectionModel.footprintMetricsById[STATE.selectedFootprintId]
        : null;

    const shapeType = fpMetric?.shapeTypeFriendly
        || (plotMetric.shapeTypeFriendlyList?.join(', ') || 'Custom Typology');
    const program = fpMetric?.programMixLabel || plotMetric.programMixLabel || 'Undefined';
    const floors = fpMetric?.floors ?? plotMetric.maxFloors;
    const gfa = fpMetric?.gfaM2 ?? plotMetric.gfaM2;
    const far = fpMetric?.far ?? plotMetric.far;
    const density = fpMetric?.density ?? plotMetric.density;
    const heightM = fpMetric?.heightM ?? plotMetric.maxHeightM;
    const distM = fpMetric?.distanceToCenterM ?? plotMetric.distanceToCenterM;
    const areaM2 = fpMetric?.areaM2 ?? plotMetric.areaM2;
    const token = fpMetric?.voxelTokenCount ?? plotMetric.voxelTokenCount ?? 0;
    const volumeM3 = fpMetric?.voxelVolumeM3 ?? plotMetric.voxelVolumeM3 ?? 0;
    const cost = fpMetric?.estimatedVoxelCost ?? plotMetric.estimatedVoxelCost ?? 0;

    titleEl.innerText = `Plot PL-${plotMetric.plotId}`;
    bodyEl.innerText = [
        `Shape Type: ${shapeType}`,
        `Program Type: ${program}`,
        `Floors: ${floors}`,
        `GFA: ${Math.round(gfa).toLocaleString()} m²`,
        `FAR: ${formatMetric(far, 2)}`,
        `Density: ${formatMetric(density * 100, 1)} %`,
        `Height: ${formatMetric(heightM, 1)} m`,
        `Distance To Centre: ${formatMetric(distM, 1)} m`,
        `Plot Area: ${Math.round(areaM2).toLocaleString()} m²`,
        `Token (Voxel Count): ${Math.round(token).toLocaleString()}`,
        `Voxel Volume: ${Math.round(volumeM3).toLocaleString()} m³`,
        `Voxel Cost (Abstract): ${Math.round(cost).toLocaleString()}`
    ].join('\n');
}

function updateCbdSummaryPanel() {
    const cbdEl = document.getElementById('cbdSummaryBody');
    if (!cbdEl) return;
    const totalGfa = building3DGenerator
        ? Math.round(building3DGenerator.calculateTotalGFA(siteGen?.AUTO_SCALE_FACTOR || 0.1))
        : 0;
    cbdEl.innerText = [
        'CBD area: 2400 acres',
        `GFA: ${totalGfa.toLocaleString()} m²`
    ].join('\n');
}

function setRightTab(tabName) {
    activeRightTab = (tabName === 'cbd') ? 'cbd' : 'plot';

    const tabPlotBtn = document.getElementById('tabPlotInspector');
    const tabCbdBtn = document.getElementById('tabCbdSummary');
    const tabPlot = document.getElementById('plotInspectorTab');
    const tabCbd = document.getElementById('cbdSummaryTab');

    const plotActive = activeRightTab === 'plot';
    if (tabPlotBtn) tabPlotBtn.classList.toggle('active', plotActive);
    if (tabCbdBtn) tabCbdBtn.classList.toggle('active', !plotActive);
    if (tabPlot) tabPlot.classList.toggle('active', plotActive);
    if (tabCbd) tabCbd.classList.toggle('active', !plotActive);
}
function applySelectionVisual() {
    if (!threeRenderer) return;
    threeRenderer.setSelectionHighlight({
        footprintId: STATE.selectedFootprintId,
        plotId: STATE.selectedPlotId
    });
    updateInspectorPanel();
    updateCbdSummaryPanel();
}

function renderFullScene() {
    if (!threeRenderer || !siteGen) return;
    rebuildInspectionModel();

    const thematicPlots = STATE.mapColorMode === InspectionColorMode.ZERO
        ? buildDefaultBluePlotColors()
        : (inspectionModel?.mapColorByPlotId || {});
    const thematicBuildings = STATE.buildingColorMode === InspectionColorMode.ZERO
        ? buildDefaultBlueBuildingColors()
        : (inspectionModel?.buildingColorByFootprintId || {});
    const useProgramColoring = STATE.buildingColorMode === BUILDING_COLOR_PROGRAM;

    threeRenderer.renderSiteLayers(siteGen, parcelSubdivider, {
        showSite: true,
        showSubdividedPlots: STATE.showPlots,
        thematicPlotColorsByPlotId: thematicPlots,
        selectedPlotId: STATE.selectedPlotId,
        clientBlueViz: true,
        plotOpacity: STATE.transparent ? 0.35 : 0.88
    });

    if (building3DGenerator) {
        threeRenderer.renderBuildings(building3DGenerator, {
            renderedView: true,
            technicalView: false,
            thematicBuildingColorsByFootprintId: useProgramColoring ? null : thematicBuildings,
            selectedFootprintId: STATE.selectedFootprintId,
            thematicOpacity: STATE.transparent ? 0.46 : 0.95,
            programColoring: useProgramColoring,
            showBuildingsByProgram: useProgramColoring,
            programOpacity: STATE.transparent ? 0.86 : 0.96
        });
    }
    applyClientVisualPreset();
    threeRenderer.setFirstPersonMode(STATE.firstPerson);
    applySelectionVisual();
}

function applySelection(plotId, footprintId = null) {
    if (!inspectionModel || !Number.isFinite(plotId)) {
        STATE.selectedPlotId = null;
        STATE.selectedFootprintId = null;
        applySelectionVisual();
        return;
    }
    STATE.selectedPlotId = plotId;
    if (Number.isFinite(footprintId) && inspectionModel.footprintMetricsById[footprintId]) {
        STATE.selectedFootprintId = footprintId;
    } else {
        const candidates = inspectionModel.footprintsByPlotId[plotId] || [];
        STATE.selectedFootprintId = candidates.length > 0 ? candidates[0] : null;
    }
    applySelectionVisual();
}

function onSceneSelection(selection) {
    if (!selection) {
        STATE.selectedPlotId = null;
        STATE.selectedFootprintId = null;
        applySelectionVisual();
        return;
    }
    const footprintId = Number.isFinite(selection.footprintId) ? selection.footprintId : null;
    let plotId = Number.isFinite(selection.plotId) ? selection.plotId : null;
    if (plotId === null && footprintId !== null && inspectionModel) {
        plotId = inspectionModel.plotIdByFootprintId[footprintId] ?? null;
    }
    applySelection(plotId, footprintId);
}

function findPlotByQuery(rawQuery) {
    if (!inspectionModel || !rawQuery) return null;
    const query = rawQuery.trim().toLowerCase();
    if (!query) return null;

    const exact = Number.parseInt(query, 10);
    if (Number.isFinite(exact) && inspectionModel.plotMetricsById[exact]) return exact;

    const match = query.match(/(\d+)/);
    if (match) {
        const id = Number.parseInt(match[1], 10);
        if (Number.isFinite(id) && inspectionModel.plotMetricsById[id]) return id;
    }
    return null;
}

function bindUI() {
    const mapModeEl = document.getElementById('mapColorMode');
    const buildingModeEl = document.getElementById('buildingColorMode');
    const transparentEl = document.getElementById('toggleTransparency');
    const showPlotsEl = document.getElementById('togglePlots');
    const searchEl = document.getElementById('plotSearchInput');
    const findBtn = document.getElementById('findPlotBtn');
    const zoomBtn = document.getElementById('zoomSelectedBtn');
    const tabPlotBtn = document.getElementById('tabPlotInspector');
    const tabCbdBtn = document.getElementById('tabCbdSummary');

    if (mapModeEl) {
        mapModeEl.value = STATE.mapColorMode;
        mapModeEl.addEventListener('change', (e) => {
            STATE.mapColorMode = e.target.value || InspectionColorMode.ZERO;
            renderFullScene();
        });
    }
    if (buildingModeEl) {
        buildingModeEl.value = STATE.buildingColorMode;
        buildingModeEl.addEventListener('change', (e) => {
            STATE.buildingColorMode = e.target.value || InspectionColorMode.ZERO;
            renderFullScene();
        });
    }
    if (transparentEl) {
        transparentEl.checked = STATE.transparent;
        transparentEl.addEventListener('change', (e) => {
            STATE.transparent = !!e.target.checked;
            renderFullScene();
        });
    }
    if (showPlotsEl) {
        showPlotsEl.checked = STATE.showPlots;
        showPlotsEl.addEventListener('change', (e) => {
            STATE.showPlots = !!e.target.checked;
            renderFullScene();
        });
    }
    const fpEl = document.getElementById('toggleFirstPerson');
    if (fpEl) {
        fpEl.checked = STATE.firstPerson;
        fpEl.addEventListener('change', (e) => {
            STATE.firstPerson = !!e.target.checked;
            threeRenderer?.setFirstPersonMode(STATE.firstPerson);
            setStatus(STATE.firstPerson ? 'First person enabled' : 'Orbit mode enabled');
        });
    }

    window.addEventListener('keydown', (e) => {
        if (e.key !== 'v' && e.key !== 'V') return;
        STATE.firstPerson = !STATE.firstPerson;
        const el = document.getElementById('toggleFirstPerson');
        if (el) el.checked = STATE.firstPerson;
        threeRenderer?.setFirstPersonMode(STATE.firstPerson);
        setStatus(STATE.firstPerson ? 'First person enabled' : 'Orbit mode enabled');
    });

    const runFind = () => {
        const query = searchEl?.value || '';
        const plotId = findPlotByQuery(query);
        if (Number.isFinite(plotId)) {
            applySelection(plotId, null);
            setStatus(`Selected PL-${plotId}`);
        } else {
            setStatus('Plot not found');
        }
    };

    if (searchEl) {
        searchEl.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                runFind();
            }
        });
    }
    if (findBtn) findBtn.addEventListener('click', runFind);

    if (zoomBtn) {
        zoomBtn.addEventListener('click', () => {
            if (!threeRenderer || STATE.selectedPlotId === null) {
                setStatus('Select a plot first');
                return;
            }
            const ok = threeRenderer.focusOnSelection({
                footprintId: STATE.selectedFootprintId,
                plotId: STATE.selectedPlotId
            });
            setStatus(ok ? `Zoomed to PL-${STATE.selectedPlotId}` : 'Unable to zoom to plot');
        });
    }

    if (tabPlotBtn) tabPlotBtn.addEventListener('click', () => setRightTab('plot'));
    if (tabCbdBtn) tabCbdBtn.addEventListener('click', () => setRightTab('cbd'));
    setRightTab(activeRightTab);
}

async function buildScenario() {
    setStatus('Loading site curves...');
    const loaded = await siteGen.importCurvesFromCSV('/Site_CSV/curves.csv');
    if (!loaded) {
        setStatus('Failed to load curves');
        return;
    }
    await loadAttractorsFromCSV('/Site_CSV/Attractors_CSV.csv');
    ensureFallbackAttractors();

    setStatus('Generating parcels and buildings...');
    siteGen.generateParcelsFromBoundaries(PARAMS.largeThresh, PARAMS.mediumThresh);
    parcelAnalyzer = new ParcelAnalyzer(siteGen);
    parcelAnalyzer.analyzeAllParcels();
    roadAnalyzer = new RoadAnalyzer(siteGen);

    parcelSubdivider = new ParcelSubdivider(siteGen, parcelAnalyzer, PARAMS.subdivScale, PARAMS.greenRatio, {
        large: PARAMS.largeTarget,
        medium: PARAMS.mediumTarget,
        small: PARAMS.smallTarget
    });
    parcelSubdivider.subdivideAllParcels();

    footprintGenerator = new BuildingFootprintGenerator(parcelSubdivider, parcelAnalyzer);
    footprintGenerator.generateAllFootprints();

    building3DGenerator = new Building3DGenerator(
        footprintGenerator,
        parcelSubdivider,
        parcelAnalyzer,
        roadAnalyzer,
        PARAMS.heightScale,
        PARAMS.voxelSize
    );
    building3DGenerator.attractorPoints = PARAMS.attractors;
    building3DGenerator.attractorRange = PARAMS.attractorRange;
    building3DGenerator.attractorPeakHeightMeters = PARAMS.attractorPeakHeightM;
    building3DGenerator.attractorLowHeightMeters = PARAMS.attractorLowHeightM;
    building3DGenerator.maxSiteDistance = getSiteSpan();
    building3DGenerator.generateAll3DBuildings();

    renderFullScene();
    setStatus(`Ready - ${building3DGenerator.building3DVoxels.length} voxels`);
}

async function init() {
    siteGen = new SiteGeneration();
    threeRenderer = new ThreeRenderer('clientThreeContainer');
    if (threeRenderer?.renderer) {
        threeRenderer.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.25));
    }
    threeRenderer.setSelectionChangeCallback(onSceneSelection);
    bindUI();
    applyClientVisualPreset();
    await buildScenario();
}

window.addEventListener('DOMContentLoaded', init);

