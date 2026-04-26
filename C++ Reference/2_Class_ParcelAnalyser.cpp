#define _MAIN_
#ifdef _MAIN_

#include "main.h"
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"  // Include our new header

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

// Visualization toggles
bool showSiteData = true;
bool showAnalysisResults = true;
bool showRoadSegments = true;
bool showEdgeConditions = true;
bool showCornerConditions = true;
bool showRoadFrontages = false;

// ==================== ALICE FRAMEWORK FUNCTIONS ====================

void setup() {
    cout << "=== Site-Responsive Building Generation - Parcel Analysis Module ===" << endl;
    cout << "Initializing SiteGeneration and ParcelAnalyzer..." << endl;

    siteGen = new SiteGeneration();
    parcelAnalyzer = new ParcelAnalyzer(siteGen);

    cout << "\n*** WORKFLOW ***" << endl;
    cout << "1. Press 'g' to generate template CSV files" << endl;
    cout << "2. Edit curves.csv with your site data (roads, water, parks, parcels)" << endl;
    cout << "3. Press 'c' to import curves" << endl;
    cout << "4. Press 'b' to generate parcels from boundaries" << endl;
    cout << "5. Press 'a' to analyze all parcels" << endl;

    cout << "\n*** ANALYSIS VISUALIZATION ***" << endl;
    cout << "- Press '1' to toggle site data" << endl;
    cout << "- Press '2' to toggle analysis results (category colors)" << endl;
    cout << "- Press '3' to toggle road segments" << endl;
    cout << "- Press '4' to toggle edge conditions" << endl;
    cout << "- Press '5' to toggle corner conditions" << endl;
    cout << "- Press '6' to toggle road frontages" << endl;

    cout << "\n*** UPDATED PARCEL CATEGORIES ***" << endl;
    cout << "  - Large Parcels (≥15,000m²): Green, 4x subdivision, low density" << endl;
    cout << "  - Medium Parcels (≥10,000m²): Yellow, 2x subdivision, medium density" << endl;
    cout << "  - Small Parcels (≥2,500m²): Red, 1x/no subdivision, high density" << endl;
    cout << "\nEdge Conditions:" << endl;
    cout << "  - Water Edge: Blue outline" << endl;
    cout << "  - Park Edge: Green outline" << endl;
    cout << "  - Street Edge: Gray outline" << endl;
    cout << "  - Mixed Edge: Magenta outline" << endl;
    cout << "\nCorner Conditions:" << endl;
    cout << "  - Corner markers at parcel centers (colored squares)" << endl;
    cout << "\nRoad Hierarchy:" << endl;
    cout << "  - Major Arterial: Thick red lines" << endl;
    cout << "  - Local Streets: Thin gray lines" << endl;
    cout << "  - Blue arrows show road direction" << endl;
    cout << "\nRoad Frontages:" << endl;
    cout << "  - Cyan lines from parcel center to primary street frontage" << endl;
    cout << "  - Magenta points show frontage locations" << endl;

    cout << "\nReady for parcel analysis!" << endl;
}

bool compute = false;

void update(int value) {
    // Update logic can be added here
}

void draw() {
    // Clear the background
    backGround(0.95);
    drawGrid(5); // Scaled from 50 to 5

    // Draw site data if available and enabled
    if (siteGen && showSiteData) {
        siteGen->drawSite();
    }

    // Draw analysis results if available and enabled
    if (parcelAnalyzer) {
        if (showAnalysisResults) {
            parcelAnalyzer->drawAnalysisResults();
        }

        if (showRoadSegments) {
            parcelAnalyzer->drawRoadSegments();
        }

        if (showEdgeConditions) {
            parcelAnalyzer->drawEdgeConditions();
        }

        if (showCornerConditions) {
            parcelAnalyzer->drawCornerConditions();
        }

        if (showRoadFrontages) {
            parcelAnalyzer->drawRoadFrontages();
        }
    }

    // Draw help text (scaled positioning)
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos3f(-4, 3.5, 0); // Scaled from -40, 35
    string helpText = "Parcel Analysis - 'g' templates, 'c' import, 'b' parcels, 'a' analyze, '1-6' toggle views";
    for (char c : helpText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Show current analysis status (scaled positioning)
    if (parcelAnalyzer) {
        glRasterPos3f(-4, 3.0, 0); // Scaled from -40, 30
        string statusText = "Plots: " + to_string(parcelAnalyzer->getAnalyzedPlots().size()) +
            ", Roads: " + to_string(parcelAnalyzer->getRoadSegments().size());
        for (char c : statusText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
        }

        // Show category counts (scaled positioning)
        glRasterPos3f(-4, 2.5, 0); // Scaled from -40, 25
        auto largePlots = parcelAnalyzer->getPlotsByCategory(LARGE_PARCEL);
        auto mediumPlots = parcelAnalyzer->getPlotsByCategory(MEDIUM_PARCEL);
        auto smallPlots = parcelAnalyzer->getPlotsByCategory(SMALL_PARCEL);

        string categoryText = "Large: " + to_string(largePlots.size()) +
            ", Medium: " + to_string(mediumPlots.size()) +
            ", Small: " + to_string(smallPlots.size());
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
            // Reinitialize analyzer with new data
            delete parcelAnalyzer;
            parcelAnalyzer = new ParcelAnalyzer(siteGen);
        }
        else {
            cout << "Failed to import curves. Make sure 'curves.csv' exists." << endl;
        }
        break;

    case 'b':
        cout << "Generating parcels from imported boundaries..." << endl;
        siteGen->generateParcelsFromBoundaries();
        // Reinitialize analyzer with new parcels
        delete parcelAnalyzer;
        parcelAnalyzer = new ParcelAnalyzer(siteGen);
        break;

    case 'a':
        cout << "Analyzing all parcels..." << endl;
        if (parcelAnalyzer) {
            parcelAnalyzer->analyzeAllParcels();
        }
        break;

    case 's':
        if (siteGen) siteGen->printStatistics();
        if (parcelAnalyzer) parcelAnalyzer->printAnalysisStatistics();
        break;

    case 'r':
        cout << "Resetting system..." << endl;
        delete parcelAnalyzer;
        delete siteGen;
        siteGen = new SiteGeneration();
        parcelAnalyzer = new ParcelAnalyzer(siteGen);
        break;

        // Visualization toggles
    case '1':
        showSiteData = !showSiteData;
        cout << "Site data: " << (showSiteData ? "ON" : "OFF") << endl;
        break;

    case '2':
        showAnalysisResults = !showAnalysisResults;
        cout << "Analysis results: " << (showAnalysisResults ? "ON" : "OFF") << endl;
        break;

    case '3':
        showRoadSegments = !showRoadSegments;
        cout << "Road segments: " << (showRoadSegments ? "ON" : "OFF") << endl;
        break;

    case '4':
        showEdgeConditions = !showEdgeConditions;
        cout << "Edge conditions: " << (showEdgeConditions ? "ON" : "OFF") << endl;
        break;

    case '5':
        showCornerConditions = !showCornerConditions;
        cout << "Corner conditions: " << (showCornerConditions ? "ON" : "OFF") << endl;
        break;

    case '6':
        showRoadFrontages = !showRoadFrontages;
        cout << "Road frontages: " << (showRoadFrontages ? "ON" : "OFF") << endl;
        break;
    }
}

void mousePress(int b, int state, int x, int y) {
    // Mouse interaction can be added here if needed
}

void mouseMotion(int x, int y) {
    // Mouse motion can be added here if needed
}

#endif // _MAIN_