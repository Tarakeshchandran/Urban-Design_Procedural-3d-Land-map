#ifndef ROAD_ANALYZER_H
#define ROAD_ANALYZER_H

// Include dependencies
#include "SiteGeneration.h"

// Standard Library Includes
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <functional>
#include <cmath>

// OpenGL includes
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

using namespace std;

// ===================== ROAD ANALYSIS DATA STRUCTURES =====================

// Road hierarchy classification (enhanced from ParcelAnalyzer)
enum class RoadClassification {
    PRIMARY_ARTERIAL,      // Major highways, main city arteries
    SECONDARY_ARTERIAL,    // Important connecting roads
    COLLECTOR_ROAD,        // Neighborhood connectors
    LOCAL_STREET,          // Residential streets
    SERVICE_ROAD,          // Service lanes, alleys
    PEDESTRIAN_PATH        // Walkways, bike paths
};

// Road intersection types
enum class IntersectionType {
    T_JUNCTION,           // Three-way intersection
    CROSS_INTERSECTION,   // Four-way intersection
    Y_JUNCTION,           // Y-shaped junction
    ROUNDABOUT,           // Circular intersection
    COMPLEX_INTERSECTION, // More than 4 roads meeting
    DEAD_END             // Road terminus
};

// Road network patterns
enum class NetworkPattern {
    GRID_PATTERN,         // Regular grid layout
    RADIAL_PATTERN,       // Roads radiating from center
    ORGANIC_PATTERN,      // Irregular, curved roads
    MIXED_PATTERN,        // Combination of patterns
    LINEAR_PATTERN        // Primarily linear arrangement
};

// Enhanced road segment with analysis data
struct AnalyzedRoadSegment {
    // Basic properties
    Point2D start, end;
    Point2D direction;
    Point2D perpendicular;    // Perpendicular direction for subdivision
    float width;
    float length;
    RoadClassification classification;
    string name;
    int curveId;

    // Analysis properties
    float curvature;          // How curved the segment is
    float importance;         // Calculated importance score
    bool isDeadEnd;          // Terminal segment
    bool isConnector;        // Connects different road types
    float trafficCapacity;   // Estimated traffic capacity

    // Network position
    vector<int> connectedSegments;  // IDs of connected segments
    vector<int> intersectionIds;    // IDs of intersections this segment participates in

    AnalyzedRoadSegment() : width(6.0f), length(0), classification(RoadClassification::LOCAL_STREET),
        curveId(-1), curvature(0), importance(0), isDeadEnd(false),
        isConnector(false), trafficCapacity(100) {
    }

    void calculateBasicProperties() {
        length = start.distance(end);
        if (length > 0.001f) {
            direction = (end - start).normalize();
            // Calculate perpendicular direction (rotate 90 degrees)
            perpendicular = Point2D(-direction.y, direction.x);
        }
    }

    float getAngleDegrees() const {
        return atan2(direction.y, direction.x) * 180.0f / M_PI;
    }

    Point2D getMidpoint() const {
        return Point2D((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
    }

    // Get point at normalized distance along segment
    Point2D getPointAt(float t) const {
        t = max(0.0f, min(1.0f, t));
        return Point2D(start.x + t * (end.x - start.x), start.y + t * (end.y - start.y));
    }

    // Calculate minimum distance from point to segment
    float getDistanceToPoint(const Point2D& point) const {
        Point2D ab = end - start;
        Point2D ap = point - start;

        float abSquared = ab.x * ab.x + ab.y * ab.y;
        if (abSquared == 0) return point.distance(start);

        float t = max(0.0f, min(1.0f, (ap.x * ab.x + ap.y * ab.y) / abSquared));
        Point2D projection = start + ab * t;

        return point.distance(projection);
    }
};

// Road intersection data
struct RoadIntersection {
    int id;
    Point2D location;
    IntersectionType type;
    vector<int> connectedRoadIds;        // IDs of road segments meeting here
    vector<float> roadAngles;            // Angles of roads at intersection
    float importance;                    // Intersection importance score
    bool isMajorIntersection;           // Major road intersection

    // Geometric properties
    float averageRoadWidth;
    float intersectionRadius;            // Approximate radius of intersection area

    // Analysis properties
    bool isGoodForSubdivision;          // Good location for subdivision axis
    float subdivisionInfluence;         // How much this affects nearby subdivisions

    RoadIntersection() : id(-1), type(IntersectionType::CROSS_INTERSECTION), importance(0),
        isMajorIntersection(false), averageRoadWidth(6.0f), intersectionRadius(10.0f),
        isGoodForSubdivision(false), subdivisionInfluence(0) {
    }

    void calculateProperties() {
        // Calculate average road width
        if (!connectedRoadIds.empty()) {
            averageRoadWidth /= connectedRoadIds.size();
        }

        // Estimate intersection radius based on road widths and angles
        intersectionRadius = averageRoadWidth * 0.5f + 5.0f;

        // Determine intersection type based on number of roads
        if (connectedRoadIds.size() == 1) {
            type = IntersectionType::DEAD_END;
        }
        else if (connectedRoadIds.size() == 3) {
            type = IntersectionType::T_JUNCTION;
        }
        else if (connectedRoadIds.size() == 4) {
            type = IntersectionType::CROSS_INTERSECTION;
        }
        else if (connectedRoadIds.size() > 4) {
            type = IntersectionType::COMPLEX_INTERSECTION;
        }
    }
};

// Road network analysis data
struct NetworkAnalysis {
    NetworkPattern dominantPattern;
    float gridRegularity;           // 0-1, how grid-like the network is
    float connectivity;             // Average number of connections per intersection
    float density;                  // Road length per unit area
    float averageBlockSize;         // Average block dimension
    Point2D primaryDirection;       // Main road direction
    Point2D secondaryDirection;     // Secondary road direction (perpendicular)
    float directionalBias;          // 0-1, how directionally biased the network is

    NetworkAnalysis() : dominantPattern(NetworkPattern::MIXED_PATTERN), gridRegularity(0),
        connectivity(0), density(0), averageBlockSize(100), directionalBias(0) {
    }
};

// ===================== ROAD ANALYZER CLASS =====================

class RoadAnalyzer {
private:
    SiteGeneration* siteGen;
    vector<AnalyzedRoadSegment> analyzedSegments;
    vector<RoadIntersection> intersections;
    NetworkAnalysis networkAnalysis;

    // Analysis parameters
    static constexpr float INTERSECTION_TOLERANCE = 1.0f;        // Scaled from 10.0f
    static constexpr float PARALLEL_TOLERANCE = 15.0f;          // Angle to consider roads parallel (degrees)
    static constexpr float MAJOR_ROAD_WIDTH_THRESHOLD = 1.0f;   // Scaled from 10.0f
    static constexpr float CURVATURE_THRESHOLD = 0.1f;          // Threshold for considering road curved
    static constexpr float GRID_ALIGNMENT_TOLERANCE = 20.0f;    // Angle tolerance for grid detection

public:
    // ================== CONSTRUCTOR & DESTRUCTOR ==================

    RoadAnalyzer(SiteGeneration* siteGeneration) : siteGen(siteGeneration) {
        cout << "=== RoadAnalyzer Initialized ===" << endl;
        if (siteGen) {
            buildAnalyzedRoadSegments();
            analyzeIntersections();
            analyzeNetworkPattern();
        }
    }

    ~RoadAnalyzer() {
        cout << "=== RoadAnalyzer Destroyed ===" << endl;
    }

    // ================== MAIN ANALYSIS METHODS ==================

    // Classify road importance based on width, connections, and hierarchy
    RoadClassification classifyRoad(const Curve& road) {
        if (road.width >= 15.0f) {
            return RoadClassification::PRIMARY_ARTERIAL;
        }
        else if (road.width >= 10.0f) {
            return RoadClassification::SECONDARY_ARTERIAL;
        }
        else if (road.width >= 7.0f) {
            return RoadClassification::COLLECTOR_ROAD;
        }
        else if (road.width >= 4.0f) {
            return RoadClassification::LOCAL_STREET;
        }
        else {
            return RoadClassification::SERVICE_ROAD;
        }
    }

    // Get road direction at a specific point
    Point2D getRoadDirection(const Curve& road, const Point2D& queryPoint) {
        if (road.points.size() < 2) return Point2D(1, 0); // Default east direction

        // Find closest segment
        float minDistance = 100000.0f;
        int closestSegmentIndex = 0;

        for (size_t i = 0; i < road.points.size() - 1; ++i) {
            Point2D segmentStart = road.points[i];
            Point2D segmentEnd = road.points[i + 1];

            // Calculate distance from point to segment
            Point2D ab = segmentEnd - segmentStart;
            Point2D ap = queryPoint - segmentStart;

            float abSquared = ab.x * ab.x + ab.y * ab.y;
            if (abSquared == 0) continue;

            float t = max(0.0f, min(1.0f, (ap.x * ab.x + ap.y * ab.y) / abSquared));
            Point2D projection = segmentStart + ab * t;

            float distance = queryPoint.distance(projection);
            if (distance < minDistance) {
                minDistance = distance;
                closestSegmentIndex = i;
            }
        }

        // Return direction of closest segment
        Point2D direction = road.points[closestSegmentIndex + 1] - road.points[closestSegmentIndex];
        return direction.normalize();
    }

    // Get road direction from analyzed segment
    Point2D getRoadDirection(int segmentId) {
        if (segmentId >= 0 && segmentId < analyzedSegments.size()) {
            return analyzedSegments[segmentId].direction;
        }
        return Point2D(1, 0); // Default east direction
    }

    // Get perpendicular direction for subdivision alignment
    Point2D getSubdivisionDirection(int segmentId) {
        if (segmentId >= 0 && segmentId < analyzedSegments.size()) {
            return analyzedSegments[segmentId].perpendicular;
        }
        return Point2D(0, 1); // Default north direction
    }

    // Find best subdivision alignment for a given point (scaled influence radius)
    Point2D getBestSubdivisionAlignment(const Point2D& location, float influenceRadius = 10.0f) { // Scaled from 100.0f
        vector<Point2D> nearbyDirections;
        vector<float> weights;

        // Collect directions from nearby road segments
        for (const auto& segment : analyzedSegments) {
            float distance = segment.getDistanceToPoint(location);

            if (distance <= influenceRadius) {
                // Weight based on road importance and proximity
                float weight = segment.importance * (1.0f - distance / influenceRadius);

                nearbyDirections.push_back(segment.direction);
                weights.push_back(weight);

                // Also consider perpendicular direction if it's a major road
                if (segment.classification <= RoadClassification::SECONDARY_ARTERIAL) {
                    nearbyDirections.push_back(segment.perpendicular);
                    weights.push_back(weight * 0.5f); // Less weight for perpendicular
                }
            }
        }

        // If no nearby roads, use network primary direction
        if (nearbyDirections.empty()) {
            return networkAnalysis.primaryDirection;
        }

        // Calculate weighted average direction
        Point2D averageDirection(0, 0);
        float totalWeight = 0;

        for (size_t i = 0; i < nearbyDirections.size(); ++i) {
            averageDirection.x += nearbyDirections[i].x * weights[i];
            averageDirection.y += nearbyDirections[i].y * weights[i];
            totalWeight += weights[i];
        }

        if (totalWeight > 0) {
            averageDirection.x /= totalWeight;
            averageDirection.y /= totalWeight;
            return averageDirection.normalize();
        }

        return networkAnalysis.primaryDirection;
    }

    // Get road width for a specific segment
    float getRoadWidth(int segmentId) {
        if (segmentId >= 0 && segmentId < analyzedSegments.size()) {
            return analyzedSegments[segmentId].width;
        }
        return 6.0f; // Default width
    }

    // Get all intersections and their properties
    vector<RoadIntersection> getIntersectionPoints() {
        return intersections;
    }

    // Calculate street network density in a given area (scaled radius)
    float getStreetNetworkDensity(const Point2D& center, float radius) {
        float totalLength = 0.0f;
        float area = M_PI * radius * radius;

        for (const auto& segment : analyzedSegments) {
            // Check if segment is within or intersects the circular area
            float distanceToCenter = segment.getDistanceToPoint(center);

            if (distanceToCenter <= radius) {
                // Segment is within the area, add its full length
                if (distanceToCenter + segment.length * 0.5f <= radius) {
                    totalLength += segment.length;
                }
                else {
                    // Segment partially within area, estimate intersecting length
                    float intersectingLength = segment.length * (radius - distanceToCenter) / radius;
                    totalLength += max(0.0f, intersectingLength);
                }
            }
        }

        return totalLength / area;
    }

    // Map parcels to nearby road segments (scaled max distance)
    map<int, vector<int>> mapParcelsToRoads(const vector<Parcel>& parcels, float maxDistance = 5.0f) { // Scaled from 50.0f
        map<int, vector<int>> parcelToRoads;

        for (size_t parcelIdx = 0; parcelIdx < parcels.size(); ++parcelIdx) {
            const auto& parcel = parcels[parcelIdx];
            vector<int> nearbyRoads;

            for (size_t segmentIdx = 0; segmentIdx < analyzedSegments.size(); ++segmentIdx) {
                const auto& segment = analyzedSegments[segmentIdx];

                float distance = segment.getDistanceToPoint(parcel.center);
                if (distance <= maxDistance) {
                    nearbyRoads.push_back(segmentIdx);
                }
            }

            // Sort by distance
            sort(nearbyRoads.begin(), nearbyRoads.end(),
                [&](int a, int b) {
                    float distA = analyzedSegments[a].getDistanceToPoint(parcel.center);
                    float distB = analyzedSegments[b].getDistanceToPoint(parcel.center);
                    return distA < distB;
                });

            parcelToRoads[parcelIdx] = nearbyRoads;
        }

        return parcelToRoads;
    }

    // ================== ACCESSOR METHODS ==================

    const vector<AnalyzedRoadSegment>& getAnalyzedSegments() const { return analyzedSegments; }
    const vector<RoadIntersection>& getIntersections() const { return intersections; }
    const NetworkAnalysis& getNetworkAnalysis() const { return networkAnalysis; }

    // Get segments by classification
    vector<int> getSegmentsByClassification(RoadClassification classification) const {
        vector<int> result;
        for (size_t i = 0; i < analyzedSegments.size(); ++i) {
            if (analyzedSegments[i].classification == classification) {
                result.push_back(i);
            }
        }
        return result;
    }

    // Get major roads (primary and secondary arterials)
    vector<int> getMajorRoadSegments() const {
        vector<int> result;
        for (size_t i = 0; i < analyzedSegments.size(); ++i) {
            if (analyzedSegments[i].classification <= RoadClassification::SECONDARY_ARTERIAL) {
                result.push_back(i);
            }
        }
        return result;
    }

    // Get intersections by type
    vector<int> getIntersectionsByType(IntersectionType type) const {
        vector<int> result;
        for (size_t i = 0; i < intersections.size(); ++i) {
            if (intersections[i].type == type) {
                result.push_back(i);
            }
        }
        return result;
    }

    // Get most influential intersection near a point (scaled max distance)
    int getNearestMajorIntersection(const Point2D& location, float maxDistance = 20.0f) { // Scaled from 200.0f
        int bestIntersection = -1;
        float bestScore = 0;

        for (size_t i = 0; i < intersections.size(); ++i) {
            float distance = location.distance(intersections[i].location);

            if (distance <= maxDistance) {
                // Score based on importance and proximity
                float score = intersections[i].importance * (1.0f - distance / maxDistance);

                if (score > bestScore) {
                    bestScore = score;
                    bestIntersection = i;
                }
            }
        }

        return bestIntersection;
    }

    // ================== ANALYSIS METHODS ==================

    void analyzeNetwork() {
        cout << "\n=== ANALYZING ROAD NETWORK ===" << endl;

        buildAnalyzedRoadSegments();
        analyzeIntersections();
        analyzeNetworkPattern();
        calculateSegmentImportance();

        cout << "Road network analysis complete." << endl;
        printNetworkStatistics();
    }

    void printNetworkStatistics() {
        cout << "\n=== ROAD NETWORK STATISTICS ===" << endl;

        // Count segments by classification
        map<RoadClassification, int> classificationCounts;
        float totalLength = 0;
        for (const auto& segment : analyzedSegments) {
            classificationCounts[segment.classification]++;
            totalLength += segment.length;
        }

        cout << "Road Segments by Classification:" << endl;
        for (const auto& pair : classificationCounts) {
            cout << "  " << getRoadClassificationName(pair.first) << ": " << pair.second << endl;
        }

        cout << "Network Properties:" << endl;
        cout << "  Total road length: " << totalLength << "m" << endl;
        cout << "  Number of intersections: " << intersections.size() << endl;
        cout << "  Dominant pattern: " << getNetworkPatternName(networkAnalysis.dominantPattern) << endl;
        cout << "  Grid regularity: " << networkAnalysis.gridRegularity << endl;
        cout << "  Network connectivity: " << networkAnalysis.connectivity << endl;
        cout << "  Average block size: " << networkAnalysis.averageBlockSize << "m" << endl;
        cout << "  Directional bias: " << networkAnalysis.directionalBias << endl;

        // Primary directions
        float primaryAngle = atan2(networkAnalysis.primaryDirection.y, networkAnalysis.primaryDirection.x) * 180.0f / M_PI;
        float secondaryAngle = atan2(networkAnalysis.secondaryDirection.y, networkAnalysis.secondaryDirection.x) * 180.0f / M_PI;
        cout << "  Primary direction: " << primaryAngle << "° from east" << endl;
        cout << "  Secondary direction: " << secondaryAngle << "° from east" << endl;

        // Intersection statistics
        map<IntersectionType, int> intersectionCounts;
        for (const auto& intersection : intersections) {
            intersectionCounts[intersection.type]++;
        }

        cout << "Intersections by Type:" << endl;
        for (const auto& pair : intersectionCounts) {
            cout << "  " << getIntersectionTypeName(pair.first) << ": " << pair.second << endl;
        }
        cout << "=================================" << endl;
    }

    // ================== VISUALIZATION METHODS ==================

    void drawAnalyzedRoads() {
        drawRoadsByClassification();
        drawRoadDirections();
    }

    void drawRoadsByClassification() {
        for (const auto& segment : analyzedSegments) {
            // Color by classification
            switch (segment.classification) {
            case RoadClassification::PRIMARY_ARTERIAL:
                glColor3f(1.0f, 0.0f, 0.0f); // Bright red
                glLineWidth(0.6f); // Scaled from 6.0f
                break;
            case RoadClassification::SECONDARY_ARTERIAL:
                glColor3f(0.8f, 0.2f, 0.0f); // Orange-red
                glLineWidth(0.4f); // Scaled from 4.0f
                break;
            case RoadClassification::COLLECTOR_ROAD:
                glColor3f(0.6f, 0.4f, 0.0f); // Orange
                glLineWidth(0.3f); // Scaled from 3.0f
                break;
            case RoadClassification::LOCAL_STREET:
                glColor3f(0.5f, 0.5f, 0.5f); // Gray
                glLineWidth(0.2f); // Scaled from 2.0f
                break;
            case RoadClassification::SERVICE_ROAD:
                glColor3f(0.7f, 0.7f, 0.7f); // Light gray
                glLineWidth(0.1f); // Scaled from 1.0f
                break;
            default:
                glColor3f(0.3f, 0.3f, 0.3f); // Dark gray
                glLineWidth(0.1f); // Scaled from 1.0f
                break;
            }

            glBegin(GL_LINES);
            glVertex3f(segment.start.x, segment.start.y, 0.02f);
            glVertex3f(segment.end.x, segment.end.y, 0.02f);
            glEnd();
        }
        glLineWidth(1.0f);
    }

    void drawRoadDirections() {
        // Draw direction arrows for major roads
        for (const auto& segment : analyzedSegments) {
            if (segment.classification <= RoadClassification::COLLECTOR_ROAD) {
                Point2D mid = segment.getMidpoint();
                float arrowLength = segment.width * 0.08f; // Scaled from 0.8f
                Point2D arrowEnd = mid + segment.direction * arrowLength;

                // Main direction arrow
                glColor3f(0.0f, 0.0f, 1.0f); // Blue
                glLineWidth(0.2f); // Scaled from 2.0f
                glBegin(GL_LINES);
                glVertex3f(mid.x, mid.y, 0.03f);
                glVertex3f(arrowEnd.x, arrowEnd.y, 0.03f);
                glEnd();

                // Perpendicular direction (for subdivision)
                if (segment.classification <= RoadClassification::SECONDARY_ARTERIAL) {
                    Point2D perpEnd = mid + segment.perpendicular * (arrowLength * 0.6f);
                    glColor3f(0.0f, 0.8f, 0.0f); // Green
                    glLineWidth(0.15f); // Scaled from 1.5f
                    glBegin(GL_LINES);
                    glVertex3f(mid.x, mid.y, 0.03f);
                    glVertex3f(perpEnd.x, perpEnd.y, 0.03f);
                    glEnd();
                }
            }
        }
        glLineWidth(1.0f);
    }

    void drawIntersections() {
        for (const auto& intersection : intersections) {
            // Color by type and importance
            float intensity = min(1.0f, intersection.importance);

            switch (intersection.type) {
            case IntersectionType::CROSS_INTERSECTION:
                glColor3f(intensity, 0.0f, intensity); // Magenta
                break;
            case IntersectionType::T_JUNCTION:
                glColor3f(0.0f, intensity, intensity); // Cyan
                break;
            case IntersectionType::Y_JUNCTION:
                glColor3f(intensity, intensity, 0.0f); // Yellow
                break;
            case IntersectionType::COMPLEX_INTERSECTION:
                glColor3f(intensity, 0.0f, 0.0f); // Red
                break;
            case IntersectionType::DEAD_END:
                glColor3f(0.5f, 0.5f, 0.5f); // Gray
                break;
            default:
                glColor3f(0.3f, 0.3f, 0.3f); // Dark gray
                break;
            }

            // Draw intersection marker (properly scaled)
            float markerSize = intersection.intersectionRadius * 0.1f; // Scaled down by 0.1
            glBegin(GL_QUADS);
            glVertex3f(intersection.location.x - markerSize, intersection.location.y - markerSize, 0.1f);
            glVertex3f(intersection.location.x + markerSize, intersection.location.y - markerSize, 0.1f);
            glVertex3f(intersection.location.x + markerSize, intersection.location.y + markerSize, 0.1f);
            glVertex3f(intersection.location.x - markerSize, intersection.location.y + markerSize, 0.1f);
            glEnd();

            // Draw intersection ID for reference
            glColor3f(0.0f, 0.0f, 0.0f);
            glRasterPos3f(intersection.location.x + markerSize, intersection.location.y + markerSize, 0.15f);
            string idStr = to_string(intersection.id);
            for (char c : idStr) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
            }
        }
    }

    void drawNetworkPattern() {
        // Draw primary and secondary grid directions
        if (networkAnalysis.dominantPattern == NetworkPattern::GRID_PATTERN) {
            // Draw grid visualization
            Point2D center(0, 0); // Assume origin is roughly in the center
            float extent = 20.0f; // Scaled from 200.0f

            // Primary direction lines
            glColor3f(0.0f, 1.0f, 0.0f); // Green for primary
            glLineWidth(0.2f); // Scaled from 2.0f
            for (int i = -5; i <= 5; ++i) {
                Point2D offset = networkAnalysis.primaryDirection * (i * 5.0f); // Scaled from 50.0f
                Point2D start = center + offset - networkAnalysis.secondaryDirection * extent;
                Point2D end = center + offset + networkAnalysis.secondaryDirection * extent;

                glBegin(GL_LINES);
                glVertex3f(start.x, start.y, 0.001f);
                glVertex3f(end.x, end.y, 0.001f);
                glEnd();
            }

            // Secondary direction lines
            glColor3f(0.0f, 0.0f, 1.0f); // Blue for secondary
            for (int i = -5; i <= 5; ++i) {
                Point2D offset = networkAnalysis.secondaryDirection * (i * 5.0f); // Scaled from 50.0f
                Point2D start = center + offset - networkAnalysis.primaryDirection * extent;
                Point2D end = center + offset + networkAnalysis.primaryDirection * extent;

                glBegin(GL_LINES);
                glVertex3f(start.x, start.y, 0.001f);
                glVertex3f(end.x, end.y, 0.001f);
                glEnd();
            }
        }
        glLineWidth(1.0f);
    }

    void drawSubdivisionAlignments(const vector<Point2D>& testPoints) {
        // Draw subdivision alignment recommendations for given points
        for (const auto& point : testPoints) {
            Point2D alignment = getBestSubdivisionAlignment(point);

            // Draw alignment direction
            glColor3f(1.0f, 0.5f, 0.0f); // Orange
            glLineWidth(0.2f); // Scaled from 2.0f

            Point2D start = point - alignment * 2.5f; // Scaled from 25.0f
            Point2D end = point + alignment * 2.5f;   // Scaled from 25.0f

            glBegin(GL_LINES);
            glVertex3f(start.x, start.y, 0.05f);
            glVertex3f(end.x, end.y, 0.05f);
            glEnd();

            // Draw perpendicular alignment
            Point2D perpAlignment(-alignment.y, alignment.x);
            Point2D perpStart = point - perpAlignment * 1.5f; // Scaled from 15.0f
            Point2D perpEnd = point + perpAlignment * 1.5f;   // Scaled from 15.0f

            glColor3f(0.5f, 1.0f, 0.0f); // Light green
            glBegin(GL_LINES);
            glVertex3f(perpStart.x, perpStart.y, 0.05f);
            glVertex3f(perpEnd.x, perpEnd.y, 0.05f);
            glEnd();
        }
        glLineWidth(1.0f);
    }

    // ================== UTILITY METHODS ==================

    string getRoadClassificationName(RoadClassification classification) {
        switch (classification) {
        case RoadClassification::PRIMARY_ARTERIAL: return "Primary Arterial";
        case RoadClassification::SECONDARY_ARTERIAL: return "Secondary Arterial";
        case RoadClassification::COLLECTOR_ROAD: return "Collector Road";
        case RoadClassification::LOCAL_STREET: return "Local Street";
        case RoadClassification::SERVICE_ROAD: return "Service Road";
        case RoadClassification::PEDESTRIAN_PATH: return "Pedestrian Path";
        default: return "Unknown";
        }
    }

    string getIntersectionTypeName(IntersectionType type) {
        switch (type) {
        case IntersectionType::T_JUNCTION: return "T-Junction";
        case IntersectionType::CROSS_INTERSECTION: return "Cross Intersection";
        case IntersectionType::Y_JUNCTION: return "Y-Junction";
        case IntersectionType::ROUNDABOUT: return "Roundabout";
        case IntersectionType::COMPLEX_INTERSECTION: return "Complex Intersection";
        case IntersectionType::DEAD_END: return "Dead End";
        default: return "Unknown";
        }
    }

    string getNetworkPatternName(NetworkPattern pattern) {
        switch (pattern) {
        case NetworkPattern::GRID_PATTERN: return "Grid Pattern";
        case NetworkPattern::RADIAL_PATTERN: return "Radial Pattern";
        case NetworkPattern::ORGANIC_PATTERN: return "Organic Pattern";
        case NetworkPattern::MIXED_PATTERN: return "Mixed Pattern";
        case NetworkPattern::LINEAR_PATTERN: return "Linear Pattern";
        default: return "Unknown";
        }
    }

private:
    // ================== PRIVATE ANALYSIS METHODS ==================

    void buildAnalyzedRoadSegments() {
        if (!siteGen) return;

        analyzedSegments.clear();

        // Process major roads
        const auto& majorRoads = siteGen->getMajorRoads();
        for (size_t i = 0; i < majorRoads.size(); ++i) {
            processRoadForAnalysis(majorRoads[i], i, true);
        }

        // Process local roads
        const auto& localRoads = siteGen->getLocalRoads();
        for (size_t i = 0; i < localRoads.size(); ++i) {
            processRoadForAnalysis(localRoads[i], majorRoads.size() + i, false);
        }

        // Calculate derived properties
        for (auto& segment : analyzedSegments) {
            calculateSegmentCurvature(segment);
            segment.calculateBasicProperties();
        }

        cout << "Built " << analyzedSegments.size() << " analyzed road segments" << endl;
    }

    void processRoadForAnalysis(const Curve& road, int roadId, bool isMajor) {
        if (road.points.size() < 2) return;

        RoadClassification classification = isMajor ?
            (road.width >= MAJOR_ROAD_WIDTH_THRESHOLD ? RoadClassification::PRIMARY_ARTERIAL : RoadClassification::SECONDARY_ARTERIAL) :
            classifyRoad(road);

        // Create segments from road curve
        for (size_t i = 0; i < road.points.size() - 1; ++i) {
            AnalyzedRoadSegment segment;
            segment.start = road.points[i];
            segment.end = road.points[i + 1];
            segment.width = road.width;
            segment.classification = classification;
            segment.name = road.name;
            segment.curveId = roadId;
            segment.calculateBasicProperties();

            analyzedSegments.push_back(segment);
        }
    }

    void calculateSegmentCurvature(AnalyzedRoadSegment& segment) {
        // For now, assume straight segments (curvature = 0)
        // This could be enhanced to calculate actual curvature from neighboring segments
        segment.curvature = 0.0f;
    }

    void analyzeIntersections() {
        intersections.clear();
        map<pair<int, int>, Point2D> potentialIntersections;

        // Find all intersection points
        for (size_t i = 0; i < analyzedSegments.size(); ++i) {
            for (size_t j = i + 1; j < analyzedSegments.size(); ++j) {
                Point2D intersection;
                if (calculateIntersection(analyzedSegments[i], analyzedSegments[j], intersection)) {
                    potentialIntersections[{i, j}] = intersection;
                }
            }
        }

        // Group nearby intersections
        vector<bool> processed(potentialIntersections.size(), false);
        int intersectionId = 0;

        auto it = potentialIntersections.begin();
        for (size_t idx = 0; idx < potentialIntersections.size(); ++idx, ++it) {
            if (processed[idx]) continue;

            RoadIntersection intersection;
            intersection.id = intersectionId++;
            intersection.location = it->second;
            intersection.connectedRoadIds.push_back(it->first.first);
            intersection.connectedRoadIds.push_back(it->first.second);

            // Find nearby intersections to merge
            auto it2 = it;
            ++it2;
            for (size_t idx2 = idx + 1; idx2 < potentialIntersections.size(); ++idx2, ++it2) {
                if (processed[idx2]) continue;

                if (it->second.distance(it2->second) < INTERSECTION_TOLERANCE) {
                    intersection.connectedRoadIds.push_back(it2->first.first);
                    intersection.connectedRoadIds.push_back(it2->first.second);
                    processed[idx2] = true;
                }
            }

            // Remove duplicates and calculate properties
            sort(intersection.connectedRoadIds.begin(), intersection.connectedRoadIds.end());
            intersection.connectedRoadIds.erase(
                unique(intersection.connectedRoadIds.begin(), intersection.connectedRoadIds.end()),
                intersection.connectedRoadIds.end());

            // Calculate intersection properties
            calculateIntersectionProperties(intersection);

            intersections.push_back(intersection);
            processed[idx] = true;
        }

        // Update segments with intersection references
        updateSegmentIntersectionReferences();

        cout << "Found " << intersections.size() << " road intersections" << endl;
    }

    bool calculateIntersection(const AnalyzedRoadSegment& seg1, const AnalyzedRoadSegment& seg2, Point2D& intersection) {
        // Calculate intersection of two line segments
        float x1 = seg1.start.x, y1 = seg1.start.y;
        float x2 = seg1.end.x, y2 = seg1.end.y;
        float x3 = seg2.start.x, y3 = seg2.start.y;
        float x4 = seg2.end.x, y4 = seg2.end.y;

        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

        if (abs(denom) < 0.001f) {
            return false; // Lines are parallel
        }

        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

        // Check if intersection is within both segments (with tolerance)
        if (t >= -0.1f && t <= 1.1f && u >= -0.1f && u <= 1.1f) {
            intersection.x = x1 + t * (x2 - x1);
            intersection.y = y1 + t * (y2 - y1);
            return true;
        }

        return false;
    }

    void calculateIntersectionProperties(RoadIntersection& intersection) {
        intersection.calculateProperties();

        // Calculate importance based on road classifications
        float importance = 0;
        intersection.averageRoadWidth = 0;

        for (int roadId : intersection.connectedRoadIds) {
            if (roadId >= 0 && roadId < analyzedSegments.size()) {
                const auto& segment = analyzedSegments[roadId];

                // Add importance based on road classification
                switch (segment.classification) {
                case RoadClassification::PRIMARY_ARTERIAL:
                    importance += 3.0f;
                    break;
                case RoadClassification::SECONDARY_ARTERIAL:
                    importance += 2.0f;
                    break;
                case RoadClassification::COLLECTOR_ROAD:
                    importance += 1.5f;
                    break;
                default:
                    importance += 1.0f;
                    break;
                }

                intersection.averageRoadWidth += segment.width;

                // Calculate road angles at intersection
                Point2D toIntersection = intersection.location - segment.getMidpoint();
                float angle = atan2(toIntersection.y, toIntersection.x) * 180.0f / M_PI;
                intersection.roadAngles.push_back(angle);
            }
        }

        // Normalize importance and calculate final properties
        intersection.importance = importance / intersection.connectedRoadIds.size();
        intersection.averageRoadWidth /= intersection.connectedRoadIds.size();
        intersection.isMajorIntersection = (intersection.importance >= 2.0f);

        // Determine if good for subdivision
        intersection.isGoodForSubdivision = (intersection.type == IntersectionType::CROSS_INTERSECTION ||
            intersection.type == IntersectionType::T_JUNCTION) &&
            intersection.isMajorIntersection;

        intersection.subdivisionInfluence = intersection.importance * 5.0f; // Scaled from 50.0f
    }

    void updateSegmentIntersectionReferences() {
        for (size_t segIdx = 0; segIdx < analyzedSegments.size(); ++segIdx) {
            auto& segment = analyzedSegments[segIdx];

            for (size_t intIdx = 0; intIdx < intersections.size(); ++intIdx) {
                const auto& intersection = intersections[intIdx];

                // Check if this segment participates in this intersection
                bool participates = false;
                for (int roadId : intersection.connectedRoadIds) {
                    if (roadId == segIdx) {
                        participates = true;
                        break;
                    }
                }

                if (participates) {
                    segment.intersectionIds.push_back(intIdx);
                }
            }
        }
    }

    void analyzeNetworkPattern() {
        calculateGridAlignment();
        calculateConnectivity();
        calculateNetworkDensity();
        determineNetworkPattern();

        cout << "Network pattern analysis complete" << endl;
    }

    void calculateGridAlignment() {
        // Analyze road directions to find dominant grid alignment
        map<int, int> angleHistogram; // Angle (in 10-degree bins) -> count

        for (const auto& segment : analyzedSegments) {
            if (segment.classification <= RoadClassification::COLLECTOR_ROAD) {
                float angle = segment.getAngleDegrees();

                // Normalize angle to 0-180 range (since roads can go both ways)
                while (angle < 0) angle += 180;
                while (angle >= 180) angle -= 180;

                // Round to 10-degree bins
                int angleBin = (int)(angle / 10) * 10;
                angleHistogram[angleBin]++;
            }
        }

        // Find the two most common directions
        vector<pair<int, int>> angleFreqs(angleHistogram.begin(), angleHistogram.end());
        sort(angleFreqs.begin(), angleFreqs.end(),
            [](const pair<int, int>& a, const pair<int, int>& b) {
                return a.second > b.second;
            });

        if (!angleFreqs.empty()) {
            // Primary direction
            float primaryAngle = angleFreqs[0].first * M_PI / 180.0f;
            networkAnalysis.primaryDirection = Point2D(cos(primaryAngle), sin(primaryAngle));

            // Secondary direction (look for perpendicular)
            if (angleFreqs.size() > 1) {
                float secondaryAngle = angleFreqs[1].first * M_PI / 180.0f;
                networkAnalysis.secondaryDirection = Point2D(cos(secondaryAngle), sin(secondaryAngle));

                // Check if directions are roughly perpendicular
                float dotProduct = abs(networkAnalysis.primaryDirection.x * networkAnalysis.secondaryDirection.x +
                    networkAnalysis.primaryDirection.y * networkAnalysis.secondaryDirection.y);

                if (dotProduct < 0.2f) { // Roughly perpendicular
                    networkAnalysis.gridRegularity = (float)(angleFreqs[0].second + angleFreqs[1].second) / analyzedSegments.size();
                }
                else {
                    // Not perpendicular, make secondary perpendicular to primary
                    networkAnalysis.secondaryDirection = Point2D(-networkAnalysis.primaryDirection.y, networkAnalysis.primaryDirection.x);
                    networkAnalysis.gridRegularity = (float)angleFreqs[0].second / analyzedSegments.size();
                }
            }
            else {
                // Only one dominant direction found
                networkAnalysis.secondaryDirection = Point2D(-networkAnalysis.primaryDirection.y, networkAnalysis.primaryDirection.x);
                networkAnalysis.gridRegularity = (float)angleFreqs[0].second / analyzedSegments.size();
            }
        }
        else {
            // No clear direction found, use default
            networkAnalysis.primaryDirection = Point2D(1, 0);
            networkAnalysis.secondaryDirection = Point2D(0, 1);
            networkAnalysis.gridRegularity = 0;
        }

        // Calculate directional bias
        networkAnalysis.directionalBias = networkAnalysis.gridRegularity;
    }

    void calculateConnectivity() {
        if (intersections.empty()) {
            networkAnalysis.connectivity = 0;
            return;
        }

        float totalConnections = 0;
        for (const auto& intersection : intersections) {
            totalConnections += intersection.connectedRoadIds.size();
        }

        networkAnalysis.connectivity = totalConnections / intersections.size();
    }

    void calculateNetworkDensity() {
        if (analyzedSegments.empty()) {
            networkAnalysis.density = 0;
            return;
        }

        // Calculate total road length
        float totalLength = 0;
        for (const auto& segment : analyzedSegments) {
            totalLength += segment.length;
        }

        // Estimate coverage area (simplified as bounding box)
        float minX = analyzedSegments[0].start.x, maxX = analyzedSegments[0].start.x;
        float minY = analyzedSegments[0].start.y, maxY = analyzedSegments[0].start.y;

        for (const auto& segment : analyzedSegments) {
            minX = min(minX, min(segment.start.x, segment.end.x));
            maxX = max(maxX, max(segment.start.x, segment.end.x));
            minY = min(minY, min(segment.start.y, segment.end.y));
            maxY = max(maxY, max(segment.start.y, segment.end.y));
        }

        float area = (maxX - minX) * (maxY - minY);
        networkAnalysis.density = totalLength / area;

        // Estimate average block size
        float avgIntersectionSpacing = 0;
        int spacingCount = 0;

        for (const auto& intersection : intersections) {
            for (const auto& other : intersections) {
                if (&intersection != &other) {
                    float distance = intersection.location.distance(other.location);
                    if (distance < 30) { // Scaled from 300 - Only consider nearby intersections
                        avgIntersectionSpacing += distance;
                        spacingCount++;
                    }
                }
            }
        }

        if (spacingCount > 0) {
            networkAnalysis.averageBlockSize = avgIntersectionSpacing / spacingCount;
        }
        else {
            networkAnalysis.averageBlockSize = 10; // Scaled from 100 - Default value
        }
    }

    void determineNetworkPattern() {
        if (networkAnalysis.gridRegularity > 0.6f) {
            networkAnalysis.dominantPattern = NetworkPattern::GRID_PATTERN;
        }
        else if (networkAnalysis.connectivity > 3.5f) {
            networkAnalysis.dominantPattern = NetworkPattern::RADIAL_PATTERN;
        }
        else if (networkAnalysis.directionalBias > 0.4f) {
            networkAnalysis.dominantPattern = NetworkPattern::LINEAR_PATTERN;
        }
        else {
            networkAnalysis.dominantPattern = NetworkPattern::ORGANIC_PATTERN;
        }
    }

    void calculateSegmentImportance() {
        for (auto& segment : analyzedSegments) {
            float importance = 0;

            // Base importance from classification
            switch (segment.classification) {
            case RoadClassification::PRIMARY_ARTERIAL:
                importance = 1.0f;
                break;
            case RoadClassification::SECONDARY_ARTERIAL:
                importance = 0.8f;
                break;
            case RoadClassification::COLLECTOR_ROAD:
                importance = 0.6f;
                break;
            case RoadClassification::LOCAL_STREET:
                importance = 0.4f;
                break;
            default:
                importance = 0.2f;
                break;
            }

            // Bonus for being connected to intersections
            importance += segment.intersectionIds.size() * 0.1f;

            // Bonus for longer segments (scaled threshold)
            importance += min(0.3f, segment.length / 20.0f); // Scaled from 200.0f

            segment.importance = importance;
        }
    }
};

#endif // ROAD_ANALYZER_H