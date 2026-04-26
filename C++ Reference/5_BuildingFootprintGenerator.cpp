#define _MAIN_
#ifdef _MAIN_

#include "main.h"
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"
#include "ParcelSubdivider.h"
#include "BuildingFootprintGenerator.h"

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

// Visualization toggles
bool showSiteData = true;
bool showParcelAnalysis = false;
bool showRoadAnalysis = false;
bool showSubdividedPlots = false;
bool showBuildingFootprints = true;
bool colorFootprintsByEdgeType = false;

// Additional settings
bool verboseLogging = false;

// ==================== FUNCTION PROTOTYPES ====================
void initializeAnalyzers();
void analyzeAndSubdivideParcels();
void generateBuildingFootprints();
void drawVisualization();
void adjustSubdivisionParameters();

// ==================== ALICE FRAMEWORK FUNCTIONS ====================

void setup() {
    cout << "=== Urban Building Generation System - FIXED LARGE PARCELS ===" << endl;
    cout << "Initializing SiteGeneration, ParcelAnalyzer, RoadAnalyzer, ParcelSubdivider, and BuildingFootprintGenerator..." << endl;

    siteGen = new SiteGeneration();
    initializeAnalyzers();
    adjustSubdivisionParameters(); // Apply fixes

    cout << "\n*** WORKFLOW ***" << endl;
    cout << "1. Press 'g' to generate template CSV files" << endl;
    cout << "2. Edit curves.csv with your site data (roads, water, parks, parcels)" << endl;
    cout << "3. Press 'c' to import curves" << endl;
    cout << "4. Press 'b' to generate parcels from boundaries" << endl;
    cout << "5. Press 'a' to analyze parcels and roads" << endl;
    cout << "6. Press 'd' to subdivide parcels" << endl;
    cout << "7. Press 'f' to generate building footprints" << endl;

    cout << "\n*** VISUALIZATION CONTROLS ***" << endl;
    cout << "- Press '1' to toggle site data (basic curves/parcels)" << endl;
    cout << "- Press '2' to toggle parcel analysis (categories, edges, corners)" << endl;
    cout << "- Press '3' to toggle road analysis (classifications, directions)" << endl;
    cout << "- Press '4' to toggle subdivided plots" << endl;
    cout << "- Press '5' to toggle building footprints" << endl;
    cout << "- Press '6' to toggle coloring footprints by edge type" << endl;
    cout << "- Press 'v' to toggle verbose logging" << endl;
    cout << "- Press 'x' to debug medium parcel subdivision" << endl;
    cout << "- Press 'y' to debug large parcel subdivision" << endl;

    cout << "\n*** FIXES APPLIED ***" << endl;
    cout << "- FIXED: Large parcel subdivision (quadruple division)" << endl;
    cout << "- FIXED: Reduced aggressive open space creation" << endl;
    cout << "- FIXED: Medium parcel courtyard generation" << endl;
    cout << "- FIXED: Building footprint coverage ratios for large plots" << endl;

    cout << "\nReady for urban generation!" << endl;
}

bool compute = false;

void update(int value) {
    // Update logic can be added here
}

void draw() {
    // Clear the background
    backGround(0.95);
    drawGrid(5); // Scaled from 50 to 5

    // Draw all visualization elements
    drawVisualization();

    // Draw help text (scaled positioning)
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos3f(-4, 3.5, 0); // Scaled from -40, 35
    string helpText = "Urban Building Generator - LARGE PARCEL FIX - Press 'a' to analyze, 'd' to subdivide, 'f' for building footprints";
    for (char c : helpText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Show current status (scaled positioning)
    glRasterPos3f(-4, 3.0, 0); // Scaled from -40, 30
    string statusText = "";
    if (parcelAnalyzer) {
        statusText += "Plots: " + to_string(parcelAnalyzer->getAnalyzedPlots().size());
    }
    if (parcelSubdivider) {
        statusText += ", Subdivided: " + to_string(parcelSubdivider->getAllPlots().size());
        // Count non-open space plots
        int buildablePlots = 0;
        for (const auto& plot : parcelSubdivider->getAllPlots()) {
            if (!plot.isOpenSpace && plot.isViablePlot()) {
                buildablePlots++;
            }
        }
        statusText += " (Buildable: " + to_string(buildablePlots) + ")";
    }
    if (footprintGenerator) {
        statusText += ", Buildings: " + to_string(footprintGenerator->getAllFootprints().size());
    }
    for (char c : statusText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }

    // Show parcel category distribution (scaled positioning)
    glRasterPos3f(-4, 2.5, 0);
    if (parcelAnalyzer && !parcelAnalyzer->getAnalyzedPlots().empty()) {
        int large = 0, medium = 0, small = 0;
        for (const auto& plot : parcelAnalyzer->getAnalyzedPlots()) {
            switch (plot.category) {
            case LARGE_PARCEL: large++; break;
            case MEDIUM_PARCEL: medium++; break;
            case SMALL_PARCEL: small++; break;
            }
        }
        string categoryText = "Parcels - Large: " + to_string(large) +
            ", Medium: " + to_string(medium) +
            ", Small: " + to_string(small);
        for (char c : categoryText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
        }
    }
}

void keyPress(unsigned char k, int xm, int ym) {
    if (!siteGen) return;

    switch (k) {
    case 'g':
        cout << "Generating template CSV files..." << endl;
        siteGen->generateExampleFiles();
        cout << "Template files created! Edit them with your site data." << endl;
        break;

    case 'c':
        cout << "Importing curves from CSV..." << endl;
        if (siteGen->importCurvesFromCSV("curves.csv")) {
            cout << "Curves imported successfully!" << endl;
            // Reinitialize analyzers with new data
            initializeAnalyzers();
            adjustSubdivisionParameters();
        }
        else {
            cout << "Failed to import curves. Make sure 'curves.csv' exists." << endl;
        }
        break;

    case 'b':
        cout << "Generating parcels from imported boundaries..." << endl;
        siteGen->generateParcelsFromBoundaries();
        // Reinitialize analyzers with new parcels
        initializeAnalyzers();
        adjustSubdivisionParameters();
        break;

    case 'a':
        cout << "Analyzing parcels and road network..." << endl;
        analyzeAndSubdivideParcels();
        break;

    case 'd':
        cout << "Subdividing parcels based on analysis..." << endl;
        if (parcelSubdivider) {
            // Make sure analysis was done before subdivision
            if (parcelAnalyzer && roadAnalyzer) {
                if (parcelAnalyzer->getAnalyzedPlots().size() == 0) {
                    cout << "Please run analysis ('a') before subdivision." << endl;
                }
                else {
                    parcelSubdivider->subdivideAllParcels();
                    showSubdividedPlots = true;

                    // Debug output for all categories
                    cout << "\n=== SUBDIVISION DEBUG SUMMARY ===" << endl;
                    int totalPlots = parcelSubdivider->getAllPlots().size();
                    int openSpaces = 0, buildablePlots = 0;

                    map<ParcelCategory, pair<int, int>> categoryStats; // plots, open spaces

                    for (const auto& plot : parcelSubdivider->getAllPlots()) {
                        if (plot.isOpenSpace) {
                            openSpaces++;
                            categoryStats[plot.originalCategory].second++;
                        }
                        else if (plot.isViablePlot()) {
                            buildablePlots++;
                        }
                        categoryStats[plot.originalCategory].first++;
                    }

                    cout << "Total plots: " << totalPlots << endl;
                    cout << "Buildable plots: " << buildablePlots << endl;
                    cout << "Open spaces: " << openSpaces << endl;

                    cout << "\nBy category:" << endl;
                    cout << "Large parcels: " << categoryStats[LARGE_PARCEL].first
                        << " plots (" << categoryStats[LARGE_PARCEL].second << " open spaces)" << endl;
                    cout << "Medium parcels: " << categoryStats[MEDIUM_PARCEL].first
                        << " plots (" << categoryStats[MEDIUM_PARCEL].second << " open spaces)" << endl;
                    cout << "Small parcels: " << categoryStats[SMALL_PARCEL].first
                        << " plots (" << categoryStats[SMALL_PARCEL].second << " open spaces)" << endl;
                }
            }
            else {
                cout << "Error: Analysis components not initialized." << endl;
            }
        }
        break;

    case 'f':
        cout << "Generating building footprints..." << endl;
        generateBuildingFootprints();
        break;

    case 's':
        cout << "\n=== COMPREHENSIVE GENERATION STATISTICS ===" << endl;
        if (siteGen) siteGen->printStatistics();
        if (parcelAnalyzer) parcelAnalyzer->printAnalysisStatistics();
        if (parcelSubdivider) parcelSubdivider->printSubdivisionStatistics();
        if (footprintGenerator) footprintGenerator->printFootprintStatistics();
        break;

    case 'z':
        cout << "Resetting system..." << endl;
        delete footprintGenerator;
        delete parcelSubdivider;
        delete roadAnalyzer;
        delete parcelAnalyzer;
        delete siteGen;
        siteGen = new SiteGeneration();
        initializeAnalyzers();
        adjustSubdivisionParameters();
        break;

    case 'v':
        verboseLogging = !verboseLogging;
        cout << "Verbose logging: " << (verboseLogging ? "ON" : "OFF") << endl;
        if (footprintGenerator) {
            footprintGenerator->setVerboseLogging(verboseLogging);
        }
        break;

    case 'x':
        // Debug medium parcel subdivision
        cout << "\n=== DEBUGGING MEDIUM PARCEL SUBDIVISION ===" << endl;
        if (parcelSubdivider && parcelAnalyzer) {
            auto mediumParams = parcelSubdivider->getCategoryParameters(MEDIUM_PARCEL);
            cout << "Medium parcel parameters:" << endl;
            cout << "  Subdivision level: " << (int)mediumParams.level << endl;
            cout << "  Min plot area: " << mediumParams.minPlotArea << " unit²" << endl;
            cout << "  Open space ratio: " << mediumParams.openSpaceRatio << endl;

            auto mediumPlots = parcelSubdivider->getPlotsByCategory(MEDIUM_PARCEL);
            cout << "Medium plots generated: " << mediumPlots.size() << endl;

            int openSpaceCount = 0, buildableCount = 0;
            for (const auto& plot : mediumPlots) {
                if (plot.isOpenSpace) {
                    openSpaceCount++;
                    cout << "  Plot " << plot.id << ": OPEN SPACE (" << parcelSubdivider->getOpenSpaceTypeName(plot.openSpaceType) << ")" << endl;
                }
                else if (plot.isViablePlot()) {
                    buildableCount++;
                    cout << "  Plot " << plot.id << ": BUILDABLE (Area: " << plot.area << ")" << endl;
                }
                else {
                    cout << "  Plot " << plot.id << ": NOT VIABLE (Area: " << plot.area << ")" << endl;
                }
            }
            cout << "Open spaces: " << openSpaceCount << ", Buildable: " << buildableCount << endl;
        }
        break;

    case 'y':
        // Debug large parcel subdivision
        cout << "\n=== DEBUGGING LARGE PARCEL SUBDIVISION ===" << endl;
        if (parcelSubdivider && parcelAnalyzer) {
            auto largeParams = parcelSubdivider->getCategoryParameters(LARGE_PARCEL);
            cout << "Large parcel parameters:" << endl;
            cout << "  Subdivision level: " << (int)largeParams.level << endl;
            cout << "  Min plot area: " << largeParams.minPlotArea << " unit²" << endl;
            cout << "  Open space ratio: " << largeParams.openSpaceRatio << endl;

            auto largePlots = parcelSubdivider->getPlotsByCategory(LARGE_PARCEL);
            cout << "Large plots generated: " << largePlots.size() << endl;

            int openSpaceCount = 0, buildableCount = 0, nonViableCount = 0;
            for (const auto& plot : largePlots) {
                if (plot.isOpenSpace) {
                    openSpaceCount++;
                    cout << "  Plot " << plot.id << ": OPEN SPACE (" << parcelSubdivider->getOpenSpaceTypeName(plot.openSpaceType) << ", Area: " << plot.area << ")" << endl;
                }
                else if (plot.isViablePlot()) {
                    buildableCount++;
                    cout << "  Plot " << plot.id << ": BUILDABLE (Area: " << plot.area << ", Subdivision Level: " << plot.subdivisionLevel << ")" << endl;
                }
                else {
                    nonViableCount++;
                    cout << "  Plot " << plot.id << ": NOT VIABLE (Area: " << plot.area << " - below minimum " << largeParams.minPlotArea << ")" << endl;
                }
            }
            cout << "Summary - Open spaces: " << openSpaceCount << ", Buildable: " << buildableCount << ", Non-viable: " << nonViableCount << endl;

            // Check original large parcels
            cout << "\nOriginal large parcels:" << endl;
            for (const auto& analyzedPlot : parcelAnalyzer->getAnalyzedPlots()) {
                if (analyzedPlot.category == LARGE_PARCEL) {
                    cout << "  Original parcel " << analyzedPlot.id << ": Area " << analyzedPlot.area << " unit²" << endl;
                }
            }
        }
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

    case '3':
        showRoadAnalysis = !showRoadAnalysis;
        cout << "Road analysis: " << (showRoadAnalysis ? "ON" : "OFF") << endl;
        break;

    case '4':
        showSubdividedPlots = !showSubdividedPlots;
        cout << "Subdivided plots: " << (showSubdividedPlots ? "ON" : "OFF") << endl;
        break;

    case '5':
        showBuildingFootprints = !showBuildingFootprints;
        cout << "Building footprints: " << (showBuildingFootprints ? "ON" : "OFF") << endl;
        break;

    case '6':
        colorFootprintsByEdgeType = !colorFootprintsByEdgeType;
        cout << "Color footprints by edge type: " << (colorFootprintsByEdgeType ? "ON" : "OFF") << endl;
        break;
    }
}

void mousePress(int b, int state, int x, int y) {
    // Mouse interaction can be added here if needed
}

void mouseMotion(int x, int y) {
    // Mouse motion can be added here if needed
}

// ==================== CUSTOM FUNCTIONS ====================

void initializeAnalyzers() {
    // Clean up any existing analyzers
    if (parcelAnalyzer) delete parcelAnalyzer;
    if (roadAnalyzer) delete roadAnalyzer;
    if (parcelSubdivider) delete parcelSubdivider;
    if (footprintGenerator) delete footprintGenerator;

    // Create new analyzers
    parcelAnalyzer = new ParcelAnalyzer(siteGen);
    roadAnalyzer = new RoadAnalyzer(siteGen);
    parcelSubdivider = new ParcelSubdivider(siteGen, parcelAnalyzer, roadAnalyzer);
    footprintGenerator = new BuildingFootprintGenerator(parcelSubdivider, parcelAnalyzer, roadAnalyzer);

    // Set verbosity
    if (footprintGenerator) {
        footprintGenerator->setVerboseLogging(verboseLogging);
    }

    cout << "Analysis components initialized." << endl;
}

void adjustSubdivisionParameters() {
    if (!parcelSubdivider) return;

    cout << "Applying FIXED subdivision parameters..." << endl;

    // FIXED: Large parcel parameters - reduce central park creation significantly
    SubdivisionParameters largeParams(SubdivisionLevel::QUADRUPLE_DIVISION);
    largeParams.minPlotArea = 25.0f;  // REDUCED from 35.0f - allow smaller plots after subdivision
    largeParams.preferredAspectRatio = 1.5f;
    largeParams.roadAlignmentWeight = 0.9f;
    largeParams.allowIrregularSplits = true;
    largeParams.openSpaceRatio = 0.10f; // SIGNIFICANTLY REDUCED from 0.20f to 10%
    largeParams.preferredOpenSpaceType = OpenSpaceType::CENTRAL_PARK;

    parcelSubdivider->updateCategoryParameters(LARGE_PARCEL, largeParams);

    // FIXED: Medium parcel parameters - minimal courtyard creation
    SubdivisionParameters mediumParams(SubdivisionLevel::DOUBLE_DIVISION);
    mediumParams.minPlotArea = 30.0f;  // REDUCED from 40.0f
    mediumParams.preferredAspectRatio = 1.2f;
    mediumParams.roadAlignmentWeight = 0.8f;
    mediumParams.allowIrregularSplits = true;
    mediumParams.openSpaceRatio = 0.05f; // SIGNIFICANTLY REDUCED from 0.08f to 5%
    mediumParams.preferredOpenSpaceType = OpenSpaceType::COURTYARD;

    parcelSubdivider->updateCategoryParameters(MEDIUM_PARCEL, mediumParams);

    // Small parcel parameters - keep minimal
    SubdivisionParameters smallParams(SubdivisionLevel::SINGLE_DIVISION);
    smallParams.minPlotArea = 25.0f;
    smallParams.preferredAspectRatio = 1.0f;
    smallParams.roadAlignmentWeight = 0.7f;
    smallParams.allowIrregularSplits = false;
    smallParams.openSpaceRatio = 0.02f; // REDUCED from 0.03f to 2%
    smallParams.preferredOpenSpaceType = OpenSpaceType::POCKET_PARK;

    parcelSubdivider->updateCategoryParameters(SMALL_PARCEL, smallParams);

    cout << "FIXED subdivision parameters applied:" << endl;
    cout << "  Large parcels: 10% open space (was 20%)" << endl;
    cout << "  Medium parcels: 5% open space (was 8%)" << endl;
    cout << "  Small parcels: 2% open space (was 3%)" << endl;
    cout << "  Reduced minimum plot areas for better subdivision" << endl;
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
    else {
        cout << "Error: ParcelAnalyzer not initialized." << endl;
    }

    // Analyze road network
    if (roadAnalyzer) {
        cout << "Analyzing road network..." << endl;
        roadAnalyzer->analyzeNetwork();
    }
    else {
        cout << "Error: RoadAnalyzer not initialized." << endl;
    }

    cout << "Analysis complete! Use 'd' to subdivide parcels based on analysis." << endl;
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

    cout << "Generating building footprints with FIXED parameters..." << endl;

    // FIXED: Adjust building footprint parameters for better coverage
    footprintGenerator->setCoverageRatios(0.4f, 0.7f); // Min 40%, Max 70% (was 50%-80%)
    footprintGenerator->setDefaultSetbacks(0.3f, 0.5f, 0.3f, 0.8f, 0.6f); // Reduced setbacks

    // Debug: Count plots by category before footprint generation
    int totalPlots = parcelSubdivider->getAllPlots().size();
    int buildablePlots = 0;
    int openSpaces = 0;
    int nonViablePlots = 0;

    map<ParcelCategory, int> buildableByCategory;
    map<ParcelCategory, int> openSpaceByCategory;
    map<ParcelCategory, int> nonViableByCategory;

    for (const auto& plot : parcelSubdivider->getAllPlots()) {
        if (plot.isOpenSpace) {
            openSpaces++;
            openSpaceByCategory[plot.originalCategory]++;
        }
        else if (plot.isViablePlot()) {
            buildablePlots++;
            buildableByCategory[plot.originalCategory]++;
        }
        else {
            nonViablePlots++;
            nonViableByCategory[plot.originalCategory]++;
        }
    }

    cout << "Plot analysis before footprint generation:" << endl;
    cout << "  Total plots: " << totalPlots << endl;
    cout << "  Buildable plots: " << buildablePlots << endl;
    cout << "  Open spaces: " << openSpaces << endl;
    cout << "  Non-viable plots: " << nonViablePlots << endl;

    cout << "Buildable plots by category:" << endl;
    cout << "  Large: " << buildableByCategory[LARGE_PARCEL] << " (open: " << openSpaceByCategory[LARGE_PARCEL] << ", non-viable: " << nonViableByCategory[LARGE_PARCEL] << ")" << endl;
    cout << "  Medium: " << buildableByCategory[MEDIUM_PARCEL] << " (open: " << openSpaceByCategory[MEDIUM_PARCEL] << ", non-viable: " << nonViableByCategory[MEDIUM_PARCEL] << ")" << endl;
    cout << "  Small: " << buildableByCategory[SMALL_PARCEL] << " (open: " << openSpaceByCategory[SMALL_PARCEL] << ", non-viable: " << nonViableByCategory[SMALL_PARCEL] << ")" << endl;

    footprintGenerator->generateAllFootprints();
    showBuildingFootprints = true;

    cout << "Building footprint generation complete!" << endl;
    cout << "Generated " << footprintGenerator->getAllFootprints().size() << " building footprints" << endl;

    // Final statistics
    map<ParcelCategory, int> footprintsByCategory;
    for (const auto& footprint : footprintGenerator->getAllFootprints()) {
        // Find the parent plot to determine category
        for (const auto& plot : parcelSubdivider->getAllPlots()) {
            if (plot.id == footprint.parentPlotId) {
                footprintsByCategory[plot.originalCategory]++;
                break;
            }
        }
    }

    cout << "Building footprints by original parcel category:" << endl;
    cout << "  Large parcels: " << footprintsByCategory[LARGE_PARCEL] << " footprints" << endl;
    cout << "  Medium parcels: " << footprintsByCategory[MEDIUM_PARCEL] << " footprints" << endl;
    cout << "  Small parcels: " << footprintsByCategory[SMALL_PARCEL] << " footprints" << endl;
}

void drawVisualization() {
    // Layer 1: Basic site data
    if (siteGen && showSiteData) {
        siteGen->drawSite();
    }

    // Layer 2: Parcel analysis
    if (parcelAnalyzer && showParcelAnalysis) {
        parcelAnalyzer->drawAnalysisResults();
        parcelAnalyzer->drawEdgeConditions();
        parcelAnalyzer->drawCornerConditions();
    }

    // Layer 3: Road analysis
    if (roadAnalyzer && showRoadAnalysis) {
        roadAnalyzer->drawAnalyzedRoads();
    }

    // Layer 4: Subdivided plots
    if (parcelSubdivider && showSubdividedPlots) {
        parcelSubdivider->drawPlotsBySubdivisionLevel();
        parcelSubdivider->drawOpenSpaces();
    }

    // Layer 5: Building footprints
    if (footprintGenerator && showBuildingFootprints) {
        if (colorFootprintsByEdgeType) {
            footprintGenerator->drawFootprintsByEdgeType();
        }
        else {
            footprintGenerator->drawAllFootprints();
        }
    }
}

#endif // _MAIN_