#ifndef SITE_GENERATION_H
#define SITE_GENERATION_H

// Standard Library Includes
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

// Define M_PI if not defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// OpenGL includes
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

using namespace std;

// ===================== ENUMERATIONS =====================
enum CurveType {
    MAJOR_ROAD,
    LOCAL_ROAD,
    WATER_BOUNDARY,
    PARK_BOUNDARY,
    SITE_BOUNDARY,
    PARCEL_BOUNDARY
};

enum ParcelCategory {
    LARGE_PARCEL,   // >= 1500 sqm
    MEDIUM_PARCEL,  // >= 1000 sqm
    SMALL_PARCEL    // >= 250 sqm
};

// ===================== DATA STRUCTURES =====================

// 2D Point structure
struct Point2D {
    float x, y;

    Point2D() : x(0), y(0) {}
    Point2D(float x_, float y_) : x(x_), y(y_) {}

    Point2D operator+(const Point2D& other) const {
        return Point2D(x + other.x, y + other.y);
    }

    Point2D operator-(const Point2D& other) const {
        return Point2D(x - other.x, y - other.y);
    }

    Point2D operator*(float scalar) const {
        return Point2D(x * scalar, y * scalar);
    }

    float distance(const Point2D& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return sqrt(dx * dx + dy * dy);
    }

    float magnitude() const {
        return sqrt(x * x + y * y);
    }

    Point2D normalize() const {
        float mag = magnitude();
        if (mag > 0.001f) {
            return Point2D(x / mag, y / mag);
        }
        return Point2D(0, 0);
    }
};

// Curve structure (polyline)
struct Curve {
    vector<Point2D> points;
    CurveType type;
    bool isClosed;
    float width;
    string name;

    Curve() : type(LOCAL_ROAD), isClosed(false), width(6.0f) {}

    float getLength() const {
        float length = 0;
        for (size_t i = 0; i < points.size() - 1; ++i) {
            length += points[i].distance(points[i + 1]);
        }
        return length;
    }

    Point2D getPointAt(float t) const {
        if (points.empty()) return Point2D();
        if (points.size() == 1) return points[0];

        float totalLength = getLength();
        float targetLength = t * totalLength;
        float currentLength = 0;

        for (size_t i = 0; i < points.size() - 1; ++i) {
            float segmentLength = points[i].distance(points[i + 1]);
            if (currentLength + segmentLength >= targetLength) {
                float segmentT = (targetLength - currentLength) / segmentLength;
                return Point2D(
                    points[i].x + (points[i + 1].x - points[i].x) * segmentT,
                    points[i].y + (points[i + 1].y - points[i].y) * segmentT
                );
            }
            currentLength += segmentLength;
        }

        return points.back();
    }

    Point2D getClosestPoint(const Point2D& point) const {
        if (points.empty()) return Point2D();
        if (points.size() == 1) return points[0];

        Point2D closest = points[0];
        float minDist = point.distance(points[0]);

        for (size_t i = 0; i < points.size() - 1; ++i) {
            Point2D segmentClosest = getClosestPointOnSegment(point, points[i], points[i + 1]);
            float dist = point.distance(segmentClosest);
            if (dist < minDist) {
                minDist = dist;
                closest = segmentClosest;
            }
        }

        return closest;
    }

    float getDistanceToPoint(const Point2D& point) const {
        return point.distance(getClosestPoint(point));
    }

private:
    Point2D getClosestPointOnSegment(const Point2D& point, const Point2D& a, const Point2D& b) const {
        Point2D ab = b - a;
        Point2D ap = point - a;

        float abSquared = ab.x * ab.x + ab.y * ab.y;
        if (abSquared == 0) return a;

        float t = max(0.0f, min(1.0f, (ap.x * ab.x + ap.y * ab.y) / abSquared));
        return a + ab * t;
    }
};

// Parcel structure
struct Parcel {
    int id;
    Point2D center;
    vector<Point2D> boundary;
    float area;
    ParcelCategory category;
    float radius;

    Parcel() : id(-1), area(0), category(SMALL_PARCEL), radius(20.0f) {}

    void calculateArea() {
        area = 0;
        if (boundary.size() < 3) return;

        // Shoelace formula
        for (size_t i = 0; i < boundary.size(); ++i) {
            size_t j = (i + 1) % boundary.size();
            area += boundary[i].x * boundary[j].y;
            area -= boundary[j].x * boundary[i].y;
        }
        area = abs(area) * 0.5f;

        // Categorize based on area
        if (area >= 100.0f) category = LARGE_PARCEL;
        else if (area >= 80.0f) category = MEDIUM_PARCEL;
        else category = SMALL_PARCEL;
    }

    bool containsPoint(const Point2D& point) const {
        if (boundary.size() < 3) return false;

        bool inside = false;
        size_t j = boundary.size() - 1;

        for (size_t i = 0; i < boundary.size(); j = i++) {
            if (((boundary[i].y > point.y) != (boundary[j].y > point.y)) &&
                (point.x < (boundary[j].x - boundary[i].x) * (point.y - boundary[i].y) /
                    (boundary[j].y - boundary[i].y) + boundary[i].x)) {
                inside = !inside;
            }
        }
        return inside;
    }
};
// Calculate point on Catmull-Rom spline
Point2D catmullRomSpline(const Point2D& p0, const Point2D& p1, const Point2D& p2, const Point2D& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    // Catmull-Rom matrix coefficients
    float b0 = 0.5f * (-t3 + 2 * t2 - t);
    float b1 = 0.5f * (3 * t3 - 5 * t2 + 2);
    float b2 = 0.5f * (-3 * t3 + 4 * t2 + t);
    float b3 = 0.5f * (t3 - t2);

    // Calculate interpolated point
    return Point2D(
        b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
        b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y
    );
}

// Generate points along a smooth curve using Catmull-Rom spline
vector<Point2D> generateSmoothCurvePoints(const vector<Point2D>& controlPoints, int pointsPerSegment, bool isClosed) {
    vector<Point2D> smoothPoints;
    if (controlPoints.size() < 2) return controlPoints;

    // For each segment between control points
    for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
        // Get four points needed for the spline (with appropriate handling for endpoints)
        Point2D p0, p1, p2, p3;

        // Set p1 and p2 (the current segment)
        p1 = controlPoints[i];
        p2 = controlPoints[i + 1];

        // Handle p0 (point before segment)
        if (i == 0) {
            if (isClosed) {
                p0 = controlPoints[controlPoints.size() - 1];
            }
            else {
                // Extrapolate first point
                p0 = p1 - (p2 - p1);
            }
        }
        else {
            p0 = controlPoints[i - 1];
        }

        // Handle p3 (point after segment)
        if (i == controlPoints.size() - 2) {
            if (isClosed) {
                p3 = controlPoints[0];
            }
            else {
                // Extrapolate last point
                p3 = p2 + (p2 - p1);
            }
        }
        else {
            p3 = controlPoints[i + 2];
        }

        // First point of segment (except for first segment where it's already added)
        if (i == 0 || smoothPoints.empty()) {
            smoothPoints.push_back(p1);
        }

        // Interpolate points along the segment
        for (int j = 1; j <= pointsPerSegment; ++j) {
            float t = (float)j / pointsPerSegment;
            Point2D interpolated = catmullRomSpline(p0, p1, p2, p3, t);
            smoothPoints.push_back(interpolated);
        }
    }

    // Add closing segment if the curve is closed
    if (isClosed) {
        int last = controlPoints.size() - 1;
        Point2D p0 = controlPoints[last - 1];
        Point2D p1 = controlPoints[last];
        Point2D p2 = controlPoints[0];
        Point2D p3 = controlPoints[1];

        for (int j = 1; j <= pointsPerSegment; ++j) {
            float t = (float)j / pointsPerSegment;
            Point2D interpolated = catmullRomSpline(p0, p1, p2, p3, t);
            smoothPoints.push_back(interpolated);
        }
    }

    return smoothPoints;
}

// ===================== SITE GENERATION CLASS =====================
class SiteGeneration {
private:
    // Imported curves
    vector<Curve> curves;
    vector<Curve> majorRoads;
    vector<Curve> localRoads;
    vector<Curve> waterBoundaries;
    vector<Curve> parkBoundaries;
    vector<Curve> siteBoundaries;
    vector<Curve> parcelBoundaries;

    // Parcel centers (optional) and generated parcels
    vector<Point2D> parcelCenters;
    vector<Parcel> parcels;

    // Generation parameters
    float minParcelRadius;
    float maxParcelRadius;

    // Statistics
    int totalCurvesImported;
    int totalCentersImported;
    int totalParcelsGenerated;
    int smoothnessLevel; // Controls number of interpolation points between control points

    // Auto-scaling parameters
    static constexpr float AUTO_SCALE_FACTOR = 0.1f; // Scale down by factor of 10

public:
    // Make these accessible for parameter adjustment
    float roadBuffer;
    int boundaryResolution;

    // Constructor & Destructor
    SiteGeneration()
        : minParcelRadius(10.0f), maxParcelRadius(100.0f), roadBuffer(5.0f),
        boundaryResolution(100), smoothnessLevel(10), totalCurvesImported(0), totalCentersImported(0),
        totalParcelsGenerated(0) {
        cout << "=== Site Generation Initialized ===" << endl;
    }

    ~SiteGeneration() {
        cout << "=== Site Generation Destroyed ===" << endl;
    }
    void setSmoothness(int level) {
        smoothnessLevel = max(2, min(20, level));
        cout << "Curve smoothness set to: " << smoothnessLevel << " points per segment" << endl;
    }

    int getSmoothness() const {
        return smoothnessLevel;
    }
    // ==================== IMPORT METHODS ====================
    bool importCurvesFromCSV(const string& filename) {
        cout << "\n=== IMPORTING CURVES FROM CSV ===" << endl;
        cout << "File: " << filename << endl;

        ifstream file(filename);
        if (!file.is_open()) {
            cout << "Error: Cannot open file: " << filename << endl;
            return false;
        }

        string line;
        int lineNumber = 0;

        while (getline(file, line)) {
            lineNumber++;
            if (line.empty() || line[0] == '#') continue;

            if (!parseCurveLine(line)) {
                cout << "Warning: Could not parse line " << lineNumber << endl;
            }
        }

        file.close();
        organizeCurvesByType();

        // Automatically scale down all curves by factor of 10
        scaleAllCurves(AUTO_SCALE_FACTOR);

        totalCurvesImported = curves.size();

        cout << "Imported " << totalCurvesImported << " curves (auto-scaled by factor " << AUTO_SCALE_FACTOR << ")" << endl;
        return true;
    }

    bool importCentersFromCSV(const string& filename) {
        cout << "\n=== IMPORTING PARCEL CENTERS FROM CSV ===" << endl;
        cout << "File: " << filename << endl;

        ifstream file(filename);
        if (!file.is_open()) {
            cout << "Error: Cannot open file: " << filename << endl;
            return false;
        }

        string line;
        int lineNumber = 0;
        int validCenters = 0;

        // Clear existing centers
        parcelCenters.clear();

        while (getline(file, line)) {
            lineNumber++;

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Try to parse the line
            if (parseCenterLine(line)) {
                validCenters++;
            }
            else {
                cout << "Warning: Could not parse line " << lineNumber << ": " << line << endl;
            }
        }

        file.close();

        // Automatically scale down centers to match the scaled curves
        scaleAllCenters(AUTO_SCALE_FACTOR);

        totalCentersImported = parcelCenters.size();

        cout << "Successfully imported " << validCenters << " parcel centers (auto-scaled by factor " << AUTO_SCALE_FACTOR << ")" << endl;

        // Print coordinate range for debugging
        if (!parcelCenters.empty()) {
            float minX = parcelCenters[0].x, maxX = parcelCenters[0].x;
            float minY = parcelCenters[0].y, maxY = parcelCenters[0].y;

            for (const auto& center : parcelCenters) {
                minX = min(minX, center.x);
                maxX = max(maxX, center.x);
                minY = min(minY, center.y);
                maxY = max(maxY, center.y);
            }

            cout << "Scaled coordinate range: X[" << minX << " to " << maxX << "], Y[" << minY << " to " << maxY << "]" << endl;
        }

        return validCenters > 0;
    }

    void generateExampleFiles() {
        cout << "\n=== GENERATING EXAMPLE CSV FILES ===" << endl;

        // Generate curves example
        ofstream curvesFile("example_curves.csv");
        curvesFile << "# Curves CSV Format\n";
        curvesFile << "# Format: type,name,width,closed,x1,y1,x2,y2,x3,y3,...\n";
        curvesFile << "# Types: major_road, local_road, water, park, site, parcel\n";
        curvesFile << "# NOTE: Coordinates will be automatically scaled down by factor of 10 after import\n\n";

        curvesFile << "# Example entries (delete these and add your own data)\n";
        curvesFile << "# major_road,MainStreet,12,0,0,1000,2000,1000,4000,800\n";
        curvesFile << "# local_road,Street1,6,0,500,500,1500,600,2500,500\n";
        curvesFile << "# water,River,0,0,0,2000,1000,2200,2000,2000\n";
        curvesFile << "# park,CentralPark,0,1,1500,2500,2500,2500,2500,3500,1500,3500\n";
        curvesFile << "# parcel,Parcel01,0,1,1000,1000,1500,1000,1500,1500,1000,1500\n";
        curvesFile.close();

        // Generate centers example
        ofstream centersFile("example_centers.csv");
        centersFile << "# Parcel Centers CSV Format\n";
        centersFile << "# Format: x,y,radius(optional) OR id,x,y,radius(optional)\n";
        centersFile << "# NOTE: Coordinates will be automatically scaled down by factor of 10 after import\n\n";

        centersFile << "# Example entries (delete these and add your own data)\n";
        centersFile << "# Format 1 (x,y,radius):\n";
        centersFile << "# 755,1253,25\n";
        centersFile << "# 1752,1357,30\n";
        centersFile << "# Format 2 (id,x,y,radius):\n";
        centersFile << "# 0,755,1253,25\n";
        centersFile << "# 1,1752,1357,30\n";
        centersFile.close();

        cout << "Generated example_curves.csv and example_centers.csv" << endl;
        cout << "These are TEMPLATE files - replace with your own data!" << endl;
        cout << "File locations:" << endl;
        cout << "- example_curves.csv (for roads, water, parks, parcels)" << endl;
        cout << "- example_centers.csv (for parcel centers - optional)" << endl;
        cout << "NOTE: All imported coordinates will be automatically scaled down by factor of 10" << endl;
    }

    // ==================== PARCEL GENERATION METHODS ====================
    void generateParcelsFromBoundaries() {
        cout << "\n=== GENERATING PARCELS FROM IMPORTED BOUNDARIES ===" << endl;

        parcels.clear();

        for (size_t i = 0; i < parcelBoundaries.size(); ++i) {
            const auto& boundary = parcelBoundaries[i];

            if (!boundary.isClosed) {
                cout << "Warning: Parcel boundary " << i << " is not closed, skipping." << endl;
                continue;
            }

            if (boundary.points.size() < 3) {
                cout << "Warning: Parcel boundary " << i << " has too few points, skipping." << endl;
                continue;
            }

            Parcel parcel;
            parcel.id = i;
            parcel.boundary = boundary.points;

            // Calculate centroid
            calculateParcelCentroid(parcel);

            // Calculate area and category
            parcel.calculateArea();

            parcels.push_back(parcel);
        }

        totalParcelsGenerated = parcels.size();
        cout << "Generated " << totalParcelsGenerated << " parcels from " << parcelBoundaries.size() << " boundaries" << endl;
        classifyParcels();
    }

    void calculateParcelCentroid(Parcel& parcel) {
        if (parcel.boundary.empty()) return;

        float cx = 0, cy = 0;
        float signedArea = 0;

        for (size_t i = 0; i < parcel.boundary.size(); ++i) {
            size_t j = (i + 1) % parcel.boundary.size();
            float a = parcel.boundary[i].x * parcel.boundary[j].y - parcel.boundary[j].x * parcel.boundary[i].y;
            signedArea += a;
            cx += (parcel.boundary[i].x + parcel.boundary[j].x) * a;
            cy += (parcel.boundary[i].y + parcel.boundary[j].y) * a;
        }

        signedArea *= 0.5f;
        if (abs(signedArea) > 0.001f) {
            cx /= (6.0f * signedArea);
            cy /= (6.0f * signedArea);
            parcel.center = Point2D(cx, cy);
        }
        else {
            // Fallback to simple average if signed area is too small
            float avgX = 0, avgY = 0;
            for (const auto& pt : parcel.boundary) {
                avgX += pt.x;
                avgY += pt.y;
            }
            parcel.center = Point2D(avgX / parcel.boundary.size(), avgY / parcel.boundary.size());
        }
    }

    // ==================== SCALING METHODS ====================
    void scaleAllCurves(float scaleFactor) {
        cout << "Scaling all curves by factor: " << scaleFactor << endl;

        // Scale all curves in the main curves vector
        for (auto& curve : curves) {
            for (auto& point : curve.points) {
                point.x *= scaleFactor;
                point.y *= scaleFactor;
            }
            // Also scale the width proportionally
            curve.width *= scaleFactor;
        }

        // Re-organize curves by type after scaling
        organizeCurvesByType();

        cout << "Curves scaled successfully" << endl;
    }

    void scaleAllCenters(float scaleFactor) {
        cout << "Scaling all parcel centers by factor: " << scaleFactor << endl;

        // Scale all parcel centers
        for (auto& center : parcelCenters) {
            center.x *= scaleFactor;
            center.y *= scaleFactor;
        }

        cout << "Parcel centers scaled successfully" << endl;
    }

    void scaleAllParcels(float scaleFactor) {
        cout << "Scaling all parcels by factor: " << scaleFactor << endl;

        // Scale all parcels
        for (auto& parcel : parcels) {
            // Scale boundary points
            for (auto& point : parcel.boundary) {
                point.x *= scaleFactor;
                point.y *= scaleFactor;
            }

            // Scale center point
            parcel.center.x *= scaleFactor;
            parcel.center.y *= scaleFactor;

            // Scale radius
            parcel.radius *= scaleFactor;

            // Recalculate area (area scales by factor squared)
            parcel.calculateArea();
        }

        cout << "Parcels scaled successfully" << endl;
    }

    // ==================== HELPER METHODS ====================
    bool parseCurveLine(const string& line) {
        stringstream ss(line);
        string item;
        vector<string> tokens;

        while (getline(ss, item, ',')) {
            tokens.push_back(item);
        }

        if (tokens.size() < 7) return false; // Minimum: type,name,width,closed,x1,y1,x2,y2

        Curve curve;

        // Parse type
        string typeStr = tokens[0];
        if (typeStr == "major_road") curve.type = MAJOR_ROAD;
        else if (typeStr == "local_road") curve.type = LOCAL_ROAD;
        else if (typeStr == "water") curve.type = WATER_BOUNDARY;
        else if (typeStr == "park") curve.type = PARK_BOUNDARY;
        else if (typeStr == "site") curve.type = SITE_BOUNDARY;
        else if (typeStr == "parcel") curve.type = PARCEL_BOUNDARY;
        else return false;

        curve.name = tokens[1];
        curve.width = stof(tokens[2]);
        curve.isClosed = (tokens[3] == "1");

        // Parse points
        for (size_t i = 4; i < tokens.size() - 1; i += 2) {
            if (i + 1 < tokens.size()) {
                Point2D point(stof(tokens[i]), stof(tokens[i + 1]));
                curve.points.push_back(point);
            }
        }

        if (curve.points.size() >= 2) {
            curves.push_back(curve);
            return true;
        }

        return false;
    }

    bool parseCenterLine(const string& line) {
        stringstream ss(line);
        string item;
        vector<string> tokens;

        while (getline(ss, item, ',')) {
            tokens.push_back(item);
        }

        // Handle both formats:
        // Format 1: id,x,y,radius (4 tokens)
        // Format 2: x,y,radius (3 tokens - like your file)

        if (tokens.size() >= 2) {
            try {
                Point2D center;

                if (tokens.size() >= 3) {
                    // Format: x,y,radius (your format)
                    center.x = stof(tokens[0]);
                    center.y = stof(tokens[1]);
                    // Radius is in tokens[2] but we calculate it automatically
                }
                else if (tokens.size() >= 2) {
                    // Format: x,y
                    center.x = stof(tokens[0]);
                    center.y = stof(tokens[1]);
                }

                parcelCenters.push_back(center);
                return true;
            }
            catch (const exception& e) {
                cout << "Error parsing center: " << line << " - " << e.what() << endl;
                return false;
            }
        }

        return false;
    }

    void organizeCurvesByType() {
        majorRoads.clear();
        localRoads.clear();
        waterBoundaries.clear();
        parkBoundaries.clear();
        siteBoundaries.clear();
        parcelBoundaries.clear();

        for (const auto& curve : curves) {
            switch (curve.type) {
            case MAJOR_ROAD:
                majorRoads.push_back(curve);
                break;
            case LOCAL_ROAD:
                localRoads.push_back(curve);
                break;
            case WATER_BOUNDARY:
                waterBoundaries.push_back(curve);
                break;
            case PARK_BOUNDARY:
                parkBoundaries.push_back(curve);
                break;
            case SITE_BOUNDARY:
                siteBoundaries.push_back(curve);
                break;
            case PARCEL_BOUNDARY:
                parcelBoundaries.push_back(curve);
                break;
            }
        }

        cout << "Organized curves: " << majorRoads.size() << " major roads, "
            << localRoads.size() << " local roads, " << waterBoundaries.size() << " water, "
            << parkBoundaries.size() << " parks, " << siteBoundaries.size() << " site, "
            << parcelBoundaries.size() << " parcels" << endl;
    }

    void classifyParcels() {
        int large = 0, medium = 0, small = 0;

        for (const auto& parcel : parcels) {
            switch (parcel.category) {
            case LARGE_PARCEL: large++; break;
            case MEDIUM_PARCEL: medium++; break;
            case SMALL_PARCEL: small++; break;
            }
        }

        cout << "Parcel classification:" << endl;
        cout << "  Large (≥1500m²): " << large << endl;
        cout << "  Medium (≥1000m²): " << medium << endl;
        cout << "  Small (<1000m²): " << small << endl;
    }

    // ==================== ACCESSOR METHODS ====================
    const vector<Curve>& getCurves() const { return curves; }
    const vector<Curve>& getMajorRoads() const { return majorRoads; }
    const vector<Curve>& getLocalRoads() const { return localRoads; }
    const vector<Curve>& getWaterBoundaries() const { return waterBoundaries; }
    const vector<Curve>& getParkBoundaries() const { return parkBoundaries; }
    const vector<Curve>& getSiteBoundaries() const { return siteBoundaries; }
    const vector<Curve>& getParcelBoundaries() const { return parcelBoundaries; }
    const vector<Point2D>& getParcelCenters() const { return parcelCenters; }
    const vector<Parcel>& getParcels() const { return parcels; }

    int getTotalCurves() const { return totalCurvesImported; }
    int getTotalCenters() const { return totalCentersImported; }
    int getTotalParcels() const { return totalParcelsGenerated; }

    float getRoadBuffer() const { return roadBuffer; }
    int getBoundaryResolution() const { return boundaryResolution; }

    // Get the auto-scale factor being used
    float getAutoScaleFactor() const { return AUTO_SCALE_FACTOR; }

    // ==================== VISUALIZATION METHODS ====================
    void drawSite() {
        drawCurves();
        drawParcelCenters();
        drawParcels();
    }

    void drawCurves() {
        // Draw major roads
        glColor3f(1.0f, 0.0f, 0.0f);
        glLineWidth(4.0f);
        for (const auto& road : majorRoads) {
            drawSmoothCurve(road);
        }

        // Draw local roads
        glColor3f(0.5f, 0.5f, 0.5f);
        glLineWidth(2.0f);
        for (const auto& road : localRoads) {
            drawSmoothCurve(road);
        }

        // Draw water boundaries
        glColor3f(0.0f, 0.7f, 1.0f); // Cyan-blue for water
        glLineWidth(3.0f);
        for (const auto& water : waterBoundaries) {
            drawSmoothCurve(water);
        }

        // Draw park boundaries
        glColor3f(0.0f, 0.8f, 0.0f);
        glLineWidth(3.0f);
        for (const auto& park : parkBoundaries) {
            drawSmoothCurve(park);
        }

        // Draw site boundaries
        glColor3f(0.0f, 0.0f, 1.0f); // Blue for site boundaries
        glLineWidth(3.0f);
        for (const auto& site : siteBoundaries) {
            drawSmoothCurve(site);
        }

        // Draw parcel boundaries
        glColor3f(0.7f, 0.0f, 0.7f);  // Purple for parcels
        glLineWidth(2.0f);
        for (const auto& parcel : parcelBoundaries) {
            drawSmoothCurve(parcel);
        }

        glLineWidth(1.0f);
    }

    void drawCurve(const Curve& curve) {
        if (curve.points.empty()) return;

        if (curve.isClosed) {
            glBegin(GL_LINE_LOOP);
        }
        else {
            glBegin(GL_LINE_STRIP);
        }

        for (const auto& point : curve.points) {
            glVertex3f(point.x, point.y, 0.0f);
        }

        glEnd();
    }

    void drawParcelCenters() {
        glColor3f(1.0f, 0.0f, 1.0f);
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        for (const auto& center : parcelCenters) {
            glVertex3f(center.x, center.y, 0.1f);
        }
        glEnd();

        // Draw center IDs
        glColor3f(0.0f, 0.0f, 0.0f);
        for (size_t i = 0; i < parcelCenters.size(); ++i) {
            glRasterPos3f(parcelCenters[i].x + 5, parcelCenters[i].y + 5, 0.1f);
            string idStr = to_string(i);
            for (char c : idStr) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
            }
        }

        glPointSize(1.0f);
    }

    void drawSmoothCurve(const Curve& curve) {
        if (curve.points.empty()) return;

        // Determine how many interpolation points to use based on curve type
        int pointsPerSegment = smoothnessLevel;

        // Use more interpolation points for site and water boundaries to make them smoother
        if (curve.type == SITE_BOUNDARY || curve.type == WATER_BOUNDARY) {
            pointsPerSegment = smoothnessLevel * 2;
        }

        // Generate smooth points using spline interpolation
        vector<Point2D> smoothPoints = generateSmoothCurvePoints(curve.points, pointsPerSegment, curve.isClosed);

        if (curve.isClosed) {
            glBegin(GL_LINE_LOOP);
        }
        else {
            glBegin(GL_LINE_STRIP);
        }

        for (const auto& point : smoothPoints) {
            glVertex3f(point.x, point.y, 0.0f);
        }

        glEnd();
    }

    void drawParcels() {
        for (const auto& parcel : parcels) {
            // Color code by category
            switch (parcel.category) {
            case LARGE_PARCEL:
                glColor4f(0.0f, 1.0f, 0.0f, 0.3f); // Green
                break;
            case MEDIUM_PARCEL:
                glColor4f(1.0f, 1.0f, 0.0f, 0.3f); // Yellow
                break;
            case SMALL_PARCEL:
                glColor4f(1.0f, 0.0f, 0.0f, 0.3f); // Red
                break;
            }

            // Draw filled parcel
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (parcel.boundary.size() >= 3) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(parcel.center.x, parcel.center.y, 0.01f);
                for (const auto& pt : parcel.boundary) {
                    glVertex3f(pt.x, pt.y, 0.01f);
                }
                glVertex3f(parcel.boundary[0].x, parcel.boundary[0].y, 0.01f);
                glEnd();
            }

            glDisable(GL_BLEND);

            // Draw parcel boundary
            glColor3f(0.3f, 0.3f, 0.3f);
            glLineWidth(1.0f);
            glBegin(GL_LINE_LOOP);
            for (const auto& pt : parcel.boundary) {
                glVertex3f(pt.x, pt.y, 0.02f);
            }
            glEnd();

            // Draw parcel ID
            glColor3f(0.0f, 0.0f, 0.0f);
            glRasterPos3f(parcel.center.x - 5, parcel.center.y - 5, 0.1f);
            string idStr = to_string(parcel.id);
            for (char c : idStr) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
            }
        }
    }

    // ==================== UTILITY METHODS ====================
    void clearData() {
        curves.clear();
        majorRoads.clear();
        localRoads.clear();
        waterBoundaries.clear();
        parkBoundaries.clear();
        siteBoundaries.clear();
        parcelBoundaries.clear();
        parcelCenters.clear();
        parcels.clear();

        totalCurvesImported = 0;
        totalCentersImported = 0;
        totalParcelsGenerated = 0;

        cout << "Site data cleared" << endl;
    }

    void printStatistics() {
        cout << "\n=== SITE GENERATION STATISTICS ===" << endl;
        cout << "Auto-scale factor: " << AUTO_SCALE_FACTOR << " (coordinates scaled down by factor of 10)" << endl;
        cout << "Curves imported: " << totalCurvesImported << endl;
        cout << "Curve smoothness level: " << smoothnessLevel << endl;
        cout << "  Major roads: " << majorRoads.size() << endl;
        cout << "  Local roads: " << localRoads.size() << endl;
        cout << "  Water boundaries: " << waterBoundaries.size() << endl;
        cout << "  Park boundaries: " << parkBoundaries.size() << endl;
        cout << "  Site boundaries: " << siteBoundaries.size() << endl;
        cout << "  Parcel boundaries: " << parcelBoundaries.size() << endl;
        cout << "Parcel centers imported: " << totalCentersImported << endl;
        cout << "Parcels generated: " << totalParcelsGenerated << endl;


        // Calculate total areas
        float totalArea = 0;
        for (const auto& parcel : parcels) {
            totalArea += parcel.area;
        }
        cout << "Total parcel area: " << totalArea << " m²" << endl;
        cout << "=================================" << endl;
    }

    // ==================== PARAMETER ADJUSTMENT ====================
    void setMinParcelRadius(float radius) {
        minParcelRadius = radius;
        cout << "Min parcel radius set to: " << radius << "m" << endl;
    }

    void setMaxParcelRadius(float radius) {
        maxParcelRadius = radius;
        cout << "Max parcel radius set to: " << radius << "m" << endl;
    }

    void setRoadBuffer(float buffer) {
        roadBuffer = buffer;
        cout << "Road buffer set to: " << buffer << "m" << endl;
    }

    void setBoundaryResolution(int resolution) {
        boundaryResolution = max(8, min(128, resolution));
        cout << "Boundary resolution set to: " << boundaryResolution << " rays" << endl;
    }

    void regenerateParcels() {
        cout << "Regenerating parcels with current parameters..." << endl;
        generateParcelsFromBoundaries();
    }

    // Manual scaling methods (for additional flexibility)
    void manualScaleAllData(float scaleFactor) {
        scaleAllCurves(scaleFactor);
        scaleAllCenters(scaleFactor);
        scaleAllParcels(scaleFactor);

        // Update road buffer proportionally
        roadBuffer *= scaleFactor;

        cout << "All site data manually scaled by factor: " << scaleFactor << endl;
    }
};

#endif // SITE_GENERATION_H