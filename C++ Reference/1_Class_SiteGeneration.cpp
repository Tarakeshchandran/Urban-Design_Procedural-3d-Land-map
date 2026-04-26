#define _MAIN_
#ifdef _MAIN_

#include "main.h"
#include "SiteGeneration.h"  // Include our header file

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
bool showCurves = true;
bool showCenters = true;
bool showParcels = true;

// ==================== ALICE FRAMEWORK FUNCTIONS ====================

void setup() {
    cout << "=== Site-Responsive Building Generation System ===" << endl;
    cout << "Initializing SiteGeneration..." << endl;

    siteGen = new SiteGeneration();

    cout << "\n*** IMPORT WORKFLOW ***" << endl;
    cout << "1. Press 'g' to generate template CSV files" << endl;
    cout << "2. Edit the CSV files with your site data" << endl;
    cout << "3. Press 'c' to import curves (including parcel boundaries)" << endl;
    cout << "4. Press 'b' to generate parcels from imported boundaries" << endl;
    cout << "\n*** VISUALIZATION TOGGLES ***" << endl;
    cout << "- Press '1' to toggle curves" << endl;
    cout << "- Press '2' to toggle parcel centers" << endl;
    cout << "- Press '3' to toggle parcels" << endl;
    cout << "\n*** PARAMETER ADJUSTMENT ***" << endl;
    cout << "- Press '+' to increase road buffer" << endl;
    cout << "- Press '-' to decrease road buffer" << endl;
    cout << "- Press 'q' to increase boundary resolution" << endl;
    cout << "- Press 'w' to decrease boundary resolution" << endl;
    cout << "\n*** OTHER CONTROLS ***" << endl;
    cout << "- Press 's' to show statistics" << endl;
    cout << "- Press 'r' to regenerate parcels" << endl;
    cout << "- Press 'x' to clear all data" << endl;
    cout << "- Press 'z' to reset system" << endl;

    cout << "\n*** RHINO EXPORT GUIDE ***" << endl;
    cout << "To export from Rhino to CSV:" << endl;
    cout << "1. CURVES: Export roads/water/parks/parcels as polylines" << endl;
    cout << "   Format: type,name,width,closed,x1,y1,x2,y2,..." << endl;
    cout << "   Types: major_road, local_road, water, park, site, parcel" << endl;
    cout << "2. Use the provided Rhino script for automatic export" << endl;
    cout << "3. Save as 'curves.csv'" << endl;

    cout << "\nReady for site generation!" << endl;
}

bool compute = false;

void update(int value) {
    // Update logic can be added here
}

void draw() {
    // Clear the background
    backGround(0.95);
    drawGrid(50);

    // Draw site data if available
    if (siteGen) {
        siteGen->drawSite();
    }

    // Draw help text
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos3f(-40, 35, 0);
    string helpText = "Site Generation - Press 'g' for templates, edit curves.csv, then 'c' to import, 'b' for parcels";
    for (char c : helpText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Show current parameters
    if (siteGen) {
        glRasterPos3f(-40, 30, 0);
        string paramText = "Buffer: " + to_string((int)siteGen->getRoadBuffer()) + "m, Resolution: " + to_string(siteGen->getBoundaryResolution());
        for (char c : paramText) {
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
        }
        else {
            cout << "Failed to import curves. Make sure 'curves.csv' exists." << endl;
        }
        break;

    case 'p':
        cout << "Importing parcel centers from CSV (optional)..." << endl;
        if (siteGen->importCentersFromCSV("centers.csv")) {
            cout << "Centers imported successfully!" << endl;
        }
        else {
            cout << "Failed to import centers. This is optional - parcels can be generated from boundaries only." << endl;
        }
        break;

    case 'b':
        cout << "Generating parcels from imported boundaries..." << endl;
        siteGen->generateParcelsFromBoundaries();
        break;

    case 's':
        siteGen->printStatistics();
        break;

    case 'r':
        cout << "Regenerating parcels..." << endl;
        siteGen->regenerateParcels();
        break;

    case 'x':
        cout << "Clearing all data..." << endl;
        siteGen->clearData();
        break;

    case 'z':
        cout << "Resetting system..." << endl;
        delete siteGen;
        siteGen = new SiteGeneration();
        break;

        // Visualization toggles
    case '1':
        showCurves = !showCurves;
        cout << "Curves: " << (showCurves ? "ON" : "OFF") << endl;
        break;

    case '2':
        showCenters = !showCenters;
        cout << "Centers: " << (showCenters ? "ON" : "OFF") << endl;
        break;

    case '3':
        showParcels = !showParcels;
        cout << "Parcels: " << (showParcels ? "ON" : "OFF") << endl;
        break;

        // Parameter adjustments
    case '+':
    case '=':
        siteGen->setRoadBuffer(siteGen->getRoadBuffer() + 1.0f);
        break;

    case '-':
        siteGen->setRoadBuffer(max(0.0f, siteGen->getRoadBuffer() - 1.0f));
        break;

    case 'q':
        siteGen->setBoundaryResolution(siteGen->getBoundaryResolution() + 4);
        break;

    case 'w':
        siteGen->setBoundaryResolution(siteGen->getBoundaryResolution() - 4);
        break;
    }
}

void mousePress(int b, int state, int x, int y) {
    // Mouse interaction can be added here
}

void mouseMotion(int x, int y) {
    // Mouse motion can be added here
}

#endif // _MAIN_