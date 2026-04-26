import './styles.css';
import { SiteGeneration } from './core/SiteGeneration.js';
import { CanvasRenderer } from './ui/CanvasRenderer.js';
import { ParcelAnalyzer } from './core/ParcelAnalyzer.js';
import { ParcelSubdivider } from './core/ParcelSubdivider.js';
import { BuildingFootprintGenerator } from './core/BuildingFootprintGenerator.js';
import { Building3DGenerator } from './core/Building3DGenerator.js';
import { ThreeRenderer } from './ui/ThreeRenderer.js';
import { RoadAnalyzer } from './core/RoadAnalyzer.js';

let siteGen = null;
let renderer = null;
let threeRenderer = null;
let parcelAnalyzer = null;
let roadAnalyzer = null;
let parcelSubdivider = null;
let footprintGenerator = null;
let building3DGenerator = null;

const STATE = {
  showSite: true,
  showParcels: true,
  showCenters: true, // Internal setting for C++ parity
  showAnalysis: true,
  showSubdividedPlots: false,
  showBuildingFootprints: false,
  show3DBuildings: false,
  showBuildingsByProgram: false,
  showFootprintsByEdge: false,
  showOrientationDebug: false,
  showRoadSegments: false,
  showEdgeConditions: false,
  showCornerConditions: false,
  showRoadFrontages: false,
  showAnalyzedRoads: false,
  showIntersections: false,
  showNetworkPattern: false,
  showAttractors: false,
  showFirstPerson: false,
  showTechnicalView: false,
  showRenderedView: false,
};

const DEFAULT_PARAMS = {
    subdivScale: 1.0,
    greenRatio: 1.0,
    heightScale: 1.0,
    voxelSize: 1.0,
    largeThresh: 95,
    mediumThresh: 36,
    largeTarget: 25,
    mediumTarget: 40,
    smallTarget: 25,
    attractorRange: 130,
    attractorPeakHeightM: 180,
    attractorLowHeightM: 20
};
const PARAMS = {
    ...DEFAULT_PARAMS,
    attractors: []
};

const UNIT_TO_METERS = 10.0;
const AREA_UNIT_TO_M2 = UNIT_TO_METERS * UNIT_TO_METERS;
const RED_ATTRACTOR_PEAK_FACTOR = 1.0;
const BLUE_ATTRACTOR_PEAK_FACTOR = 0.6;

function parseQuotedCSVLine(line) {
  const tokens = [];
  const regex = /"([^"]*)"/g;
  let match;
  while ((match = regex.exec(line)) !== null) {
    tokens.push(match[1]);
  }
  return tokens;
}

async function loadAttractorsFromCSV(url) {
  try {
    const response = await fetch(url);
    if (!response.ok) return false;
    const text = await response.text();
    const lines = text.split(/\r?\n/).filter(l => l.trim().length > 0);
    if (lines.length <= 1) return false;

    const points = [];
    for (let i = 1; i < lines.length; i++) {
      const tokens = parseQuotedCSVLine(lines[i]);
      if (tokens.length < 5) continue;

      const layerName = tokens[1] || '';
      const pointText = tokens[4] || '';
      if (!pointText.includes(',')) continue;

      const coords = pointText.split(',').map(v => parseFloat(v));
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

    // Ensure at least one strongest attractor exists.
    if (!points.some(p => p.isPrimary)) {
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

async function init() {
  document.getElementById('status').innerText = 'Initializing...';
  
  const canvas = document.getElementById('mapCanvas');
  renderer = new CanvasRenderer(canvas);
  threeRenderer = new ThreeRenderer('threeContainer');
  siteGen = new SiteGeneration();
  renderer.setLocationPinCallback((worldPt) => {
    if (!threeRenderer) return;
    if (STATE.showFirstPerson) {
      threeRenderer.jumpFirstPersonTo(worldPt.x, worldPt.y);
      document.getElementById('status').innerText = `Teleported to map pin (${worldPt.x.toFixed(1)}, ${worldPt.y.toFixed(1)})`;
    } else {
      document.getElementById('status').innerText = 'Map pin set. Enable First Person Walk (V) to jump there.';
    }
  });

  setupEventListeners();
  setupSliders();
  
  // Render loop
  function loop() {
    if (renderer && threeRenderer) {
      renderer.setNavigatorPose(threeRenderer.getNavigatorPose());
    }
    renderer.drawSite(siteGen, parcelAnalyzer, roadAnalyzer, parcelSubdivider, footprintGenerator, {
      showCurves: STATE.showSite,
      showParcels: STATE.showParcels,
      showCenters: STATE.showCenters,
      showAnalysis: STATE.showAnalysis,
      showSubdividedPlots: STATE.showSubdividedPlots,
      showBuildingFootprints: STATE.showBuildingFootprints,
      showRoadSegments: STATE.showRoadSegments,
      showEdgeConditions: STATE.showEdgeConditions,
      showCornerConditions: STATE.showCornerConditions,
      showRoadFrontages: STATE.showRoadFrontages,
      showAnalyzedRoads: STATE.showAnalyzedRoads,
      showIntersections: STATE.showIntersections,
      showNetworkPattern: STATE.showNetworkPattern
    });
    requestAnimationFrame(loop);
  }
  
  requestAnimationFrame(loop);
  document.getElementById('status').innerText = 'Ready';
}

function setupEventListeners() {
  // Buttons
  document.querySelectorAll('button[data-action]').forEach(btn => {
    btn.addEventListener('click', async (e) => {
      const action = e.target.getAttribute('data-action');
      handleAction(action);
    });
  });

  // Toggles
  document.querySelectorAll('input[type="checkbox"][data-toggle]').forEach(chk => {
    chk.addEventListener('change', (e) => {
      const toggle = e.target.getAttribute('data-toggle');
      if (toggle === 'site') {
          STATE.showSite = e.target.checked;
          update3DDisplay();
      }
      if (toggle === 'analysis') STATE.showAnalysis = e.target.checked;
      if (toggle === 'plots') {
          STATE.showSubdividedPlots = e.target.checked;
          update3DDisplay();
      }
      if (toggle === 'footprints') STATE.showBuildingFootprints = e.target.checked;
      if (toggle === 'buildings') {
          STATE.show3DBuildings = e.target.checked;
          update3DDisplay();
      }
      if (toggle === 'buildingsByProgram') {
          STATE.showBuildingsByProgram = e.target.checked;
          update3DDisplay();
      }
      if (toggle === 'footprintsByEdge') {
          STATE.showFootprintsByEdge = e.target.checked;
          update3DDisplay();
      }
      if (toggle === 'orientationDebug') {
          STATE.showOrientationDebug = e.target.checked;
          update3DDisplay();
      }
      if (toggle === 'analyzedRoads') STATE.showAnalyzedRoads = e.target.checked;
      if (toggle === 'intersections') STATE.showIntersections = e.target.checked;
      if (toggle === 'networkPattern') STATE.showNetworkPattern = e.target.checked;
      if (toggle === 'attractors') {
          STATE.showAttractors = e.target.checked;
          if (threeRenderer) threeRenderer.toggleAttractorsVisibility(STATE.showAttractors);
      }
      if (toggle === 'firstPerson') {
          STATE.showFirstPerson = e.target.checked;
          if (threeRenderer) threeRenderer.setFirstPersonMode(STATE.showFirstPerson);
      }
      if (toggle === 'technicalView') {
          STATE.showTechnicalView = e.target.checked;
          if (STATE.showTechnicalView) {
              STATE.showRenderedView = false;
              updateCheckbox('renderedView', false);
          }
          if (threeRenderer) threeRenderer.setViewModes({ technical: STATE.showTechnicalView, rendered: STATE.showRenderedView });
          update3DDisplay();
      }
      if (toggle === 'renderedView') {
          STATE.showRenderedView = e.target.checked;
          if (STATE.showRenderedView) {
              STATE.showTechnicalView = false;
              updateCheckbox('technicalView', false);
          }
          if (threeRenderer) threeRenderer.setViewModes({ technical: STATE.showTechnicalView, rendered: STATE.showRenderedView });
          update3DDisplay();
      }
    });
  });
  
  // Keyboard listeners for C++ style interaction
  window.addEventListener('keydown', (e) => {
    switch (e.key) {
      case 'c': handleAction('load'); break;
      case 'b': handleAction('parcels'); break;
      case 'a': handleAction('analyze'); break;
      case 'r': handleAction('analyzeRoads'); break;
      case 'd': handleAction('subdivide'); break;
      case 'f': handleAction('footprints'); break;
      case '#': handleAction('genAll'); break;
      case 'e': handleAction('exportAll'); break;
      case 'E': handleAction('exportSelected'); break;

      case '1':
        STATE.showSite = !STATE.showSite;
        updateCheckbox('site', STATE.showSite);
        update3DDisplay();
        break;
      case '2':
        STATE.showAnalysis = !STATE.showAnalysis;
        updateCheckbox('analysis', STATE.showAnalysis);
        break;
      case '3': STATE.showRoadSegments = !STATE.showRoadSegments; break;
      case '4': 
        STATE.showSubdividedPlots = !STATE.showSubdividedPlots; 
        updateCheckbox('plots', STATE.showSubdividedPlots);
        update3DDisplay();
        break;
      case '5': 
        STATE.showBuildingFootprints = !STATE.showBuildingFootprints;
        updateCheckbox('footprints', STATE.showBuildingFootprints);
        break;
      case '6':
        STATE.show3DBuildings = !STATE.show3DBuildings;
        updateCheckbox('buildings', STATE.show3DBuildings);
        update3DDisplay();
        break;
      case '7':
        STATE.showBuildingsByProgram = !STATE.showBuildingsByProgram;
        updateCheckbox('buildingsByProgram', STATE.showBuildingsByProgram);
        update3DDisplay();
        break;
      case '8':
        STATE.showFootprintsByEdge = !STATE.showFootprintsByEdge;
        updateCheckbox('footprintsByEdge', STATE.showFootprintsByEdge);
        update3DDisplay();
        break;
      case 'o':
      case 'O':
        STATE.showOrientationDebug = !STATE.showOrientationDebug;
        updateCheckbox('orientationDebug', STATE.showOrientationDebug);
        update3DDisplay();
        break;
      case 'g':
      case 'G':
        STATE.showAttractors = !STATE.showAttractors;
        updateCheckbox('attractors', STATE.showAttractors);
        if (threeRenderer) threeRenderer.toggleAttractorsVisibility(STATE.showAttractors);
        break;
      case 'v':
      case 'V':
        STATE.showFirstPerson = !STATE.showFirstPerson;
        updateCheckbox('firstPerson', STATE.showFirstPerson);
        if (threeRenderer) threeRenderer.setFirstPersonMode(STATE.showFirstPerson);
        break;
      case 'm':
      case 'M':
        STATE.showTechnicalView = !STATE.showTechnicalView;
        if (STATE.showTechnicalView) {
          STATE.showRenderedView = false;
          updateCheckbox('renderedView', false);
        }
        updateCheckbox('technicalView', STATE.showTechnicalView);
        if (threeRenderer) threeRenderer.setViewModes({ technical: STATE.showTechnicalView, rendered: STATE.showRenderedView });
        update3DDisplay();
        break;
      case 'n':
      case 'N':
        STATE.showRenderedView = !STATE.showRenderedView;
        if (STATE.showRenderedView) {
          STATE.showTechnicalView = false;
          updateCheckbox('technicalView', false);
        }
        updateCheckbox('renderedView', STATE.showRenderedView);
        if (threeRenderer) threeRenderer.setViewModes({ technical: STATE.showTechnicalView, rendered: STATE.showRenderedView });
        update3DDisplay();
        break;
      case '9': STATE.showCornerConditions = !STATE.showCornerConditions; break;
      case '0': STATE.showRoadFrontages = !STATE.showRoadFrontages; break;
    }
  });
}

function regenerateFromAttractors() {
    if (building3DGenerator) {
        building3DGenerator.attractorPoints = PARAMS.attractors;
        building3DGenerator.attractorRange = PARAMS.attractorRange;
        building3DGenerator.attractorPeakHeightMeters = PARAMS.attractorPeakHeightM;
        building3DGenerator.attractorLowHeightMeters = PARAMS.attractorLowHeightM;
        building3DGenerator.generateAll3DBuildings();
        update3DDisplay();
    }
}

function setupSliders() {
    const bindSlider = (id, paramKey, valId, regenCallback, formatter = null) => {
        const slider = document.getElementById(id);
        const valSpan = document.getElementById(valId);
        if (!slider) return;
        slider.value = String(PARAMS[paramKey]);

        const updateDisplay = (value) => {
            if (!valSpan) return;
            valSpan.innerText = formatter ? formatter(value) : value.toFixed(1);
        };
        updateDisplay(PARAMS[paramKey]);
        
        slider.addEventListener('input', (e) => {
            const val = parseFloat(e.target.value);
            PARAMS[paramKey] = val;
            updateDisplay(val);
            regenCallback();
        });
    };

    const asRealArea = (val) => `${Math.round(val * AREA_UNIT_TO_M2)}`;
    const asRealLength = (val) => `${Math.round(val * UNIT_TO_METERS)}`;
    const asMeters = (val) => `${Math.round(val)}`;

    bindSlider('sliderLargeThresh', 'largeThresh', 'valLargeThresh', regenerateFromParcels, asRealArea);
    bindSlider('sliderMediumThresh', 'mediumThresh', 'valMediumThresh', regenerateFromParcels, asRealArea);
    bindSlider('sliderLargeTarget', 'largeTarget', 'valLargeTarget', regenerateFromSubdivision, asRealArea);
    bindSlider('sliderMediumTarget', 'mediumTarget', 'valMediumTarget', regenerateFromSubdivision, asRealArea);
    bindSlider('sliderSmallTarget', 'smallTarget', 'valSmallTarget', regenerateFromSubdivision, asRealArea);
    bindSlider('sliderAttractorRange', 'attractorRange', 'valAttractorRange', regenerateFromAttractors, asRealLength);
    bindSlider('sliderPeakHeight', 'attractorPeakHeightM', 'valPeakHeight', regenerateFromAttractors, asMeters);
    bindSlider('sliderLowHeight', 'attractorLowHeightM', 'valLowHeight', regenerateFromAttractors, asMeters);
    bindSlider('sliderSubdiv', 'subdivScale', 'valSubdiv', regenerateFromSubdivision);
    bindSlider('sliderGreen', 'greenRatio', 'valGreen', regenerateFromSubdivision);
    bindSlider('sliderHeight', 'heightScale', 'valHeight', regenerateFromSubdivision);
    bindSlider('sliderVoxel', 'voxelSize', 'valVoxel', regenerateFromSubdivision);
}

function regenerateFromParcels() {
    if (siteGen && siteGen.curves.length > 0) {
        siteGen.generateParcelsFromBoundaries(PARAMS.largeThresh, PARAMS.mediumThresh);
        if (parcelAnalyzer) {
            parcelAnalyzer = new ParcelAnalyzer(siteGen);
            parcelAnalyzer.analyzeAllParcels();
        }
        if (roadAnalyzer) {
            roadAnalyzer = new RoadAnalyzer(siteGen);
        }
        regenerateFromSubdivision();
    }
}

function regenerateFromSubdivision() {
    if (parcelAnalyzer && parcelAnalyzer.analyzedPlots.length > 0) {
        parcelSubdivider = new ParcelSubdivider(siteGen, parcelAnalyzer, PARAMS.subdivScale, PARAMS.greenRatio, {
            large: PARAMS.largeTarget,
            medium: PARAMS.mediumTarget,
            small: PARAMS.smallTarget
        });
        parcelSubdivider.subdivideAllParcels();
        
        if (footprintGenerator) {
            footprintGenerator = new BuildingFootprintGenerator(parcelSubdivider, parcelAnalyzer);
            footprintGenerator.generateAllFootprints();
            
            if (building3DGenerator) {
                building3DGenerator = new Building3DGenerator(footprintGenerator, parcelSubdivider, parcelAnalyzer, roadAnalyzer, PARAMS.heightScale, PARAMS.voxelSize);
                building3DGenerator.attractorPoints = PARAMS.attractors;
                building3DGenerator.attractorRange = PARAMS.attractorRange;
                building3DGenerator.attractorPeakHeightMeters = PARAMS.attractorPeakHeightM;
                building3DGenerator.attractorLowHeightMeters = PARAMS.attractorLowHeightM;
                if (siteGen && siteGen.curves.length > 0) {
                    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
                    for (const curve of siteGen.curves) {
                        for (const pt of curve.points) {
                            minX = Math.min(minX, pt.x); maxX = Math.max(maxX, pt.x);
                            minY = Math.min(minY, pt.y); maxY = Math.max(maxY, pt.y);
                        }
                    }
                    building3DGenerator.maxSiteDistance = Math.max(maxX - minX, maxY - minY);
                }
                building3DGenerator.generateAll3DBuildings();
            }
        }
        update3DDisplay();
    }
}

function updateCheckbox(name, checked) {
  const chk = document.querySelector(`input[data-toggle="${name}"]`);
  if (chk) chk.checked = checked;
}

function downloadDataUrl(dataUrl, fileName) {
  if (!dataUrl) return;
  const a = document.createElement('a');
  a.href = dataUrl;
  a.download = fileName;
  a.click();
}

function getRenderOptions() {
  return {
    showCurves: STATE.showSite,
    showParcels: STATE.showParcels,
    showCenters: STATE.showCenters,
    showAnalysis: STATE.showAnalysis,
    showSubdividedPlots: STATE.showSubdividedPlots,
    showBuildingFootprints: STATE.showBuildingFootprints,
    showRoadSegments: STATE.showRoadSegments,
    showEdgeConditions: STATE.showEdgeConditions,
    showCornerConditions: STATE.showCornerConditions,
    showRoadFrontages: STATE.showRoadFrontages,
    showAnalyzedRoads: STATE.showAnalyzedRoads,
    showIntersections: STATE.showIntersections,
    showNetworkPattern: STATE.showNetworkPattern
  };
}

async function handleAction(action) {
  const status = document.getElementById('status');
  switch (action) {
    case 'clearSelection':
      status.innerText = 'Selection clear is not used in technical editor mode';
      break;

    case 'load':
      status.innerText = 'Importing CSV...';
      const success = await siteGen.importCurvesFromCSV('/Site_CSV/curves.csv');
      if (success) {
        await loadAttractorsFromCSV('/Site_CSV/Attractors_CSV.csv');
        status.innerText = 'Curves Loaded';
        renderer.autoFit(siteGen.curves);
        parcelAnalyzer = null;
        update3DDisplay();
      } else {
        status.innerText = 'Failed to load Curves';
      }
      break;

    case 'parcels':
      if (siteGen.curves.length === 0) {
        status.innerText = 'Load curves first!';
        return;
      }
      status.innerText = 'Generating Parcels...';
      siteGen.generateParcelsFromBoundaries(PARAMS.largeThresh, PARAMS.mediumThresh);
      parcelAnalyzer = new ParcelAnalyzer(siteGen);
      status.innerText = `Parcels Generated (${siteGen.parcels.length})`;
      break;
      
    case 'analyze':
      if (!parcelAnalyzer) {
        status.innerText = 'Generate parcels first!';
        return;
      }
      status.innerText = 'Analyzing Parcels...';
      parcelAnalyzer.analyzeAllParcels();
      
      updateCheckbox('site', false); 
      STATE.showSite = false;
      updateCheckbox('analysis', true);
      STATE.showAnalysis = true;
      
      status.innerText = `Analyzed ${parcelAnalyzer.analyzedPlots.length} Plots`;
      break;

    case 'analyzeRoads':
      if (siteGen.curves.length === 0) {
        status.innerText = 'Load curves first!';
        return;
      }
      status.innerText = 'Analyzing Roads...';
      roadAnalyzer = new RoadAnalyzer(siteGen);
      
      updateCheckbox('site', false);
      STATE.showSite = false;
      updateCheckbox('analyzedRoads', true);
      STATE.showAnalyzedRoads = true;
      updateCheckbox('intersections', true);
      STATE.showIntersections = true;
      updateCheckbox('networkPattern', true);
      STATE.showNetworkPattern = true;
      
      status.innerText = `Analyzed ${roadAnalyzer.analyzedSegments.length} Segments & ${roadAnalyzer.intersections.length} Intersections`;
      break;

    case 'subdivide':
      if (!parcelAnalyzer || parcelAnalyzer.analyzedPlots.length === 0) {
        status.innerText = 'Analyze parcels first!';
        return;
      }
      status.innerText = 'Subdividing Plots...';
      parcelSubdivider = new ParcelSubdivider(siteGen, parcelAnalyzer, PARAMS.subdivScale, PARAMS.greenRatio, {
          large: PARAMS.largeTarget,
          medium: PARAMS.mediumTarget,
          small: PARAMS.smallTarget
      });
      parcelSubdivider.subdivideAllParcels();
      
      updateCheckbox('analysis', false);
      STATE.showAnalysis = false;
      updateCheckbox('plots', true);
      STATE.showSubdividedPlots = true;
      update3DDisplay();
      
      status.innerText = `Subdivided into ${parcelSubdivider.allPlots.length} Plots & ${parcelSubdivider.allOpenSpaces.length} Parks`;
      break;

    case 'footprints':
      if (!parcelSubdivider || parcelSubdivider.allPlots.length === 0) {
        status.innerText = 'Subdivide parcels first!';
        return;
      }
      status.innerText = 'Generating Building Footprints...';
      footprintGenerator = new BuildingFootprintGenerator(parcelSubdivider, parcelAnalyzer);
      footprintGenerator.generateAllFootprints();
      
      updateCheckbox('footprints', true);
      STATE.showBuildingFootprints = true;
      
      status.innerText = `Generated ${footprintGenerator.buildingFootprints.length} Building Footprints`;
      break;

    case 'genAll':
      if (!footprintGenerator || footprintGenerator.buildingFootprints.length === 0) {
        status.innerText = 'Generate footprints first!';
        return;
      }
      building3DGenerator = new Building3DGenerator(footprintGenerator, parcelSubdivider, parcelAnalyzer, roadAnalyzer, PARAMS.heightScale, PARAMS.voxelSize);
      
      // Set attractor point to the center of the site's bounds for a default visual effect
      if (siteGen && siteGen.curves.length > 0) {
          let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
          for (const curve of siteGen.curves) {
              for (const pt of curve.points) {
                  minX = Math.min(minX, pt.x); maxX = Math.max(maxX, pt.x);
                  minY = Math.min(minY, pt.y); maxY = Math.max(maxY, pt.y);
              }
          }
          let cx = (minX + maxX) / 2;
          let cy = (minY + maxY) / 2;
          
          if (PARAMS.attractors.length === 0) {
              PARAMS.attractors = [
                  { x: cx, y: cy, isPrimary: true, weight: RED_ATTRACTOR_PEAK_FACTOR },
                  { x: cx + (maxX - cx)*0.5, y: cy + (maxY - cy)*0.5, isPrimary: false, weight: BLUE_ATTRACTOR_PEAK_FACTOR },
                  { x: cx - (cx - minX)*0.5, y: cy + (maxY - cy)*0.5, isPrimary: false, weight: BLUE_ATTRACTOR_PEAK_FACTOR }
              ];
          }
          
          building3DGenerator.attractorPoints = PARAMS.attractors;
          building3DGenerator.attractorRange = PARAMS.attractorRange;
          building3DGenerator.attractorPeakHeightMeters = PARAMS.attractorPeakHeightM;
          building3DGenerator.attractorLowHeightMeters = PARAMS.attractorLowHeightM;
          building3DGenerator.maxSiteDistance = Math.max(maxX - minX, maxY - minY);
          
          if (threeRenderer) {
              threeRenderer.setupAttractors(PARAMS.attractors, (newPoints) => {
                  PARAMS.attractors = newPoints;
                  regenerateFromAttractors();
              });
              threeRenderer.toggleAttractorsVisibility(STATE.showAttractors);
          }
      }
      
      building3DGenerator.generateAll3DBuildings();
      
      STATE.show3DBuildings = true;
      updateCheckbox('buildings', true);
      update3DDisplay();
      
      status.innerText = `Generated ${building3DGenerator.building3DVoxels.length} 3D Voxels`;
      break;

    case 'genSelected':
      status.innerText = `Action ${action} not implemented in current stage`;
      break;

    case 'exportAll':
    case 'exportSelected':
      if (!threeRenderer) return;
      const objData = threeRenderer.exportToOBJ();
      if (objData) {
        const blob = new Blob([objData], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `urban_site_${Date.now()}.obj`;
        a.click();
        URL.revokeObjectURL(url);
        status.innerText = 'Exported to OBJ';
      }
      break;

    case 'export2dPngHQ': {
      if (!renderer) return;
      const png = renderer.exportHighResPNG(
        siteGen,
        parcelAnalyzer,
        roadAnalyzer,
        parcelSubdivider,
        footprintGenerator,
        getRenderOptions(),
        4
      );
      if (png) {
        downloadDataUrl(png, `map_2d_hq_${Date.now()}.png`);
        status.innerText = 'Exported HQ 2D PNG';
      }
      break;
    }

    case 'export3dPngHQ': {
      if (!threeRenderer) return;
      const png = threeRenderer.exportHighResPNG(4);
      if (png) {
        downloadDataUrl(png, `viewer_3d_hq_${Date.now()}.png`);
        status.innerText = 'Exported HQ 3D PNG';
      }
      break;
    }

    case 'exportBothPngHQ': {
      if (!renderer || !threeRenderer) return;
      const ts = Date.now();
      const mapPng = renderer.exportHighResPNG(
        siteGen,
        parcelAnalyzer,
        roadAnalyzer,
        parcelSubdivider,
        footprintGenerator,
        getRenderOptions(),
        4
      );
      const viewPng = threeRenderer.exportHighResPNG(4);
      if (mapPng) downloadDataUrl(mapPng, `map_2d_hq_${ts}.png`);
      if (viewPng) downloadDataUrl(viewPng, `viewer_3d_hq_${ts}.png`);
      status.innerText = 'Exported HQ 2D + 3D PNGs';
      break;
    }
  }
}

function update3DDisplay() {
    if (!threeRenderer) return;
    threeRenderer.setViewModes({ technical: STATE.showTechnicalView, rendered: STATE.showRenderedView });
    
    // Always render site layers in 3D if they are enabled
    if (siteGen) {
        threeRenderer.renderSiteLayers(siteGen, parcelSubdivider, {
            showSite: STATE.showSite,
            showSubdividedPlots: STATE.showSubdividedPlots
        });
    }

    if (building3DGenerator && STATE.show3DBuildings) {
        threeRenderer.renderBuildings(building3DGenerator, {
            showBuildingsByProgram: STATE.showBuildingsByProgram,
            showFootprintsByEdge: STATE.showFootprintsByEdge,
            showOrientationDebug: STATE.showOrientationDebug,
            technicalView: STATE.showTechnicalView,
            renderedView: STATE.showRenderedView
        });
        threeRenderer.setFirstPersonMode(STATE.showFirstPerson);
    } else {
        threeRenderer.clear();
        threeRenderer.setFirstPersonMode(false);
    }
    
    updateStatistics();
}

function updateStatistics() {
    const gfaElement = document.getElementById('totalGFA');
    if (!gfaElement) return;

    if (building3DGenerator && STATE.show3DBuildings) {
        const gfa = building3DGenerator.calculateTotalGFA(siteGen.AUTO_SCALE_FACTOR);
        gfaElement.innerText = Math.round(gfa).toLocaleString();
    } else {
        gfaElement.innerText = '0';
    }
}

// Start application
window.addEventListener('DOMContentLoaded', init);

