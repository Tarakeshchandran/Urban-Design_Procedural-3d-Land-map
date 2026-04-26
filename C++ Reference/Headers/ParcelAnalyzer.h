#ifndef PARCEL_ANALYZER_H
#define PARCEL_ANALYZER_H

// Include the existing SiteGeneration header
#include "SiteGeneration.h"

// Standard Library Includes
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <cmath>

// OpenGL includes
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

using namespace std;

// ===================== ENHANCED DATA STRUCTURES =====================

// Road hierarchy for better classification
enum class RoadHierarchy {
    MAJOR_ARTERIAL,   // Major roads (thick red lines)
    MINOR_ARTERIAL,
    COLLECTOR,
    LOCAL_STREET
};

// Edge condition types (renamed to avoid conflicts)
enum class SiteEdgeType {
    STREET_EDGE,
    WATER_EDGE,
    PARK_EDGE,
    INTERNAL_EDGE,
    MIXED_EDGE        // Multiple edge types
};

// Corner condition types (renamed to avoid conflicts with Eigen::CornerType)
enum class SiteCornerType {
    STREET_CORNER,
    PLAZA_CORNER,
    PARK_CORNER,
    WATER_CORNER,
    MIXED_CORNER,     // Multiple corner types
    NO_CORNER
};

// Road segment structure for detailed road analysis
struct RoadSegment {
    Point2D start, end;
    Point2D direction;
    float width;
    RoadHierarchy hierarchy;
    string name;
    int curveId;      // Reference to source curve

    RoadSegment() : width(6.0f), hierarchy(RoadHierarchy::LOCAL_STREET), curveId(-1) {}

    float getLength() const {
        return start.distance(end);
    }

    Point2D getMidpoint() const {
        return Point2D((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
    }

    // Get distance from point to road segment
    float getDistanceToPoint(const Point2D& point) const {
        // Vector from start to end
        Point2D ab = end - start;
        // Vector from start to point
        Point2D ap = point - start;

        float abSquared = ab.x * ab.x + ab.y * ab.y;
        if (abSquared == 0) return point.distance(start);

        // Project point onto line, clamped to segment
        float t = max(0.0f, min(1.0f, (ap.x * ab.x + ap.y * ab.y) / abSquared));
        Point2D projection = start + ab * t;

        return point.distance(projection);
    }
};

// Road information for parcel analysis
struct RoadInfo {
    RoadSegment segment;
    float distanceToParcel;
    float frontageLength;    // Length of parcel edge facing this road
    bool isPrimaryFrontage;  // Is this the main street frontage?

    RoadInfo() : distanceToParcel(0), frontageLength(0), isPrimaryFrontage(false) {}
};

// Enhanced Plot structure
struct AnalyzedPlot {
    int id;
    int parentParcelId;
    vector<Point2D> boundary;
    Point2D center;
    float area;
    ParcelCategory category;  // Using existing enum from SiteGeneration.h
    vector<RoadInfo> adjacentRoads;
    float setbackDistance;
    SiteEdgeType edgeCondition;
    SiteCornerType cornerCondition;

    // Additional analysis data
    float perimeterLength;
    float aspectRatio;
    Point2D primaryStreetFrontage;
    float streetFrontageLength;

    AnalyzedPlot() : id(-1), parentParcelId(-1), area(0), category(SMALL_PARCEL),
        setbackDistance(5.0f), edgeCondition(SiteEdgeType::INTERNAL_EDGE),
        cornerCondition(SiteCornerType::NO_CORNER), perimeterLength(0), aspectRatio(1.0f),
        streetFrontageLength(0) {
    }

    void calculateBasicMetrics() {
        // Calculate area using shoelace formula
        area = 0;
        if (boundary.size() < 3) return;

        for (size_t i = 0; i < boundary.size(); ++i) {
            size_t j = (i + 1) % boundary.size();
            area += boundary[i].x * boundary[j].y;
            area -= boundary[j].x * boundary[i].y;
        }
        area = abs(area) * 0.5f;

        // Calculate perimeter
        perimeterLength = 0;
        for (size_t i = 0; i < boundary.size(); ++i) {
            size_t j = (i + 1) % boundary.size();
            perimeterLength += boundary[i].distance(boundary[j]);
        }

        // Calculate centroid
        float cx = 0, cy = 0;
        for (const auto& pt : boundary) {
            cx += pt.x;
            cy += pt.y;
        }
        center = Point2D(cx / boundary.size(), cy / boundary.size());

        // Calculate aspect ratio (rough approximation)
        if (boundary.size() >= 3) {
            float minX = boundary[0].x, maxX = boundary[0].x;
            float minY = boundary[0].y, maxY = boundary[0].y;

            for (const auto& pt : boundary) {
                minX = min(minX, pt.x);
                maxX = max(maxX, pt.x);
                minY = min(minY, pt.y);
                maxY = max(maxY, pt.y);
            }

            float width = maxX - minX;
            float height = maxY - minY;
            aspectRatio = width / max(height, 0.1f);
        }
    }
};

// ===================== PARCEL ANALYZER CLASS =====================

class ParcelAnalyzer {
private:
    SiteGeneration* siteGen;
    vector<RoadSegment> roadSegments;
    vector<AnalyzedPlot> analyzedPlots;

    // Distance thresholds for edge detection (scaled by 0.1)
    static constexpr float WATER_EDGE_THRESHOLD = 5.0f;
    static constexpr float PARK_EDGE_THRESHOLD = 3.0f;
    static constexpr float ROAD_ADJACENCY_THRESHOLD = 1.5f;
    static constexpr float CORNER_DETECTION_ANGLE = 45.0f; // degrees

public:
    // ================== CONSTRUCTOR & DESTRUCTOR ==================

    ParcelAnalyzer(SiteGeneration* siteGeneration) : siteGen(siteGeneration) {
        cout << "=== ParcelAnalyzer Initialized ===" << endl;
        if (siteGen) {
            buildRoadSegments();
            convertParcelsToPlots();
        }
    }

    ~ParcelAnalyzer() {
        cout << "=== ParcelAnalyzer Destroyed ===" << endl;
    }

    // ================== MAIN ANALYSIS METHODS ==================

    // Classify parcel by size with updated thresholds (using existing enum but new logic)
    ParcelCategory classifyParcelBySize(const Parcel& parcel) {
        if (parcel.area >= 100.0f) {
            return LARGE_PARCEL;   // ≥15,000m² -> Large (4x subdivision, low density)
        }
        else if (parcel.area >= 80.0f) {
            return MEDIUM_PARCEL;  // ≥10,000m² -> Medium (2x subdivision, medium density)
        }
        else if (parcel.area >= 25.0f) {
            return SMALL_PARCEL;   // ≥2,500m² -> Small (1x/no subdivision, high density)
        }
        else {
            return SMALL_PARCEL;   // <2,500m² -> Too small, treat as small
        }
    }

    // Classify parcel by size (AnalyzedPlot version)
    ParcelCategory classifyParcelBySize(const AnalyzedPlot& plot) {
        if (plot.area >= 100.0f) {
            return LARGE_PARCEL;
        }
        else if (plot.area >= 80.0f) {
            return MEDIUM_PARCEL;
        }
        else if (plot.area >= 25.0f) {
            return SMALL_PARCEL;
        }
        else {
            return SMALL_PARCEL;
        }
    }

    // Detect edge conditions for a parcel
    SiteEdgeType detectEdgeConditions(const Parcel& parcel) {
        float distanceToWater = getDistanceToWater(parcel);
        float distanceToPark = getDistanceToPark(parcel);
        float distanceToRoad = getDistanceToMajorRoad(parcel);

        vector<SiteEdgeType> detectedEdges;

        if (distanceToWater < WATER_EDGE_THRESHOLD) {
            detectedEdges.push_back(SiteEdgeType::WATER_EDGE);
        }

        if (distanceToPark < PARK_EDGE_THRESHOLD) {
            detectedEdges.push_back(SiteEdgeType::PARK_EDGE);
        }

        if (distanceToRoad < ROAD_ADJACENCY_THRESHOLD) {
            detectedEdges.push_back(SiteEdgeType::STREET_EDGE);
        }

        // Return the most prominent edge type
        if (detectedEdges.empty()) {
            return SiteEdgeType::INTERNAL_EDGE;
        }
        else if (detectedEdges.size() == 1) {
            return detectedEdges[0];
        }
        else {
            return SiteEdgeType::MIXED_EDGE;
        }
    }

    // Detect corner conditions for a parcel
    SiteCornerType detectCornerConditions(const Parcel& parcel) {
        vector<SiteCornerType> detectedCorners;

        // Check proximity to different features
        float distanceToWater = getDistanceToWater(parcel);
        float distanceToPark = getDistanceToPark(parcel);

        // Check for major road intersections near parcel corners
        bool nearRoadIntersection = isNearRoadIntersection(parcel);

        if (nearRoadIntersection) {
            detectedCorners.push_back(SiteCornerType::STREET_CORNER);
        }

        if (distanceToWater < WATER_EDGE_THRESHOLD) {
            detectedCorners.push_back(SiteCornerType::WATER_CORNER);
        }

        if (distanceToPark < PARK_EDGE_THRESHOLD) {
            // Check if it's a plaza (smaller) or park (larger)
            if (isNearPlaza(parcel)) {
                detectedCorners.push_back(SiteCornerType::PLAZA_CORNER);
            }
            else {
                detectedCorners.push_back(SiteCornerType::PARK_CORNER);
            }
        }

        // Return the most prominent corner type
        if (detectedCorners.empty()) {
            return SiteCornerType::NO_CORNER;
        }
        else if (detectedCorners.size() == 1) {
            return detectedCorners[0];
        }
        else {
            return SiteCornerType::MIXED_CORNER;
        }
    }

    // Get adjacent roads for a parcel
    vector<RoadInfo> getAdjacentRoads(const Parcel& parcel) {
        vector<RoadInfo> adjacentRoads;

        for (const auto& roadSeg : roadSegments) {
            float distance = getDistanceFromParcelToRoad(parcel, roadSeg);

            if (distance < ROAD_ADJACENCY_THRESHOLD) {
                RoadInfo roadInfo;
                roadInfo.segment = roadSeg;
                roadInfo.distanceToParcel = distance;
                roadInfo.frontageLength = calculateFrontageLength(parcel, roadSeg);

                adjacentRoads.push_back(roadInfo);
            }
        }

        // Sort by distance and frontage length to identify primary roads
        sort(adjacentRoads.begin(), adjacentRoads.end(),
            [](const RoadInfo& a, const RoadInfo& b) {
                // Prioritize major roads, then frontage length, then proximity
                if (a.segment.hierarchy != b.segment.hierarchy) {
                    return a.segment.hierarchy < b.segment.hierarchy;
                }
                if (abs(a.frontageLength - b.frontageLength) > 5.0f) {
                    return a.frontageLength > b.frontageLength;
                }
                return a.distanceToParcel < b.distanceToParcel;
            });

        // Mark primary frontage
        if (!adjacentRoads.empty()) {
            adjacentRoads[0].isPrimaryFrontage = true;
        }

        return adjacentRoads;
    }

    // Get distance to water features
    float getDistanceToWater(const Parcel& parcel) {
        if (!siteGen) return 1000.0f;

        const auto& waterBoundaries = siteGen->getWaterBoundaries();
        float minDistance = 1000.0f;

        for (const auto& water : waterBoundaries) {
            float distance = water.getDistanceToPoint(parcel.center);
            minDistance = min(minDistance, distance);
        }

        return minDistance;
    }

    // Get distance to park features
    float getDistanceToPark(const Parcel& parcel) {
        if (!siteGen) return 1000.0f;

        const auto& parkBoundaries = siteGen->getParkBoundaries();
        float minDistance = 1000.0f;

        for (const auto& park : parkBoundaries) {
            float distance = park.getDistanceToPoint(parcel.center);
            minDistance = min(minDistance, distance);
        }

        return minDistance;
    }

    // Get dominant road angle for subdivision alignment
    float getDominantRoadAngle(const Parcel& parcel) {
        vector<RoadInfo> adjacentRoads = getAdjacentRoads(parcel);

        if (adjacentRoads.empty()) {
            return 0.0f; // Default north-south orientation
        }

        // Use the primary road's direction
        const RoadSegment& primaryRoad = adjacentRoads[0].segment;

        // Calculate angle of road direction
        float dx = primaryRoad.direction.x;
        float dy = primaryRoad.direction.y;

        float angle = atan2(dy, dx) * 180.0f / M_PI;

        // Normalize to 0-180 range (since buildings can face either direction)
        while (angle < 0) angle += 180.0f;
        while (angle >= 180.0f) angle -= 180.0f;

        return angle;
    }

    // Get primary street frontage information
    Point2D getPrimaryStreetFrontage(const Parcel& parcel) {
        vector<RoadInfo> adjacentRoads = getAdjacentRoads(parcel);

        if (adjacentRoads.empty()) {
            return parcel.center; // Default to parcel center
        }

        // Return the midpoint of the segment closest to the primary road
        const RoadSegment& primaryRoad = adjacentRoads[0].segment;
        Point2D closestPoint = getClosestPointOnParcelToRoad(parcel, primaryRoad);

        return closestPoint;
    }

    // ================== ACCESSOR METHODS ==================

    const vector<RoadSegment>& getRoadSegments() const { return roadSegments; }
    const vector<AnalyzedPlot>& getAnalyzedPlots() const { return analyzedPlots; }

    // Get analyzed plot by ID
    const AnalyzedPlot* getAnalyzedPlot(int plotId) const {
        for (const auto& plot : analyzedPlots) {
            if (plot.id == plotId) {
                return &plot;
            }
        }
        return nullptr;
    }

    // Get plots by category
    vector<AnalyzedPlot> getPlotsByCategory(ParcelCategory category) const {
        vector<AnalyzedPlot> result;
        for (const auto& plot : analyzedPlots) {
            if (plot.category == category) {
                result.push_back(plot);
            }
        }
        return result;
    }

    // Get plots by edge condition
    vector<AnalyzedPlot> getPlotsByEdgeCondition(SiteEdgeType edgeType) const {
        vector<AnalyzedPlot> result;
        for (const auto& plot : analyzedPlots) {
            if (plot.edgeCondition == edgeType) {
                result.push_back(plot);
            }
        }
        return result;
    }

    // Get plots by corner condition
    vector<AnalyzedPlot> getPlotsByCornerCondition(SiteCornerType cornerType) const {
        vector<AnalyzedPlot> result;
        for (const auto& plot : analyzedPlots) {
            if (plot.cornerCondition == cornerType) {
                result.push_back(plot);
            }
        }
        return result;
    }

    // Convert all parcels to plots with analysis
    void analyzeAllParcels() {
        cout << "\n=== ANALYZING ALL PARCELS ===" << endl;

        if (!siteGen) {
            cout << "Error: SiteGeneration not initialized" << endl;
            return;
        }

        const auto& parcels = siteGen->getParcels();
        analyzedPlots.clear();

        for (size_t i = 0; i < parcels.size(); ++i) {
            const auto& parcel = parcels[i];

            AnalyzedPlot plot;
            plot.id = i;
            plot.parentParcelId = parcel.id;
            plot.boundary = parcel.boundary;
            plot.calculateBasicMetrics();

            // Perform analysis
            plot.category = classifyParcelBySize(parcel);
            plot.edgeCondition = detectEdgeConditions(parcel);
            plot.cornerCondition = detectCornerConditions(parcel);
            plot.adjacentRoads = getAdjacentRoads(parcel);

            // Calculate additional metrics
            plot.primaryStreetFrontage = getPrimaryStreetFrontage(parcel);
            plot.streetFrontageLength = calculateTotalStreetFrontage(parcel);
            plot.setbackDistance = calculateRequiredSetback(plot);

            analyzedPlots.push_back(plot);

            cout << "Plot " << i << ": " << getParcelCategoryName(plot.category)
                << ", Area: " << plot.area << "m², Edge: " << getSiteEdgeTypeName(plot.edgeCondition)
                << ", Corner: " << getSiteCornerTypeName(plot.cornerCondition) << endl;
        }

        cout << "Analyzed " << analyzedPlots.size() << " parcels" << endl;
        printAnalysisStatistics();
    }

    // Print detailed analysis statistics
    void printAnalysisStatistics() {
        cout << "\n=== PARCEL ANALYSIS STATISTICS ===" << endl;

        // Count by category
        int largeCount = 0, mediumCount = 0, smallCount = 0;
        for (const auto& plot : analyzedPlots) {
            switch (plot.category) {
            case LARGE_PARCEL: largeCount++; break;
            case MEDIUM_PARCEL: mediumCount++; break;
            case SMALL_PARCEL: smallCount++; break;
            }
        }

        cout << "Parcel Categories:" << endl;
        cout << "  Large (≥15,000m²): " << largeCount << " parcels" << endl;
        cout << "  Medium (≥10,000m²): " << mediumCount << " parcels" << endl;
        cout << "  Small (≥2,500m²): " << smallCount << " parcels" << endl;

        // Count by edge conditions
        map<SiteEdgeType, int> edgeCounts;
        for (const auto& plot : analyzedPlots) {
            edgeCounts[plot.edgeCondition]++;
        }

        cout << "Edge Conditions:" << endl;
        for (const auto& pair : edgeCounts) {
            cout << "  " << getSiteEdgeTypeName(pair.first) << ": " << pair.second << " parcels" << endl;
        }

        // Count by corner conditions
        map<SiteCornerType, int> cornerCounts;
        for (const auto& plot : analyzedPlots) {
            cornerCounts[plot.cornerCondition]++;
        }

        cout << "Corner Conditions:" << endl;
        for (const auto& pair : cornerCounts) {
            cout << "  " << getSiteCornerTypeName(pair.first) << ": " << pair.second << " parcels" << endl;
        }

        // Road access statistics
        int roadAccessibleParcels = 0;
        int majorRoadAdjacentParcels = 0;
        for (const auto& plot : analyzedPlots) {
            if (!plot.adjacentRoads.empty()) {
                roadAccessibleParcels++;
                for (const auto& road : plot.adjacentRoads) {
                    if (road.segment.hierarchy == RoadHierarchy::MAJOR_ARTERIAL) {
                        majorRoadAdjacentParcels++;
                        break;
                    }
                }
            }
        }

        cout << "Road Access:" << endl;
        cout << "  Road accessible parcels: " << roadAccessibleParcels << "/" << analyzedPlots.size() << endl;
        cout << "  Major road adjacent: " << majorRoadAdjacentParcels << "/" << analyzedPlots.size() << endl;
        cout << "=================================" << endl;
    }

    // ================== VISUALIZATION METHODS ==================

    void drawAnalysisResults() {
        drawPlotsByCategory();
    }

    void drawPlotsByCategory() {
        for (const auto& plot : analyzedPlots) {
            // Color code by category
            switch (plot.category) {
            case LARGE_PARCEL:
                glColor4f(0.0f, 1.0f, 0.0f, 0.4f); // Green
                break;
            case MEDIUM_PARCEL:
                glColor4f(1.0f, 1.0f, 0.0f, 0.4f); // Yellow
                break;
            case SMALL_PARCEL:
                glColor4f(1.0f, 0.0f, 0.0f, 0.4f); // Red
                break;
            }

            // Draw filled plot
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (plot.boundary.size() >= 3) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(plot.center.x, plot.center.y, 0.01f);
                for (const auto& pt : plot.boundary) {
                    glVertex3f(pt.x, pt.y, 0.01f);
                }
                glVertex3f(plot.boundary[0].x, plot.boundary[0].y, 0.01f);
                glEnd();
            }

            glDisable(GL_BLEND);
        }
    }

    void drawRoadSegments() {
        for (const auto& segment : roadSegments) {
            // Color by hierarchy
            switch (segment.hierarchy) {
            case RoadHierarchy::MAJOR_ARTERIAL:
                glColor3f(1.0f, 0.0f, 0.0f); // Red for major roads
                glLineWidth(1.0f); // Scaled from 4.0f
                break;
            case RoadHierarchy::MINOR_ARTERIAL:
                glColor3f(0.8f, 0.4f, 0.0f); // Orange
                glLineWidth(0.3f); // Scaled from 3.0f
                break;
            case RoadHierarchy::COLLECTOR:
                glColor3f(0.6f, 0.6f, 0.0f); // Yellow-brown
                glLineWidth(0.2f); // Scaled from 2.0f
                break;
            case RoadHierarchy::LOCAL_STREET:
                glColor3f(0.5f, 0.5f, 0.5f); // Gray
                glLineWidth(0.1f); // Scaled from 1.0f
                break;
            }

            glBegin(GL_LINES);
            glVertex3f(segment.start.x, segment.start.y, 0.02f);
            glVertex3f(segment.end.x, segment.end.y, 0.02f);
            glEnd();

            // Draw direction indicator
            Point2D mid = segment.getMidpoint();
            Point2D arrowEnd = mid + segment.direction * (segment.width * 0.05f); // Scaled by 0.1 (0.5f -> 0.05f)

            glColor3f(0.0f, 0.0f, 1.0f); // Blue arrows
            glBegin(GL_LINES);
            glVertex3f(mid.x, mid.y, 0.03f);
            glVertex3f(arrowEnd.x, arrowEnd.y, 0.03f);
            glEnd();
        }
        glLineWidth(1.0f);
    }

    void drawEdgeConditions() {
        // Draw edge condition indicators around plots
        for (const auto& plot : analyzedPlots) {
            if (plot.edgeCondition == SiteEdgeType::INTERNAL_EDGE) continue;

            // Set color based on edge type
            switch (plot.edgeCondition) {
            case SiteEdgeType::WATER_EDGE:
                glColor3f(0.0f, 0.0f, 1.0f); // Blue
                break;
            case SiteEdgeType::PARK_EDGE:
                glColor3f(0.0f, 0.8f, 0.0f); // Green
                break;
            case SiteEdgeType::STREET_EDGE:
                glColor3f(0.6f, 0.6f, 0.6f); // Gray
                break;
            case SiteEdgeType::MIXED_EDGE:
                glColor3f(1.0f, 0.0f, 1.0f); // Magenta
                break;
            default:
                continue;
            }

            // Draw thicker outline for edge parcels
            glLineWidth(1.0f); // Scaled from 3.0f
            glBegin(GL_LINE_LOOP);
            for (const auto& pt : plot.boundary) {
                glVertex3f(pt.x, pt.y, 0.05f);
            }
            glEnd();
        }
        glLineWidth(1.0f);
    }

    void drawCornerConditions() {
        // Draw corner condition indicators
        for (const auto& plot : analyzedPlots) {
            if (plot.cornerCondition == SiteCornerType::NO_CORNER) continue;

            // Set color based on corner type
            switch (plot.cornerCondition) {
            case SiteCornerType::WATER_CORNER:
                glColor3f(0.0f, 0.0f, 1.0f); // Blue
                break;
            case SiteCornerType::PARK_CORNER:
                glColor3f(0.0f, 0.8f, 0.0f); // Green
                break;
            case SiteCornerType::PLAZA_CORNER:
                glColor3f(1.0f, 0.5f, 0.0f); // Orange
                break;
            case SiteCornerType::STREET_CORNER:
                glColor3f(0.8f, 0.8f, 0.8f); // Light gray
                break;
            case SiteCornerType::MIXED_CORNER:
                glColor3f(1.0f, 0.0f, 1.0f); // Magenta
                break;
            default:
                continue;
            }

            // Draw corner marker at plot center
            float markerSize = 0.8f; // Scaled from 8.0f
            glBegin(GL_QUADS);
            glVertex3f(plot.center.x - markerSize, plot.center.y - markerSize, 0.1f);
            glVertex3f(plot.center.x + markerSize, plot.center.y - markerSize, 0.1f);
            glVertex3f(plot.center.x + markerSize, plot.center.y + markerSize, 0.1f);
            glVertex3f(plot.center.x - markerSize, plot.center.y + markerSize, 0.1f);
            glEnd();
        }
    }

    void drawRoadFrontages() {
        // Draw primary road frontages for each plot
        for (const auto& plot : analyzedPlots) {
            if (plot.adjacentRoads.empty()) continue;

            // Find primary frontage
            const RoadInfo* primaryRoad = nullptr;
            for (const auto& road : plot.adjacentRoads) {
                if (road.isPrimaryFrontage) {
                    primaryRoad = &road;
                    break;
                }
            }

            if (!primaryRoad) continue;

            // Draw line from plot center to primary street frontage
            glColor3f(0.0f, 1.0f, 1.0f); // Cyan
            glLineWidth(0.2f); // Scaled from 2.0f
            glBegin(GL_LINES);
            glVertex3f(plot.center.x, plot.center.y, 0.1f);
            glVertex3f(plot.primaryStreetFrontage.x, plot.primaryStreetFrontage.y, 0.1f);
            glEnd();

            // Draw frontage point
            glColor3f(1.0f, 0.0f, 1.0f); // Magenta
            glPointSize(0.6f); // Scaled from 6.0f
            glBegin(GL_POINTS);
            glVertex3f(plot.primaryStreetFrontage.x, plot.primaryStreetFrontage.y, 0.1f);
            glEnd();
        }

        glLineWidth(1.0f);
        glPointSize(1.0f);
    }

    // ================== UTILITY METHODS ==================

    string getParcelCategoryName(ParcelCategory category) {
        switch (category) {
        case LARGE_PARCEL: return "Large (≥15,000m²)";
        case MEDIUM_PARCEL: return "Medium (≥10,000m²)";
        case SMALL_PARCEL: return "Small (≥2,500m²)";
        default: return "Unknown";
        }
    }

    string getSiteEdgeTypeName(SiteEdgeType edge) {
        switch (edge) {
        case SiteEdgeType::WATER_EDGE: return "Water";
        case SiteEdgeType::PARK_EDGE: return "Park";
        case SiteEdgeType::STREET_EDGE: return "Street";
        case SiteEdgeType::MIXED_EDGE: return "Mixed";
        case SiteEdgeType::INTERNAL_EDGE: return "Internal";
        default: return "Unknown";
        }
    }

    string getSiteCornerTypeName(SiteCornerType corner) {
        switch (corner) {
        case SiteCornerType::WATER_CORNER: return "Water";
        case SiteCornerType::PARK_CORNER: return "Park";
        case SiteCornerType::PLAZA_CORNER: return "Plaza";
        case SiteCornerType::STREET_CORNER: return "Street";
        case SiteCornerType::MIXED_CORNER: return "Mixed";
        case SiteCornerType::NO_CORNER: return "None";
        default: return "Unknown";
        }
    }

    string getRoadHierarchyName(RoadHierarchy hierarchy) {
        switch (hierarchy) {
        case RoadHierarchy::MAJOR_ARTERIAL: return "Major Arterial";
        case RoadHierarchy::MINOR_ARTERIAL: return "Minor Arterial";
        case RoadHierarchy::COLLECTOR: return "Collector";
        case RoadHierarchy::LOCAL_STREET: return "Local Street";
        default: return "Unknown";
        }
    }

private:
    // ================== PRIVATE HELPER METHODS ==================

    void buildRoadSegments() {
        if (!siteGen) return;

        roadSegments.clear();

        // Process major roads
        const auto& majorRoads = siteGen->getMajorRoads();
        for (size_t i = 0; i < majorRoads.size(); ++i) {
            processRoadCurve(majorRoads[i], RoadHierarchy::MAJOR_ARTERIAL, i);
        }

        // Process local roads
        const auto& localRoads = siteGen->getLocalRoads();
        for (size_t i = 0; i < localRoads.size(); ++i) {
            processRoadCurve(localRoads[i], RoadHierarchy::LOCAL_STREET, majorRoads.size() + i);
        }

        cout << "Built " << roadSegments.size() << " road segments" << endl;
    }

    void processRoadCurve(const Curve& curve, RoadHierarchy hierarchy, int curveId) {
        if (curve.points.size() < 2) return;

        // Break curve into segments
        for (size_t i = 0; i < curve.points.size() - 1; ++i) {
            RoadSegment segment;
            segment.start = curve.points[i];
            segment.end = curve.points[i + 1];
            segment.direction = (segment.end - segment.start).normalize();
            segment.width = curve.width;
            segment.hierarchy = hierarchy;
            segment.name = curve.name;
            segment.curveId = curveId;

            roadSegments.push_back(segment);
        }
    }

    void convertParcelsToPlots() {
        if (!siteGen) return;

        const auto& parcels = siteGen->getParcels();
        analyzedPlots.clear();
        analyzedPlots.reserve(parcels.size());

        for (size_t i = 0; i < parcels.size(); ++i) {
            AnalyzedPlot plot;
            plot.id = i;
            plot.parentParcelId = parcels[i].id;
            plot.boundary = parcels[i].boundary;
            plot.calculateBasicMetrics();

            analyzedPlots.push_back(plot);
        }
    }

    float getDistanceToMajorRoad(const Parcel& parcel) {
        float minDistance = 1000.0f;

        for (const auto& segment : roadSegments) {
            if (segment.hierarchy == RoadHierarchy::MAJOR_ARTERIAL) {
                float distance = segment.getDistanceToPoint(parcel.center);
                minDistance = min(minDistance, distance);
            }
        }

        return minDistance;
    }

    float getDistanceFromParcelToRoad(const Parcel& parcel, const RoadSegment& road) {
        // Find minimum distance from any parcel boundary point to road
        float minDistance = 1000.0f;

        for (const auto& pt : parcel.boundary) {
            float distance = road.getDistanceToPoint(pt);
            minDistance = min(minDistance, distance);
        }

        return minDistance;
    }

    float calculateFrontageLength(const Parcel& parcel, const RoadSegment& road) {
        // Calculate approximate frontage length
        float frontageLength = 0.0f;

        for (size_t i = 0; i < parcel.boundary.size(); ++i) {
            size_t j = (i + 1) % parcel.boundary.size();

            // Check if this edge is facing the road
            float dist1 = road.getDistanceToPoint(parcel.boundary[i]);
            float dist2 = road.getDistanceToPoint(parcel.boundary[j]);

            if (dist1 < ROAD_ADJACENCY_THRESHOLD && dist2 < ROAD_ADJACENCY_THRESHOLD) {
                frontageLength += parcel.boundary[i].distance(parcel.boundary[j]);
            }
        }

        return frontageLength;
    }

    Point2D getClosestPointOnParcelToRoad(const Parcel& parcel, const RoadSegment& road) {
        Point2D closestPoint = parcel.center;
        float minDistance = 1000.0f;

        for (const auto& pt : parcel.boundary) {
            float distance = road.getDistanceToPoint(pt);
            if (distance < minDistance) {
                minDistance = distance;
                closestPoint = pt;
            }
        }

        return closestPoint;
    }

    float calculateTotalStreetFrontage(const Parcel& parcel) {
        float totalFrontage = 0.0f;

        for (const auto& road : roadSegments) {
            totalFrontage += calculateFrontageLength(parcel, road);
        }

        return totalFrontage;
    }

    float calculateRequiredSetback(const AnalyzedPlot& plot) {
        // Calculate setback based on parcel category and edge conditions
        float baseSetback = 5.0f; // Base setback

        switch (plot.category) {
        case LARGE_PARCEL:
            baseSetback = 1.0f;
            break;
        case MEDIUM_PARCEL:
            baseSetback = 0.7f;
            break;
        case SMALL_PARCEL:
            baseSetback = 0.5f;
            break;
        }

        // Adjust based on edge conditions
        switch (plot.edgeCondition) {
        case SiteEdgeType::WATER_EDGE:
            baseSetback *= 1.5f; // Larger setback from water
            break;
        case SiteEdgeType::PARK_EDGE:
            baseSetback *= 1.2f; // Slightly larger setback from parks
            break;
        case SiteEdgeType::STREET_EDGE:
            // Standard setback for streets
            break;
        default:
            break;
        }

        return baseSetback;
    }

    bool isNearRoadIntersection(const Parcel& parcel) {
        // Check if parcel is near a major road intersection
        const float INTERSECTION_THRESHOLD = 10.0f; // Scaled from 100.0f

        for (size_t i = 0; i < roadSegments.size(); ++i) {
            for (size_t j = i + 1; j < roadSegments.size(); ++j) {
                // Find intersection point of two road segments
                Point2D intersection;
                if (findLineIntersection(roadSegments[i], roadSegments[j], intersection)) {
                    float distance = parcel.center.distance(intersection);
                    if (distance < INTERSECTION_THRESHOLD) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool findLineIntersection(const RoadSegment& road1, const RoadSegment& road2, Point2D& intersection) {
        // Calculate intersection of two line segments
        float x1 = road1.start.x, y1 = road1.start.y;
        float x2 = road1.end.x, y2 = road1.end.y;
        float x3 = road2.start.x, y3 = road2.start.y;
        float x4 = road2.end.x, y4 = road2.end.y;

        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

        if (abs(denom) < 0.001f) {
            return false; // Lines are parallel
        }

        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

        if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
            intersection.x = x1 + t * (x2 - x1);
            intersection.y = y1 + t * (y2 - y1);
            return true;
        }

        return false;
    }

    bool isNearPlaza(const Parcel& parcel) {
        // Heuristic: assume smaller park boundaries are plazas
        if (!siteGen) return false;

        const auto& parkBoundaries = siteGen->getParkBoundaries();

        for (const auto& park : parkBoundaries) {
            float distance = park.getDistanceToPoint(parcel.center);
            if (distance < PARK_EDGE_THRESHOLD) {
                // Calculate approximate area of park
                float parkArea = 0.0f;
                if (park.points.size() >= 3) {
                    // Simplified area calculation
                    for (size_t i = 0; i < park.points.size(); ++i) {
                        size_t j = (i + 1) % park.points.size();
                        parkArea += park.points[i].x * park.points[j].y;
                        parkArea -= park.points[j].x * park.points[i].y;
                    }
                    parkArea = abs(parkArea) * 0.5f;
                }

                // Consider it a plaza if area is smaller than 5000 sqm
                if (parkArea < 5000.0f) {
                    return true;
                }
            }
        }

        return false;
    }
};

#endif // PARCEL_ANALYZER_H