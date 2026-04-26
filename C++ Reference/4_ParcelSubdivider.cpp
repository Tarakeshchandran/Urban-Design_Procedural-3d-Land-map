#define _MAIN_
#ifdef _MAIN_

#include "main.h"
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"      // Include our road analyzer
#include "ParcelSubdivider.h"  // Include our parcel subdivider

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

// Visualization toggles
bool showSiteData = true;
bool showParcelAnalysis = true;
bool showRoadAnalysis = true;
bool showRoadIntersections = true;
bool showSubdivisionAlignments = false;
bool showNetworkPattern = false;
bool showSubdividedPlots = false;
bool showSubdivisionAxes = false;
bool showBuildableAreas = false;

// Debug toggles
bool verboseLogging = false;

// Test points for subdivision alignment visualization
vector<Point2D> testPoints;

// ==================== FUNCTION PROTOTYPES ====================
void initializeAnalyzers();
void analyzeAndSubdivideParcels();
void drawVisualization();
void printSubdivisionParameters();
void adjustSubdivisionParameters(); // New function to adjust parameters

// ==================== ALICE FRAMEWORK FUNCTIONS ====================

void setup() {
    cout << "=== Site-Responsive Building Generation - Analysis Module ===" << endl;
    cout << "Initializing SiteGeneration, ParcelAnalyzer, RoadAnalyzer, and ParcelSubdivider..." << endl;

    siteGen = new SiteGeneration();
    initializeAnalyzers();

    cout << "\n*** WORKFLOW ***" << endl;
    cout << "1. Press 'g' to generate template CSV files" << endl;
    cout << "2. Edit curves.csv with your site data (roads, water, parks, parcels)" << endl;
    cout << "3. Press 'c' to import curves" << endl;
    cout << "4. Press 'b' to generate parcels from boundaries" << endl;
    cout << "5. Press 'a' to analyze parcels and roads" << endl;
    cout << "6. Press 'd' to subdivide parcels" << endl;

    cout << "\n*** VISUALIZATION CONTROLS ***" << endl;
    cout << "- Press '1' to toggle site data (basic curves/parcels)" << endl;
    cout << "- Press '2' to toggle parcel analysis (categories, edges, corners)" << endl;
    cout << "- Press '3' to toggle road analysis (classifications, directions)" << endl;
    cout << "- Press '4' to toggle road intersections" << endl;
    cout << "- Press '5' to toggle subdivision alignments" << endl;
    cout << "- Press '6' to toggle network pattern visualization" << endl;
    cout << "- Press '7' to toggle subdivided plots" << endl;
    cout << "- Press '8' to toggle subdivision axes" << endl;
    cout << "- Press '9' to toggle buildable areas" << endl;
    cout << "- Press 't' to add test points for subdivision alignment" << endl;
    cout << "- Press 'x' to clear test points" << endl;
    cout << "- Press 'v' to toggle verbose logging" << endl;

    cout << "\n*** ANALYSIS & SUBDIVISION FEATURES ***" << endl;
    cout << "PARCEL ANALYSIS:" << endl;
    cout << "  - Large Parcels (≥15,000m²): Green, 4x subdivision, low density" << endl;
    cout << "  - Medium Parcels (≥10,000m²): Yellow, 2x subdivision, medium density" << endl;
    cout << "  - Small Parcels (≥2,500m²): Red, 1x/no subdivision, high density" << endl;
    cout << "  - Edge conditions: Water/Park/Street/Mixed edges" << endl;
    cout << "  - Corner conditions: Plaza/Park/Water/Street corners" << endl;

    cout << "\nROAD ANALYSIS:" << endl;
    cout << "  - Road classification: Primary/Secondary Arterials, Collectors, Local Streets" << endl;
    cout << "  - Direction vectors: Blue arrows (road direction), Green arrows (perpendicular)" << endl;
    cout << "  - Intersection detection: Colored markers by type and importance" << endl;
    cout << "  - Network pattern recognition: Grid/Radial/Organic/Mixed patterns" << endl;
    cout << "  - Subdivision alignment: Orange/Green lines showing optimal subdivision directions" << endl;

    cout << "\nPARCEL SUBDIVISION:" << endl;
    cout << "  - Road-aligned subdivision axes" << endl;
    cout << "  - Multi-level subdivision based on parcel size" << endl;
    cout << "  - Buildable area calculation with appropriate setbacks" << endl;
    cout << "  - Subdivision coloring: Light Gray (None), Cyan (Single), Green (Double), Magenta (Quadruple)" << endl;

    cout << "\nReady for comprehensive site analysis and subdivision!" << endl;
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
    string helpText = "Site Analysis - Press 'a' analyze parcels & roads, 'd' subdivide, 's' statistics";
    for (char c : helpText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Show current analysis status (scaled positioning)
    glRasterPos3f(-4, 3.0, 0); // Scaled from -40, 30
    string statusText = "";
    if (parcelAnalyzer) {
        statusText += "Plots: " + to_string(parcelAnalyzer->getAnalyzedPlots().size());
    }
    if (roadAnalyzer) {
        statusText += ", Roads: " + to_string(roadAnalyzer->getAnalyzedSegments().size());
        statusText += ", Intersections: " + to_string(roadAnalyzer->getIntersections().size());
    }
    if (parcelSubdivider) {
        statusText += ", Subdivided Plots: " + to_string(parcelSubdivider->getAllPlots().size());
    }
    for (char c : statusText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }

    // Show category and classification counts (scaled positioning)
    glRasterPos3f(-4, 2.5, 0); // Scaled from -40, 25
    string detailText = "";
    if (parcelAnalyzer) {
        auto largePlots = parcelAnalyzer->getPlotsByCategory(LARGE_PARCEL);
        auto mediumPlots = parcelAnalyzer->getPlotsByCategory(MEDIUM_PARCEL);
        auto smallPlots = parcelAnalyzer->getPlotsByCategory(SMALL_PARCEL);
        detailText += "Large: " + to_string(largePlots.size()) +
            " Medium: " + to_string(mediumPlots.size()) +
            " Small: " + to_string(smallPlots.size());
    }
    if (roadAnalyzer) {
        auto majorRoads = roadAnalyzer->getMajorRoadSegments();
        detailText += ", Major Roads: " + to_string(majorRoads.size());
        if (roadAnalyzer->getNetworkAnalysis().dominantPattern != NetworkPattern::MIXED_PATTERN) {
            detailText += ", Pattern: " + roadAnalyzer->getNetworkPatternName(roadAnalyzer->getNetworkAnalysis().dominantPattern);
        }
    }
    for (char c : detailText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
    }

    // Show test points count (scaled positioning)
    if (!testPoints.empty()) {
        glRasterPos3f(-4, 2.0, 0); // Scaled from -40, 20
        string testText = "Test Points: " + to_string(testPoints.size()) + " (press 't' to add, 'x' to clear)";
        for (char c : testText) {
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
                    // Adjust subdivision parameters to ensure medium and large parcels subdivide correctly
                    adjustSubdivisionParameters();
                    parcelSubdivider->subdivideAllParcels();
                    showSubdividedPlots = true;
                }
            }
            else {
                cout << "Error: Analysis components not initialized." << endl;
            }
        }
        break;

    case 'p':
        printSubdivisionParameters();
        break;

    case 's':
        cout << "\n=== COMPREHENSIVE ANALYSIS STATISTICS ===" << endl;
        if (siteGen) siteGen->printStatistics();
        if (parcelAnalyzer) parcelAnalyzer->printAnalysisStatistics();
        if (roadAnalyzer) roadAnalyzer->printNetworkStatistics();
        if (parcelSubdivider) parcelSubdivider->printSubdivisionStatistics();
        break;

    case 'z':
        cout << "Resetting system..." << endl;
        testPoints.clear();
        delete parcelAnalyzer;
        delete roadAnalyzer;
        delete parcelSubdivider;
        delete siteGen;
        siteGen = new SiteGeneration();
        initializeAnalyzers();
        break;

    case 't':
        // Add test point at center of view (you could modify this to use mouse position)
        testPoints.push_back(Point2D(0, 0));
        cout << "Added test point. Total: " << testPoints.size() << endl;
        break;

    case 'x':
        testPoints.clear();
        cout << "Cleared all test points." << endl;
        break;

    case 'v':
        verboseLogging = !verboseLogging;
        cout << "Verbose logging: " << (verboseLogging ? "ON" : "OFF") << endl;
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
        showRoadIntersections = !showRoadIntersections;
        cout << "Road intersections: " << (showRoadIntersections ? "ON" : "OFF") << endl;
        break;

    case '5':
        showSubdivisionAlignments = !showSubdivisionAlignments;
        cout << "Subdivision alignments: " << (showSubdivisionAlignments ? "ON" : "OFF") << endl;
        break;

    case '6':
        showNetworkPattern = !showNetworkPattern;
        cout << "Network pattern: " << (showNetworkPattern ? "ON" : "OFF") << endl;
        break;

    case '7':
        showSubdividedPlots = !showSubdividedPlots;
        cout << "Subdivided plots: " << (showSubdividedPlots ? "ON" : "OFF") << endl;
        break;

    case '8':
        showSubdivisionAxes = !showSubdivisionAxes;
        cout << "Subdivision axes: " << (showSubdivisionAxes ? "ON" : "OFF") << endl;
        break;

    case '9':
        showBuildableAreas = !showBuildableAreas;
        cout << "Buildable areas: " << (showBuildableAreas ? "ON" : "OFF") << endl;
        break;
    }
}

void mousePress(int b, int state, int x, int y) {
    // Add test point at mouse position when right-clicking
    if (b == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        // Convert screen coordinates to world coordinates (simplified)
        // This would need proper screen-to-world coordinate conversion
        Point2D worldPos((x - 600) * 0.1f, (800 - y) * 0.15f);  // Scaled by 0.012 to match the smaller coordinate system
        testPoints.push_back(worldPos);
        cout << "Added test point at (" << worldPos.x << ", " << worldPos.y << "). Total: " << testPoints.size() << endl;
    }
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

    // Create new analyzers
    parcelAnalyzer = new ParcelAnalyzer(siteGen);
    roadAnalyzer = new RoadAnalyzer(siteGen);
    parcelSubdivider = new ParcelSubdivider(siteGen, parcelAnalyzer, roadAnalyzer);

    cout << "Analysis components initialized." << endl;
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

        // Print parcel categorization details if verbose logging is enabled
        if (verboseLogging) {
            const auto& analyzedPlots = parcelAnalyzer->getAnalyzedPlots();
            cout << "\nParcel category details:" << endl;
            for (size_t i = 0; i < analyzedPlots.size(); ++i) {
                const auto& plot = analyzedPlots[i];
                cout << "Parcel " << i << ": Area = " << plot.area << " unit² ("
                    << plot.area * 10 << "m²), Category: "
                    << parcelAnalyzer->getParcelCategoryName(plot.category) << endl;
            }
        }
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

    // Layer 4: Road intersections
    if (roadAnalyzer && showRoadIntersections) {
        roadAnalyzer->drawIntersections();
    }

    // Layer 5: Network pattern
    if (roadAnalyzer && showNetworkPattern) {
        roadAnalyzer->drawNetworkPattern();
    }

    // Layer 6: Subdivision alignments
    if (roadAnalyzer && showSubdivisionAlignments && !testPoints.empty()) {
        roadAnalyzer->drawSubdivisionAlignments(testPoints);
    }

    // Layer 7: Subdivided plots
    if (parcelSubdivider && showSubdividedPlots) {
        parcelSubdivider->drawPlotsBySubdivisionLevel();
    }

    // Add this line to draw open spaces with green color
    if (parcelSubdivider && showSubdividedPlots) {
        parcelSubdivider->drawOpenSpacesSimplified();
    }

    // Layer 8: Subdivision axes
    if (parcelSubdivider && showSubdivisionAxes && showSubdividedPlots) {
        parcelSubdivider->drawSubdivisionAxes();
    }

    // Layer 9: Buildable areas
    if (parcelSubdivider && showBuildableAreas && showSubdividedPlots) {
        parcelSubdivider->drawBuildableAreas();
    }

    // Layer 10: Road connections for subdivided plots
    if (parcelSubdivider && showRoadAnalysis && showSubdividedPlots) {
        parcelSubdivider->drawRoadConnections();
    }

    // Draw test points (scaled point size)
    if (!testPoints.empty()) {
        glColor3f(1.0f, 0.0f, 0.0f); // Red
        glPointSize(8.0f); // Increased for better visibility
        glBegin(GL_POINTS);
        for (const auto& point : testPoints) {
            glVertex3f(point.x, point.y, 0.1f);
        }
        glEnd();
        glPointSize(1.0f);
    }
}

void printSubdivisionParameters() {
    cout << "\n=== SUBDIVISION PARAMETERS ===" << endl;

    if (!parcelSubdivider) {
        cout << "ParcelSubdivider not initialized!" << endl;
        return;
    }

    // Print parameters for each category
    for (int cat = LARGE_PARCEL; cat <= SMALL_PARCEL; cat++) {
        ParcelCategory category = static_cast<ParcelCategory>(cat);
        string catName = parcelSubdivider->getParcelCategoryName(category);
        auto params = parcelSubdivider->getCategoryParameters(category);

        cout << "Category: " << catName << endl;
        cout << "  Subdivision Level: " << parcelSubdivider->getSubdivisionLevelName(params.level) << endl;
        cout << "  Minimum Plot Area: " << params.minPlotArea << " unit² (" << params.minPlotArea * 10 << "m²)" << endl;
        cout << "  Preferred Aspect Ratio: " << params.preferredAspectRatio << endl;
        cout << "  Road Alignment Weight: " << params.roadAlignmentWeight << endl;
        cout << "  Allow Irregular Splits: " << (params.allowIrregularSplits ? "Yes" : "No") << endl;

        // Calculate and show the minimum parcel area required for subdivision
        int levelInt = static_cast<int>(params.level);
        float minAreaRequired = params.minPlotArea * levelInt;
        cout << "  Minimum Parcel Area Required: " << minAreaRequired << " unit² ("
            << minAreaRequired * 10 << "m²)" << endl;
        cout << endl;
    }
}

void adjustSubdivisionParameters() {
    if (!parcelSubdivider) {
        cout << "Error: ParcelSubdivider not initialized!" << endl;
        return;
    }

    cout << "Adjusting subdivision parameters to ensure proper subdivision..." << endl;

    // Get the original parameters for reference
    SubdivisionParameters largeParams = parcelSubdivider->getCategoryParameters(LARGE_PARCEL);
    SubdivisionParameters mediumParams = parcelSubdivider->getCategoryParameters(MEDIUM_PARCEL);
    SubdivisionParameters smallParams = parcelSubdivider->getCategoryParameters(SMALL_PARCEL);

    // Print original parameters if verbose logging is enabled
    if (verboseLogging) {
        cout << "Original parameters:" << endl;
        cout << "  Large Parcels: minPlotArea = " << largeParams.minPlotArea
            << ", level = " << static_cast<int>(largeParams.level) << endl;
        cout << "  Medium Parcels: minPlotArea = " << mediumParams.minPlotArea
            << ", level = " << static_cast<int>(mediumParams.level) << endl;
        cout << "  Small Parcels: minPlotArea = " << smallParams.minPlotArea
            << ", level = " << static_cast<int>(smallParams.level) << endl;
    }

    // Get the analyzed parcels to check their actual areas
    const auto& analyzedPlots = parcelAnalyzer->getAnalyzedPlots();

    // Calculate the average area for each category to help set appropriate parameters
    float totalLargeArea = 0.0f, totalMediumArea = 0.0f, totalSmallArea = 0.0f;
    int largeCount = 0, mediumCount = 0, smallCount = 0;

    for (const auto& plot : analyzedPlots) {
        switch (plot.category) {
        case LARGE_PARCEL:
            totalLargeArea += plot.area;
            largeCount++;
            break;
        case MEDIUM_PARCEL:
            totalMediumArea += plot.area;
            mediumCount++;
            break;
        case SMALL_PARCEL:
            totalSmallArea += plot.area;
            smallCount++;
            break;
        }
    }

    // Calculate average areas
    float avgLargeArea = largeCount > 0 ? totalLargeArea / largeCount : 0;
    float avgMediumArea = mediumCount > 0 ? totalMediumArea / mediumCount : 0;
    float avgSmallArea = smallCount > 0 ? totalSmallArea / smallCount : 0;

    if (verboseLogging) {
        cout << "Average areas:" << endl;
        cout << "  Large Parcels: " << avgLargeArea << " unit² (" << avgLargeArea * 10 << "m²)" << endl;
        cout << "  Medium Parcels: " << avgMediumArea << " unit² (" << avgMediumArea * 10 << "m²)" << endl;
        cout << "  Small Parcels: " << avgSmallArea << " unit² (" << avgSmallArea * 10 << "m²)" << endl;
    }

    // Adjust parameters based on actual parcel sizes
    // LARGE PARCELS: Need area >= minPlotArea * 4
    if (largeCount > 0) {
        float newMinPlotArea = avgLargeArea / 5.0f; // Set to 1/5 of average to ensure subdivision
        largeParams.minPlotArea = min(newMinPlotArea, 20.0f); // Cap at 20 (200m²)
    }
    else {
        largeParams.minPlotArea = 20.0f; // Default fallback
    }

    // MEDIUM PARCELS: Need area >= minPlotArea * 2
    if (mediumCount > 0) {
        float newMinPlotArea = avgMediumArea / 3.0f; // Set to 1/3 of average to ensure subdivision
        mediumParams.minPlotArea = min(newMinPlotArea, 30.0f); // Cap at 30 (300m²)
    }
    else {
        mediumParams.minPlotArea = 30.0f; // Default fallback
    }

    // SMALL PARCELS: Keep as is but reduce minimum if needed
    if (smallCount > 0 && avgSmallArea < 50.0f) {
        smallParams.minPlotArea = min(avgSmallArea / 1.5f, 25.0f); // Adjust if parcels are very small
    }

    // Update the parameters in the subdivider
    parcelSubdivider->updateCategoryParameters(LARGE_PARCEL, largeParams);
    parcelSubdivider->updateCategoryParameters(MEDIUM_PARCEL, mediumParams);
    parcelSubdivider->updateCategoryParameters(SMALL_PARCEL, smallParams);

    // Print adjusted parameters if verbose logging is enabled
    if (verboseLogging) {
        cout << "Adjusted parameters:" << endl;
        cout << "  Large Parcels: minPlotArea = " << largeParams.minPlotArea
            << ", level = " << static_cast<int>(largeParams.level) << endl;
        cout << "  Medium Parcels: minPlotArea = " << mediumParams.minPlotArea
            << ", level = " << static_cast<int>(mediumParams.level) << endl;
        cout << "  Small Parcels: minPlotArea = " << smallParams.minPlotArea
            << ", level = " << static_cast<int>(smallParams.level) << endl;
    }

    cout << "Subdivision parameters adjusted successfully." << endl;
}

#endif // _MAIN_