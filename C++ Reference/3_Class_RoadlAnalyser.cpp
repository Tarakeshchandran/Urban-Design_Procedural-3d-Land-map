#define _MAIN_
#ifdef _MAIN_

#include "main.h"
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"      // Include our new road analyzer

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

// Visualization toggles
bool showSiteData = true;
bool showParcelAnalysis = true;
bool showRoadAnalysis = true;
bool showRoadIntersections = true;
bool showSubdivisionAlignments = false;
bool showNetworkPattern = false;

// Test points for subdivision alignment visualization
vector<Point2D> testPoints;

// ==================== ALICE FRAMEWORK FUNCTIONS ====================

void setup() {
    cout << "=== Site-Responsive Building Generation - Analysis Module ===" << endl;
    cout << "Initializing SiteGeneration, ParcelAnalyzer, and RoadAnalyzer..." << endl;

    siteGen = new SiteGeneration();
    parcelAnalyzer = new ParcelAnalyzer(siteGen);
    roadAnalyzer = new RoadAnalyzer(siteGen);

    cout << "\n*** WORKFLOW ***" << endl;
    cout << "1. Press 'g' to generate template CSV files" << endl;
    cout << "2. Edit curves.csv with your site data (roads, water, parks, parcels)" << endl;
    cout << "3. Press 'c' to import curves" << endl;
    cout << "4. Press 'b' to generate parcels from boundaries" << endl;
    cout << "5. Press 'a' to analyze parcels" << endl;
    cout << "6. Press 'r' to analyze road network" << endl;

    cout << "\n*** VISUALIZATION CONTROLS ***" << endl;
    cout << "- Press '1' to toggle site data (basic curves/parcels)" << endl;
    cout << "- Press '2' to toggle parcel analysis (categories, edges, corners)" << endl;
    cout << "- Press '3' to toggle road analysis (classifications, directions)" << endl;
    cout << "- Press '4' to toggle road intersections" << endl;
    cout << "- Press '5' to toggle subdivision alignments" << endl;
    cout << "- Press '6' to toggle network pattern visualization" << endl;
    cout << "- Press 't' to add test points for subdivision alignment" << endl;
    cout << "- Press 'x' to clear test points" << endl;

    cout << "\n*** ANALYSIS FEATURES ***" << endl;
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

    cout << "\nReady for comprehensive site analysis!" << endl;
}

bool compute = false;

void update(int value) {
    // Update logic can be added here
}

void draw() {
    // Clear the background
    backGround(0.95);
    drawGrid(5); // Scaled from 50 to 5

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

    // Draw help text (scaled positioning)
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos3f(-4, 3.5, 0); // Scaled from -40, 35
    string helpText = "Site Analysis - 'g' templates, 'c' import, 'b' parcels, 'a' analyze parcels, 'r' analyze roads";
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
        const auto& network = roadAnalyzer->getNetworkAnalysis();
        detailText += ", Pattern: " + roadAnalyzer->getNetworkPatternName(network.dominantPattern);
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
            delete parcelAnalyzer;
            delete roadAnalyzer;
            parcelAnalyzer = new ParcelAnalyzer(siteGen);
            roadAnalyzer = new RoadAnalyzer(siteGen);
        }
        else {
            cout << "Failed to import curves. Make sure 'curves.csv' exists." << endl;
        }
        break;

    case 'b':
        cout << "Generating parcels from imported boundaries..." << endl;
        siteGen->generateParcelsFromBoundaries();
        // Reinitialize analyzers with new parcels
        delete parcelAnalyzer;
        delete roadAnalyzer;
        parcelAnalyzer = new ParcelAnalyzer(siteGen);
        roadAnalyzer = new RoadAnalyzer(siteGen);
        break;

    case 'a':
        cout << "Analyzing all parcels..." << endl;
        if (parcelAnalyzer) {
            parcelAnalyzer->analyzeAllParcels();
        }
        break;

    case 'r':
        cout << "Analyzing road network..." << endl;
        if (roadAnalyzer) {
            roadAnalyzer->analyzeNetwork();
        }
        break;

    case 's':
        cout << "\n=== COMPREHENSIVE ANALYSIS STATISTICS ===" << endl;
        if (siteGen) siteGen->printStatistics();
        if (parcelAnalyzer) parcelAnalyzer->printAnalysisStatistics();
        if (roadAnalyzer) roadAnalyzer->printNetworkStatistics();
        break;

    case 'z':
        cout << "Resetting system..." << endl;
        testPoints.clear();
        delete parcelAnalyzer;
        delete roadAnalyzer;
        delete siteGen;
        siteGen = new SiteGeneration();
        parcelAnalyzer = new ParcelAnalyzer(siteGen);
        roadAnalyzer = new RoadAnalyzer(siteGen);
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
    }
}

void mousePress(int b, int state, int x, int y) {
    // Add test point at mouse position when right-clicking
    if (b == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        // Convert screen coordinates to world coordinates (simplified)
        // This would need proper screen-to-world coordinate conversion
        Point2D worldPos((x - 600) * 0.12f, (800 - y) * 0.12f);  // Scaled by 0.01 to match the smaller coordinate system
        testPoints.push_back(worldPos);
        cout << "Added test point at (" << worldPos.x << ", " << worldPos.y << "). Total: " << testPoints.size() << endl;
    }
}

void mouseMotion(int x, int y) {
    // Mouse motion can be added here if needed
}

#endif // _MAIN_