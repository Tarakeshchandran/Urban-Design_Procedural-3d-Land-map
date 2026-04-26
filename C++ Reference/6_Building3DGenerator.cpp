#define _MAIN_
#ifdef _MAIN_

#include "main.h"
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"
#include "ParcelSubdivider.h"
#include "BuildingFootprintGenerator.h"
#include "Building3DGenerator.h"

// zSpace Core Headers
#include <headers/zApp/include/zObjects.h>
#include <headers/zApp/include/zFnSets.h>
#include <headers/zApp/include/zViewer.h>

using namespace zSpace;
using namespace std;

// Conversion helpers
Alice::vec zVecToAliceVec(zVector& in) {
    return Alice::vec(in.x, in.y, in.z);
}

zVector AliceVecToZvec(Alice::vec& in) {
    return zVector(in.x, in.y, in.z);
}

// ==================== GLOBAL VARIABLES ====================
SiteGeneration* siteGen = nullptr;
ParcelAnalyzer* parcelAnalyzer = nullptr;
RoadAnalyzer* roadAnalyzer = nullptr;
ParcelSubdivider* parcelSubdivider = nullptr;
BuildingFootprintGenerator* footprintGenerator = nullptr;
Building3DGenerator* building3DGenerator = nullptr;

// Visualization toggles
bool showSiteData = true;
bool showParcelAnalysis = false;
bool showRoadAnalysis = false;
bool showSubdividedPlots = false;
bool showBuildingFootprints = true;
bool show3DBuildings = true;
bool colorFootprintsByEdgeType = false;
bool show3DBuildingsByProgram = false;

// Selection system
vector<int> selectedParcelIds;
vector<int> selectedPlotIds;
int hoveredParcelId = -1;
bool selectionMode = true;

// Mouse interaction
int lastMouseX = 0;
int lastMouseY = 0;
bool mousePressed = false;

// Additional settings
bool verboseLogging = false;
bool autoGenerate3D = false;

// ==================== FUNCTION PROTOTYPES ====================
void initializeAllSystems();
void analyzeAndSubdivideParcels();
void generateBuildingFootprints();
void generate3DBuildingsForSelected();
void generate3DBuildingsForAll();
void drawVisualization();
void handleMouseSelection(int x, int y);
int findParcelAtScreenPoint(int x, int y);
Point2D screenToWorld(int screenX, int screenY);
void highlightSelectedParcels();
void drawSelectionInfo();
void exportBuildingsAsOBJ(const string& filename);

// ==================== ALICE FRAMEWORK FUNCTIONS ====================

void setup() {
    cout << "=== 3D Urban Building Generation System (No Camera Controls) ===" << endl;
    cout << "Initializing all urban generation systems with building orientation..." << endl;

    siteGen = new SiteGeneration();
    initializeAllSystems();

    cout << "\n*** COMPLETE URBAN GENERATION WORKFLOW ***" << endl;
    cout << "=== SETUP PHASE ===" << endl;
    cout << "1. Press 'g' to generate template CSV files" << endl;
    cout << "2. Edit curves.csv with your site data (roads, water, parks, parcels)" << endl;
    cout << "3. Press 'c' to import curves from CSV" << endl;
    cout << "4. Press 'b' to generate parcels from boundaries" << endl;

    cout << "\n=== ANALYSIS PHASE ===" << endl;
    cout << "5. Press 'a' to analyze parcels and roads" << endl;
    cout << "6. Press 'd' to subdivide parcels into plots" << endl;
    cout << "7. Press 'f' to generate 2D building footprints" << endl;

    cout << "\n=== 3D GENERATION PHASE ===" << endl;
    cout << "8. Click on parcels to SELECT them (green highlight)" << endl;
    cout << "9. Press '3' to generate 3D buildings for SELECTED parcels only" << endl;
    cout << "10. Press 'SHIFT+3' to generate 3D buildings for ALL parcels" << endl;

    cout << "\n*** VISUALIZATION CONTROLS ***" << endl;
    cout << "- Press '1' to toggle site data (roads, water, parks)" << endl;
    cout << "- Press '2' to toggle parcel analysis visualization" << endl;
    cout << "- Press '4' to toggle subdivided plots visualization" << endl;
    cout << "- Press '5' to toggle 2D building footprints (shows blue orientation arrows)" << endl;
    cout << "- Press '6' to toggle 3D buildings" << endl;
    cout << "- Press '7' to toggle 3D buildings colored by program type" << endl;
    cout << "- Press '8' to toggle footprint coloring by edge type" << endl;
    cout << "- Press 'h' to toggle height gradient on/off" << endl;
    cout << "- Press 'p' to cycle height gradient intensity (0.5x to 2.0x)" << endl;

    cout << "\n*** ORIENTATION CONTROLS ***" << endl;
    cout << "- Press 'o' to toggle building orientation to footprint direction" << endl;
    cout << "- Press 'O' (SHIFT+O) to toggle orientation debug visualization (red arrows)" << endl;
    cout << "- Building voxels will align with the blue arrows shown on footprints" << endl;

    cout << "\n*** EXPORT CONTROLS ***" << endl;
    cout << "- Press 'e' to export 3D buildings as OBJ file (buildings.obj)" << endl;
    cout << "- Press 'E' (SHIFT+E) to export selected buildings only as OBJ file" << endl;

    cout << "\n*** SELECTION CONTROLS ***" << endl;
    cout << "- LEFT CLICK to select/deselect individual parcels" << endl;
    cout << "- Press 'x' to clear all selections" << endl;
    cout << "- Press 'm' to toggle selection mode" << endl;

    cout << "\n*** UTILITY CONTROLS ***" << endl;
    cout << "- Press 's' to show comprehensive statistics" << endl;
    cout << "- Press 'v' to toggle verbose logging" << endl;
    cout << "- Press 'z' to reset entire system" << endl;

    cout << "\n*** 3D BUILDING FEATURES (WITH ORIENTATION ALIGNMENT) ***" << endl;
    cout << "- VISIT (Purple): Retail/Commercial spaces at ground level" << endl;
    cout << "- WORK (Blue): Office spaces in middle floors" << endl;
    cout << "- LIVE (Yellow): Residential spaces in upper floors" << endl;
    cout << "- TRANSITION (Gray): Services, amenities, circulation" << endl;
    cout << "- Buildings are aligned with their 2D footprints AND oriented to road directions" << endl;
    cout << "- Fixed coordinate system: X,Y for footprint position, Z for height" << endl;
    cout << "- HEIGHT GRADIENT: Buildings get taller from Small -> Medium -> Large parcels" << endl;
    cout << "- ORIENTATION: Buildings rotate to align with primary road or footprint direction" << endl;
    cout << "- Buildings can be exported as OBJ files for use in other 3D software" << endl;

    cout << "\nSystem ready! Generate buildings and export them as OBJ files!" << endl;
}

bool compute = false;

void update(int value) {
    // Update logic can be added here
}

void draw() {
    // Clear the background
    backGround(0.95);
    drawGrid(5); // 2D grid

    // Draw all visualization elements
    drawVisualization();

    // Highlight selected parcels
    highlightSelectedParcels();

    // Draw UI information
    drawSelectionInfo();

    // Draw help text
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos3f(-4, 3.5, 0);
    string helpText = "3D Urban Generator - Click parcels, press '3' for 3D buildings, 'e' to export OBJ";
    for (char c : helpText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Show current status
    glRasterPos3f(-4, 3.0, 0);
    string statusText = "";
    if (siteGen) {
        statusText += "Parcels: " + to_string(siteGen->getParcels().size());
    }
    if (parcelSubdivider) {
        statusText += ", Plots: " + to_string(parcelSubdivider->getAllPlots().size());
    }
    if (footprintGenerator) {
        statusText += ", Footprints: " + to_string(footprintGenerator->getAllFootprints().size());
    }
    if (building3DGenerator) {
        statusText += ", 3D Voxels: " + to_string(building3DGenerator->getAll3DVoxels().size());
    }
    for (char c : statusText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }

    // Show selection info
    glRasterPos3f(-4, 2.5, 0);
    string selectionText = "Selected Parcels: " + to_string(selectedParcelIds.size());
    if (hoveredParcelId >= 0) {
        selectionText += " | Hovered: " + to_string(hoveredParcelId);
    }
    for (char c : selectionText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }

    // Show orientation status
    glRasterPos3f(-4, 2.0, 0);
    string orientationText = "Building System: ORIENTED + HEIGHT GRADIENT - Buildings align to footprint directions";
    if (building3DGenerator) {
        orientationText += " [Press 'o' to toggle, 'O' for debug, 'e' to export OBJ]";
    }
    for (char c : orientationText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }
}

void keyPress(unsigned char k, int xm, int ym) {
    if (!siteGen) return;

    switch (k) {
        // Setup phase
    case 'g':
        cout << "Generating template CSV files..." << endl;
        siteGen->generateExampleFiles();
        cout << "Template files created! Edit them with your site data." << endl;
        break;

    case 'c':
        cout << "Importing curves from CSV..." << endl;
        if (siteGen->importCurvesFromCSV("curves.csv")) {
            cout << "Curves imported successfully!" << endl;
            initializeAllSystems();
        }
        else {
            cout << "Failed to import curves. Make sure 'curves.csv' exists." << endl;
        }
        break;

    case 'b':
        cout << "Generating parcels from imported boundaries..." << endl;
        siteGen->generateParcelsFromBoundaries();
        initializeAllSystems();
        break;

        // Analysis phase
    case 'a':
        cout << "Analyzing parcels and road network..." << endl;
        analyzeAndSubdivideParcels();
        break;

    case 'd':
        cout << "Subdividing parcels based on analysis..." << endl;
        if (parcelSubdivider && parcelAnalyzer && roadAnalyzer) {
            if (parcelAnalyzer->getAnalyzedPlots().size() == 0) {
                cout << "Please run analysis ('a') before subdivision." << endl;
            }
            else {
                parcelSubdivider->subdivideAllParcels();
                showSubdividedPlots = true;
            }
        }
        else {
            cout << "Error: Analysis components not initialized." << endl;
        }
        break;

    case 'f':
        cout << "Generating 2D building footprints..." << endl;
        generateBuildingFootprints();
        break;

        // 3D Generation phase
    case '3':
        if (selectedParcelIds.empty()) {
            cout << "No parcels selected! Click on parcels first, then press '3'." << endl;
        }
        else {
            cout << "Generating oriented 3D buildings for " << selectedParcelIds.size() << " selected parcels..." << endl;
            generate3DBuildingsForSelected();
        }
        break;

    case '#': // SHIFT+3
        cout << "Generating oriented 3D buildings for ALL parcels..." << endl;
        generate3DBuildingsForAll();
        break;

        // Export controls
    case 'e':
        cout << "Exporting all 3D buildings as OBJ file..." << endl;
        exportBuildingsAsOBJ("buildings_all.obj");
        break;

    case 'E': // SHIFT+E
        if (selectedParcelIds.empty()) {
            cout << "No parcels selected! Select parcels first, then press SHIFT+E." << endl;
        }
        else {
            cout << "Exporting selected 3D buildings as OBJ file..." << endl;
            exportBuildingsAsOBJ("buildings_selected.obj");
        }
        break;

        // Orientation controls
    case 'o':
        if (building3DGenerator) {
            static bool orientationEnabled = true;
            orientationEnabled = !orientationEnabled;
            building3DGenerator->setVoxelOrientation(orientationEnabled);
            cout << "Building orientation to footprint: " << (orientationEnabled ? "ENABLED" : "DISABLED") << endl;
            cout << "Regenerate 3D buildings to see the change." << endl;
        }
        break;

    case 'O': // SHIFT+O
        if (building3DGenerator) {
            static bool debugEnabled = false;
            debugEnabled = !debugEnabled;
            building3DGenerator->setOrientationDebug(debugEnabled);
            cout << "Orientation debug visualization: " << (debugEnabled ? "ON" : "OFF") << endl;
            cout << "Red arrows will show building orientations when enabled." << endl;
        }
        break;

        // Statistics and system controls  
    case 's':
        cout << "\n=== COMPREHENSIVE ORIENTED SYSTEM STATISTICS ===" << endl;
        if (siteGen) siteGen->printStatistics();
        if (parcelAnalyzer) parcelAnalyzer->printAnalysisStatistics();
        if (parcelSubdivider) parcelSubdivider->printSubdivisionStatistics();
        if (footprintGenerator) footprintGenerator->printFootprintStatistics();
        if (building3DGenerator) building3DGenerator->print3DStatistics();
        break;

    case 'z':
        cout << "Resetting entire oriented system..." << endl;
        selectedParcelIds.clear();
        selectedPlotIds.clear();
        hoveredParcelId = -1;
        delete building3DGenerator;
        delete footprintGenerator;
        delete parcelSubdivider;
        delete roadAnalyzer;
        delete parcelAnalyzer;
        delete siteGen;
        siteGen = new SiteGeneration();
        initializeAllSystems();
        break;

    case 'v':
        verboseLogging = !verboseLogging;
        cout << "Verbose logging: " << (verboseLogging ? "ON" : "OFF") << endl;
        if (footprintGenerator) footprintGenerator->setVerboseLogging(verboseLogging);
        if (building3DGenerator) building3DGenerator->setVerboseLogging(verboseLogging);
        break;

        // Visualization toggles
    case '1':
        showSiteData = !showSiteData;
        cout << "Site data: " << (showSiteData ? "ON" : "OFF") << endl;
        break;

    case '2':
        showParcelAnalysis = !showParcelAnalysis;
        cout << "Parcel analysis: " << (showParcelAnalysis ? "ON" : "OFF") << endl;
        break;

    case '4':
        showSubdividedPlots = !showSubdividedPlots;
        cout << "Subdivided plots: " << (showSubdividedPlots ? "ON" : "OFF") << endl;
        break;

    case '5':
        showBuildingFootprints = !showBuildingFootprints;
        cout << "Building footprints (with blue orientation arrows): " << (showBuildingFootprints ? "ON" : "OFF") << endl;
        break;

    case '6':
        show3DBuildings = !show3DBuildings;
        cout << "3D buildings: " << (show3DBuildings ? "ON" : "OFF") << endl;
        break;

    case '7':
        show3DBuildingsByProgram = !show3DBuildingsByProgram;
        cout << "3D buildings by program: " << (show3DBuildingsByProgram ? "ON" : "OFF") << endl;
        break;

    case '8':
        colorFootprintsByEdgeType = !colorFootprintsByEdgeType;
        cout << "Color footprints by edge type: " << (colorFootprintsByEdgeType ? "ON" : "OFF") << endl;
        break;

    case 'h':
    case 'H':
        if (building3DGenerator) {
            // Toggle height gradient
            static bool gradientEnabled = true;
            gradientEnabled = !gradientEnabled;
            building3DGenerator->setHeightGradient(gradientEnabled, 100.0f);
            cout << "Height gradient toggled. Regenerate buildings to see effect." << endl;
        }
        break;

    case 'p':
    case 'P':
        if (building3DGenerator) {
            // Cycle through gradient intensity levels
            static float intensityLevels[] = { 0.5f, 1.0f, 5.0f, 10.0f };
            static int currentLevel = 1; // Start at 1.0f
            currentLevel = (currentLevel + 1) % 4;
            building3DGenerator->setHeightGradient(true, intensityLevels[currentLevel]);
            cout << "Height gradient intensity set to " << intensityLevels[currentLevel]
                << ". Regenerate buildings to see effect." << endl;
        }
        break;

    case 'x':
    case 'X':
        selectedParcelIds.clear();
        selectedPlotIds.clear();
        hoveredParcelId = -1;
        cout << "Cleared all selections." << endl;
        break;

    case 'm':
    case 'M':
        selectionMode = !selectionMode;
        cout << "Selection mode: " << (selectionMode ? "ON" : "OFF") << endl;
        break;
    }
}

void mousePress(int b, int state, int x, int y) {
    lastMouseX = x;
    lastMouseY = y;

    if (b == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mousePressed = true;
            if (selectionMode) {
                handleMouseSelection(x, y);
            }
        }
        else {
            mousePressed = false;
        }
    }
}

void mouseMotion(int x, int y) {
    if (selectionMode) {
        // Hover detection for 2D mode
        int hoveredId = findParcelAtScreenPoint(x, y);
        if (hoveredId != hoveredParcelId) {
            hoveredParcelId = hoveredId;
        }
    }
}

// ==================== SYSTEM INITIALIZATION ====================

void initializeAllSystems() {
    // Clean up any existing systems
    if (building3DGenerator) delete building3DGenerator;
    if (footprintGenerator) delete footprintGenerator;
    if (parcelSubdivider) delete parcelSubdivider;
    if (roadAnalyzer) delete roadAnalyzer;
    if (parcelAnalyzer) delete parcelAnalyzer;

    // Create new systems in dependency order
    parcelAnalyzer = new ParcelAnalyzer(siteGen);
    roadAnalyzer = new RoadAnalyzer(siteGen);
    parcelSubdivider = new ParcelSubdivider(siteGen, parcelAnalyzer, roadAnalyzer);
    footprintGenerator = new BuildingFootprintGenerator(parcelSubdivider, parcelAnalyzer, roadAnalyzer);
    building3DGenerator = new Building3DGenerator(footprintGenerator, parcelSubdivider, parcelAnalyzer, roadAnalyzer);

    // Set verbosity
    if (footprintGenerator) footprintGenerator->setVerboseLogging(verboseLogging);
    if (building3DGenerator) building3DGenerator->setVerboseLogging(verboseLogging);

    cout << "All urban generation systems initialized with orientation support." << endl;
}

void analyzeAndSubdivideParcels() {
    if (!siteGen || siteGen->getParcels().empty()) {
        cout << "Error: No parcels available. Please load site data and generate parcels first." << endl;
        return;
    }

    // Analyze parcels
    if (parcelAnalyzer) {
        cout << "Analyzing parcels..." << endl;
        parcelAnalyzer->analyzeAllParcels();
    }

    // Analyze road network
    if (roadAnalyzer) {
        cout << "Analyzing road network..." << endl;
        roadAnalyzer->analyzeNetwork();
    }

    cout << "Analysis complete! Use 'd' to subdivide parcels." << endl;
}

void generateBuildingFootprints() {
    if (!footprintGenerator) {
        cout << "Error: BuildingFootprintGenerator not initialized." << endl;
        return;
    }

    if (!parcelSubdivider || parcelSubdivider->getAllPlots().empty()) {
        cout << "Error: No plots available. Please run subdivision ('d') first." << endl;
        return;
    }

    cout << "Generating 2D building footprints with orientation vectors..." << endl;
    footprintGenerator->generateAllFootprints();
    showBuildingFootprints = true;
    cout << "Footprints generated! Press '5' to see blue orientation arrows." << endl;
}

void generate3DBuildingsForSelected() {
    if (!building3DGenerator) {
        cout << "Error: Building3DGenerator not initialized." << endl;
        return;
    }

    if (!footprintGenerator || footprintGenerator->getAllFootprints().empty()) {
        cout << "Error: No footprints available. Please run footprint generation ('f') first." << endl;
        return;
    }

    if (selectedParcelIds.empty()) {
        cout << "Error: No parcels selected. Click on parcels first." << endl;
        return;
    }

    // Clear existing 3D buildings
    building3DGenerator->clear3DBuildings();

    // Generate 3D buildings only for footprints from selected parcels
    const auto& allFootprints = footprintGenerator->getAllFootprints();
    const auto& allPlots = parcelSubdivider->getAllPlots();

    int generatedCount = 0;
    vector<Voxel3D> selectedBuildings;

    for (const auto& footprint : allFootprints) {
        // Find the plot for this footprint
        const Plot* parentPlot = nullptr;
        for (const auto& plot : allPlots) {
            if (plot.id == footprint.parentPlotId) {
                parentPlot = &plot;
                break;
            }
        }

        if (parentPlot) {
            // Check if this plot's parent parcel is selected
            bool isSelected = false;
            for (int selectedId : selectedParcelIds) {
                if (parentPlot->parentParcelId == selectedId) {
                    isSelected = true;
                    break;
                }
            }

            if (isSelected) {
                // Generate 3D building for this footprint with proper orientation
                vector<Voxel3D> buildingVoxels = building3DGenerator->generate3DBuildingFromFootprint(footprint);

                // Add to our selected buildings collection
                for (auto& voxel : buildingVoxels) {
                    selectedBuildings.push_back(voxel);
                }

                generatedCount++;

                if (verboseLogging) {
                    cout << "Generated oriented 3D building with " << buildingVoxels.size()
                        << " voxels for footprint " << footprint.id
                        << " (parcel " << parentPlot->parentParcelId << ")" << endl;
                }
            }
        }
    }

    show3DBuildings = true;
    cout << "Generated oriented 3D buildings for " << generatedCount << " footprints from "
        << selectedParcelIds.size() << " selected parcels with height gradient and orientation!" << endl;
    cout << "Total 3D voxels created: " << selectedBuildings.size() << endl;
    cout << "Buildings are oriented to match their footprint directions (blue arrows)!" << endl;
    cout << "Press 'e' to export as OBJ file!" << endl;
}

void generate3DBuildingsForAll() {
    if (!building3DGenerator) {
        cout << "Error: Building3DGenerator not initialized." << endl;
        return;
    }

    if (!footprintGenerator || footprintGenerator->getAllFootprints().empty()) {
        cout << "Error: No footprints available. Please run footprint generation ('f') first." << endl;
        return;
    }

    cout << "Generating oriented 3D buildings for ALL parcels..." << endl;
    building3DGenerator->generateAll3DBuildings();
    show3DBuildings = true;

    cout << "3D buildings generated with orientation alignment and height gradient!" << endl;
    cout << "Press 'e' to export as OBJ file!" << endl;
}

// ==================== OBJ EXPORT FUNCTIONALITY ====================

void exportBuildingsAsOBJ(const string& filename) {
    if (!building3DGenerator) {
        cout << "Error: No 3D buildings to export. Generate 3D buildings first." << endl;
        return;
    }

    const auto& voxels = building3DGenerator->getAll3DVoxels();

    if (voxels.empty()) {
        cout << "Error: No 3D buildings available to export. Generate buildings first." << endl;
        return;
    }

    cout << "Exporting " << voxels.size() << " voxels to " << filename << "..." << endl;

    ofstream objFile(filename);
    if (!objFile.is_open()) {
        cout << "Error: Could not create OBJ file: " << filename << endl;
        return;
    }

    // Write OBJ header
    objFile << "# 3D Urban Buildings Export" << endl;
    objFile << "# Generated by Urban Building Generation System" << endl;
    objFile << "# Scale: 1 unit = 10 meters" << endl;
    objFile << "# Coordinate system: X,Y for footprint, Z for height" << endl;
    objFile << endl;

    int vertexIndex = 1; // OBJ uses 1-based indexing
    int objectCount = 0;

    // Export selected buildings only if we have selections and this is a selected export
    bool exportSelectedOnly = (filename.find("selected") != string::npos && !selectedParcelIds.empty());

    for (const auto& voxel : voxels) {
        // If exporting selected only, check if this voxel belongs to a selected parcel
        if (exportSelectedOnly) {
            // Find the plot for this voxel
            const auto& allPlots = parcelSubdivider->getAllPlots();
            bool isFromSelectedParcel = false;

            for (const auto& plot : allPlots) {
                if (plot.id == voxel.parentFootprintId) {
                    // Check if this plot's parent parcel is selected
                    for (int selectedId : selectedParcelIds) {
                        if (plot.parentParcelId == selectedId) {
                            isFromSelectedParcel = true;
                            break;
                        }
                    }
                    break;
                }
            }

            if (!isFromSelectedParcel) {
                continue; // Skip this voxel
            }
        }

        // Create object name based on voxel properties
        string objectName = "Building_" + to_string(objectCount++) + "_" +
            building3DGenerator->getVoxelTypeName(voxel.type) +
            "_Level" + to_string(voxel.level);

        objFile << "o " << objectName << endl;
        objFile << "# Voxel ID: " << voxel.id << ", Type: " << building3DGenerator->getVoxelTypeName(voxel.type) << endl;
        objFile << "# Position: (" << voxel.position.x << ", " << voxel.position.y << ", " << voxel.position.z << ")" << endl;
        objFile << "# Dimensions: " << voxel.dimensions.x << " x " << voxel.dimensions.y << " x " << voxel.dimensions.z << endl;

        // Handle rotation if voxel has orientation
        bool hasRotation = (voxel.orientation.x != 1.0f || voxel.orientation.y != 0.0f);

        if (hasRotation) {
            objFile << "# Orientation: (" << voxel.orientation.x << ", " << voxel.orientation.y << "), Angle: "
                << (voxel.rotationAngle * 180.0f / M_PI) << " degrees" << endl;
        }

        objFile << endl;

        // Calculate the 8 vertices of the box (with potential rotation)
        vector<Point3D> vertices(8);

        // Original box corners (before rotation)
        float x = voxel.position.x;
        float y = voxel.position.y;
        float z = voxel.position.z;
        float w = voxel.dimensions.x;
        float d = voxel.dimensions.y;
        float h = voxel.dimensions.z;

        // Define box vertices relative to center for rotation
        Point3D center = voxel.getCenter();

        // Box corners relative to center
        vector<Point3D> localVertices = {
            {-w / 2, -d / 2, -h / 2}, // 0: bottom-left-back
            { w / 2, -d / 2, -h / 2}, // 1: bottom-right-back
            { w / 2,  d / 2, -h / 2}, // 2: bottom-right-front
            {-w / 2,  d / 2, -h / 2}, // 3: bottom-left-front
            {-w / 2, -d / 2,  h / 2}, // 4: top-left-back
            { w / 2, -d / 2,  h / 2}, // 5: top-right-back
            { w / 2,  d / 2,  h / 2}, // 6: top-right-front
            {-w / 2,  d / 2,  h / 2}  // 7: top-left-front
        };

        // Apply rotation if needed and transform to world coordinates
        for (int i = 0; i < 8; i++) {
            Point3D vertex = localVertices[i];

            if (hasRotation) {
                // Rotate around Z axis (2D rotation in XY plane)
                float cosA = cos(voxel.rotationAngle);
                float sinA = sin(voxel.rotationAngle);

                float rotatedX = vertex.x * cosA - vertex.y * sinA;
                float rotatedY = vertex.x * sinA + vertex.y * cosA;

                vertex.x = rotatedX;
                vertex.y = rotatedY;
                // Z coordinate remains unchanged
            }

            // Transform to world coordinates
            vertices[i] = Point3D(center.x + vertex.x, center.y + vertex.y, center.z + vertex.z);
        }

        // Write vertices to OBJ file
        for (const auto& vertex : vertices) {
            objFile << "v " << vertex.x << " " << vertex.y << " " << vertex.z << endl;
        }

        // Write faces (using current vertex indices)
        // Bottom face (Z = z)
        objFile << "f " << vertexIndex + 0 << " " << vertexIndex + 1 << " " << vertexIndex + 2 << " " << vertexIndex + 3 << endl;

        // Top face (Z = z + h)
        objFile << "f " << vertexIndex + 4 << " " << vertexIndex + 7 << " " << vertexIndex + 6 << " " << vertexIndex + 5 << endl;

        // Front face (Y = y + d)
        objFile << "f " << vertexIndex + 3 << " " << vertexIndex + 2 << " " << vertexIndex + 6 << " " << vertexIndex + 7 << endl;

        // Back face (Y = y)
        objFile << "f " << vertexIndex + 0 << " " << vertexIndex + 4 << " " << vertexIndex + 5 << " " << vertexIndex + 1 << endl;

        // Left face (X = x)
        objFile << "f " << vertexIndex + 0 << " " << vertexIndex + 3 << " " << vertexIndex + 7 << " " << vertexIndex + 4 << endl;

        // Right face (X = x + w)
        objFile << "f " << vertexIndex + 1 << " " << vertexIndex + 5 << " " << vertexIndex + 6 << " " << vertexIndex + 2 << endl;

        objFile << endl;

        vertexIndex += 8; // Move to next set of vertices
    }

    objFile.close();

    cout << "Successfully exported " << objectCount << " building objects to " << filename << endl;
    cout << "File contains " << (vertexIndex - 1) << " vertices and " << (objectCount * 6) << " faces" << endl;
    cout << "Coordinate system: X,Y for footprint position, Z for height above ground" << endl;
    cout << "Scale: 1 unit in OBJ = 10 meters in real world" << endl;

    if (exportSelectedOnly) {
        cout << "Exported buildings from " << selectedParcelIds.size() << " selected parcels only" << endl;
    }
    else {
        cout << "Exported all generated buildings" << endl;
    }
}

// ==================== VISUALIZATION ====================

void drawVisualization() {
    // Layer 1: Basic site data (roads, water, parks, parcels) - drawn at Z=0
    if (siteGen && showSiteData) {
        siteGen->drawSite();
    }

    // Layer 2: Parcel analysis (categories, edge conditions, corner conditions) - drawn at Z=0.01
    if (parcelAnalyzer && showParcelAnalysis) {
        parcelAnalyzer->drawAnalysisResults();
        parcelAnalyzer->drawEdgeConditions();
        parcelAnalyzer->drawCornerConditions();
        parcelAnalyzer->drawRoadFrontages();
    }

    // Layer 3: Road analysis (classifications, directions, intersections) - drawn at Z=0.02
    if (roadAnalyzer && showRoadAnalysis) {
        roadAnalyzer->drawAnalyzedRoads();
        roadAnalyzer->drawIntersections();
        roadAnalyzer->drawNetworkPattern();
    }

    // Layer 4: Subdivided plots (with open spaces) - drawn at Z=0.03
    if (parcelSubdivider && showSubdividedPlots) {
        parcelSubdivider->drawSubdividedPlots();
    }

    // Layer 5: 2D Building footprints (with blue orientation arrows) - drawn at Z=0.04-0.05
    if (footprintGenerator && showBuildingFootprints) {
        if (colorFootprintsByEdgeType) {
            footprintGenerator->drawFootprintsByEdgeType();
        }
        else {
            footprintGenerator->drawAllFootprints();
        }
    }

    // Layer 6: 3D Buildings (properly oriented) - drawn from Z=0 upward
    if (building3DGenerator && show3DBuildings) {
        if (show3DBuildingsByProgram) {
            building3DGenerator->draw3DBuildingsByProgram();
        }
        else {
            building3DGenerator->drawAll3DBuildings();
        }
    }
}

void highlightSelectedParcels() {
    if (!siteGen || selectedParcelIds.empty()) return;

    const auto& parcels = siteGen->getParcels();

    // Highlight selected parcels with green outline
    glColor3f(0.0f, 1.0f, 0.0f); // Green
    glLineWidth(3.0f);

    for (int selectedId : selectedParcelIds) {
        if (selectedId >= 0 && selectedId < parcels.size()) {
            const auto& parcel = parcels[selectedId];

            glBegin(GL_LINE_LOOP);
            for (const auto& pt : parcel.boundary) {
                glVertex3f(pt.x, pt.y, 0.1f);
            }
            glEnd();
        }
    }

    // Highlight hovered parcel with yellow outline
    if (hoveredParcelId >= 0 && hoveredParcelId < parcels.size()) {
        const auto& parcel = parcels[hoveredParcelId];

        glColor3f(1.0f, 1.0f, 0.0f); // Yellow
        glLineWidth(2.0f);

        glBegin(GL_LINE_LOOP);
        for (const auto& pt : parcel.boundary) {
            glVertex3f(pt.x, pt.y, 0.15f);
        }
        glEnd();
    }

    glLineWidth(1.0f);
}

void drawSelectionInfo() {
    // Draw selection instructions
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos3f(-4, -3.0, 0);
    string instructionText = "Click parcels to select (green). Press '3' for 3D buildings, 'e' to export OBJ.";
    for (char c : instructionText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }
}

// ==================== MOUSE SELECTION SYSTEM ====================

void handleMouseSelection(int x, int y) {
    int clickedParcelId = findParcelAtScreenPoint(x, y);

    if (clickedParcelId >= 0) {
        // Toggle selection
        auto it = find(selectedParcelIds.begin(), selectedParcelIds.end(), clickedParcelId);
        if (it != selectedParcelIds.end()) {
            // Deselect
            selectedParcelIds.erase(it);
            cout << "Deselected parcel " << clickedParcelId << endl;
        }
        else {
            // Select
            selectedParcelIds.push_back(clickedParcelId);
            cout << "Selected parcel " << clickedParcelId << " (Total selected: " << selectedParcelIds.size() << ")" << endl;
        }
    }
}

int findParcelAtScreenPoint(int x, int y) {
    if (!siteGen) return -1;

    // Convert screen coordinates to world coordinates
    Point2D worldPoint = screenToWorld(x, y);

    // Test each parcel to see if the point is inside
    const auto& parcels = siteGen->getParcels();
    for (size_t i = 0; i < parcels.size(); ++i) {
        if (parcels[i].containsPoint(worldPoint)) {
            return i;
        }
    }

    return -1; // No parcel found
}

Point2D screenToWorld(int screenX, int screenY) {
    // Get viewport dimensions
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Get current projection and modelview matrices
    GLdouble projection[16], modelview[16];
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);

    // Unproject to world coordinates (assuming z = 0 for 2D)
    GLdouble worldX, worldY, worldZ;
    gluUnProject(screenX, viewport[3] - screenY, 0.0, // Screen coords (flip Y)
        modelview, projection, viewport,      // Matrices and viewport
        &worldX, &worldY, &worldZ);           // Output world coords

    return Point2D(worldX, worldY); // Use X,Y for our 2D plane (aligned with footprints)
}

#endif // _MAIN_