#ifndef BUILDING_FOOTPRINT_GENERATOR_H
#define BUILDING_FOOTPRINT_GENERATOR_H

// Include dependencies
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"
#include "ParcelSubdivider.h"

// Standard Library Includes
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <cmath>
#include <random>

// OpenGL includes
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

using namespace std;

// ===================== BUILDING FOOTPRINT STRUCTURES =====================

// Structure to represent a building footprint
struct BuildingFootprint {
    int id;
    int parentPlotId;
    vector<Point2D> boundary;    // 2D boundary points
    Point2D center;              // Center point
    float area;                  // Footprint area
    float coverage;              // Coverage ratio (footprint/plot area)

    // Edge conditions
    bool hasMajorRoadFrontage;
    bool hasWaterFrontage;
    bool hasParkFrontage;
    SiteEdgeType primaryEdgeType;
    SiteCornerType cornerType;

    // Setback information
    float frontSetback;          // Setback from primary road/front
    float rearSetback;           // Setback from rear boundary
    float sideSetback;           // Setback from side boundaries
    float waterSetback;          // Setback from water edge
    float parkSetback;           // Setback from park edge

    // Additional properties
    Point2D primaryOrientation;  // Main building orientation vector
    vector<Point2D> entrances;   // Entrance points

    BuildingFootprint() : id(-1), parentPlotId(-1), area(0), coverage(0),
        hasMajorRoadFrontage(false), hasWaterFrontage(false), hasParkFrontage(false),
        primaryEdgeType(SiteEdgeType::INTERNAL_EDGE), cornerType(SiteCornerType::NO_CORNER),
        frontSetback(0.5f), rearSetback(0.5f), sideSetback(0.5f),
        waterSetback(1.0f), parkSetback(0.8f) {
    }

    void calculateBasicProperties() {
        // Calculate area using shoelace formula
        area = 0;
        if (boundary.size() < 3) return;

        for (size_t i = 0; i < boundary.size(); ++i) {
            size_t j = (i + 1) % boundary.size();
            area += boundary[i].x * boundary[j].y;
            area -= boundary[j].x * boundary[i].y;
        }
        area = abs(area) * 0.5f;

        // Calculate centroid
        float cx = 0, cy = 0;
        for (const auto& pt : boundary) {
            cx += pt.x;
            cy += pt.y;
        }
        center = Point2D(cx / boundary.size(), cy / boundary.size());
    }
};

// ===================== BUILDING FOOTPRINT GENERATOR CLASS =====================

class BuildingFootprintGenerator {
private:
    // Dependencies
    ParcelSubdivider* parcelSubdivider;
    ParcelAnalyzer* parcelAnalyzer;
    RoadAnalyzer* roadAnalyzer;

    // Generated footprints
    vector<BuildingFootprint> buildingFootprints;
    int nextFootprintId;

    // Default setback parameters (scaled: 1 unit = 10m)
    float defaultFrontSetback;
    float defaultRearSetback;
    float defaultSideSetback;
    float defaultWaterSetback;
    float defaultParkSetback;
    float defaultCornerSetback;

    // Coverage parameters
    float maxCoverageRatio;
    float minCoverageRatio;

    // Random number generator for variations
    std::random_device rd;
    std::mt19937 rng;

    // Verbosity flag
    bool verboseLogging;

public:
    // ================== CONSTRUCTOR & DESTRUCTOR ==================

    BuildingFootprintGenerator(ParcelSubdivider* subdivider,
        ParcelAnalyzer* analyzer,
        RoadAnalyzer* roadAnalyzer)
        : parcelSubdivider(subdivider), parcelAnalyzer(analyzer), roadAnalyzer(roadAnalyzer),
        nextFootprintId(0), defaultFrontSetback(0.5f), defaultRearSetback(0.7f),
        defaultSideSetback(0.5f), defaultWaterSetback(1.0f), defaultParkSetback(0.8f),
        defaultCornerSetback(0.8f), maxCoverageRatio(0.8f), minCoverageRatio(0.5f),
        verboseLogging(false), rng(rd()) {

        cout << "=== BuildingFootprintGenerator Initialized ===" << endl;
    }

    ~BuildingFootprintGenerator() {
        cout << "=== BuildingFootprintGenerator Destroyed ===" << endl;
    }

    // ================== MAIN GENERATION METHODS ==================

    // Generate footprints for all plots
    void generateAllFootprints() {
        cout << "\n=== GENERATING BUILDING FOOTPRINTS ===" << endl;

        if (!validateDependencies()) {
            cout << "Error: Required dependencies not available" << endl;
            return;
        }

        buildingFootprints.clear();
        nextFootprintId = 0;

        // Get all plots from the subdivider
        const auto& plots = parcelSubdivider->getAllPlots();

        for (const auto& plot : plots) {
            // Skip open spaces and non-viable plots
            if (plot.isOpenSpace || !plot.isViablePlot()) {
                if (verboseLogging) {
                    cout << "Skipping plot " << plot.id << ": "
                        << (plot.isOpenSpace ? "Open Space" : "Not viable") << endl;
                }
                continue;
            }

            BuildingFootprint footprint = generateFootprintForPlot(plot);

            if (footprint.boundary.size() >= 3) {
                footprint.id = nextFootprintId++;
                buildingFootprints.push_back(footprint);

                if (verboseLogging) {
                    cout << "Generated footprint " << footprint.id
                        << " for plot " << plot.id
                        << " (Area: " << footprint.area << " unit²)" << endl;
                }
            }
        }

        cout << "Generated " << buildingFootprints.size() << " building footprints" << endl;
        printFootprintStatistics();
    }

    // Generate a footprint for a specific plot
    BuildingFootprint generateFootprintForPlot(const Plot& plot) {
        BuildingFootprint footprint;
        footprint.parentPlotId = plot.id;

        // Get edge and corner conditions from the plot
        determineEdgeConditions(plot, footprint);

        // Calculate setbacks based on edge conditions
        calculateSetbacks(plot, footprint);

        // Generate the building boundary with setbacks
        generateBuildingBoundary(plot, footprint);

        // Calculate basic properties (area, center)
        footprint.calculateBasicProperties();

        // Calculate coverage ratio
        if (plot.area > 0) {
            footprint.coverage = footprint.area / plot.area;
        }

        // Determine primary orientation based on road frontage
        determinePrimaryOrientation(plot, footprint);

        // Generate entrances
        generateEntrances(footprint);

        return footprint;
    }

    // ================== EDGE CONDITION METHODS ==================

    // Determine edge conditions for a footprint based on plot analysis
    void determineEdgeConditions(const Plot& plot, BuildingFootprint& footprint) {
        // Get analyzed plot from parcel analyzer (if available)
        const AnalyzedPlot* analyzedPlot = nullptr;
        if (parcelAnalyzer) {
            analyzedPlot = parcelAnalyzer->getAnalyzedPlot(plot.parentParcelId);
        }

        if (analyzedPlot) {
            // Transfer edge conditions from analyzed plot
            footprint.primaryEdgeType = analyzedPlot->edgeCondition;
            footprint.cornerType = analyzedPlot->cornerCondition;

            // Set edge flags based on edge condition
            footprint.hasWaterFrontage = (footprint.primaryEdgeType == SiteEdgeType::WATER_EDGE);
            footprint.hasParkFrontage = (footprint.primaryEdgeType == SiteEdgeType::PARK_EDGE);
            footprint.hasMajorRoadFrontage = (footprint.primaryEdgeType == SiteEdgeType::STREET_EDGE);

            // For mixed edge types, check adjacent roads
            if (footprint.primaryEdgeType == SiteEdgeType::MIXED_EDGE) {
                for (const auto& road : analyzedPlot->adjacentRoads) {
                    if (road.segment.hierarchy == RoadHierarchy::MAJOR_ARTERIAL ||
                        road.segment.hierarchy == RoadHierarchy::MINOR_ARTERIAL) {
                        footprint.hasMajorRoadFrontage = true;
                        break;
                    }
                }
            }
        }
        else {
            // Use plot data directly if no analyzed plot is available
            footprint.hasMajorRoadFrontage = plot.hasMajorRoadFrontage;

            // Default edge and corner types
            footprint.primaryEdgeType = SiteEdgeType::INTERNAL_EDGE;
            footprint.cornerType = SiteCornerType::NO_CORNER;
        }
    }

    // Calculate appropriate setbacks based on edge conditions
    void calculateSetbacks(const Plot& plot, BuildingFootprint& footprint) {
        // Initialize with default values
        footprint.frontSetback = defaultFrontSetback;
        footprint.rearSetback = defaultRearSetback;
        footprint.sideSetback = defaultSideSetback;
        footprint.waterSetback = defaultWaterSetback;
        footprint.parkSetback = defaultParkSetback;

        // Adjust based on parcel category (larger parcels get larger setbacks)
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            footprint.frontSetback *= 1.2f;
            footprint.rearSetback *= 1.2f;
            footprint.sideSetback *= 1.2f;
            break;

        case MEDIUM_PARCEL:
            // Keep default setbacks
            break;

        case SMALL_PARCEL:
            footprint.frontSetback *= 0.8f;
            footprint.rearSetback *= 0.8f;
            footprint.sideSetback *= 0.8f;
            break;
        }

        // Adjust based on edge conditions
        if (footprint.hasWaterFrontage) {
            footprint.waterSetback = defaultWaterSetback;
        }

        if (footprint.hasParkFrontage) {
            footprint.parkSetback = defaultParkSetback;
        }

        if (footprint.hasMajorRoadFrontage) {
            // Smaller setback for major roads to maximize frontage
            footprint.frontSetback = defaultFrontSetback * 0.8f;
        }

        // Adjust for corner conditions
        if (footprint.cornerType != SiteCornerType::NO_CORNER) {
            // Increase setback at corners for articulation
            switch (footprint.cornerType) {
            case SiteCornerType::PLAZA_CORNER:
                footprint.frontSetback *= 1.5f; // Larger setback for plazas
                break;

            case SiteCornerType::STREET_CORNER:
                footprint.frontSetback *= 1.2f; // Moderate increase
                break;

            default:
                footprint.frontSetback *= 1.1f; // Small increase
                break;
            }
        }

        // Add slight random variation for visual interest
        std::uniform_real_distribution<float> variation(0.9f, 1.1f);
        footprint.frontSetback *= variation(rng);
        footprint.rearSetback *= variation(rng);
        footprint.sideSetback *= variation(rng);
    }

    // ================== BOUNDARY GENERATION METHODS ==================

    // Generate building boundary with appropriate setbacks
    void generateBuildingBoundary(const Plot& plot, BuildingFootprint& footprint) {
        // Start with the plot boundary
        vector<Point2D> plotBoundary = plot.boundary;
        if (plotBoundary.size() < 3) {
            cout << "Warning: Plot " << plot.id << " has invalid boundary." << endl;
            return;
        }

        // Apply different setback methods based on plot shape and edge conditions
        if (isRegularPolygon(plotBoundary, 4, 0.2f)) {
            // For rectangular-ish plots, use the simple offset method
            footprint.boundary = generateOffsetPolygon(plotBoundary,
                footprint.frontSetback,
                footprint.rearSetback,
                footprint.sideSetback);
        }
        else {
            // For irregular plots, use uniform offset with adjustments
            footprint.boundary = generateUniformOffsetPolygon(plotBoundary, footprint.sideSetback);

            // Make additional edge-specific adjustments
            adjustBoundaryForEdgeConditions(plot, footprint);
        }

        // Validate and clean up the resulting boundary
        cleanupBoundary(footprint.boundary);

        // Ensure coverage is within limits
        adjustCoverageIfNeeded(plot, footprint);
    }

    // Generate a polygon offset from the original with different setbacks for front/rear/sides
    vector<Point2D> generateOffsetPolygon(const vector<Point2D>& original,
        float frontSetback,
        float rearSetback,
        float sideSetback) {
        if (original.size() < 3) return {};

        // Calculate centroid
        Point2D centroid;
        for (const auto& pt : original) {
            centroid.x += pt.x;
            centroid.y += pt.y;
        }
        centroid.x /= original.size();
        centroid.y /= original.size();

        // Find primary axis (longest edge or road-facing edge)
        int primaryEdgeIndex = findPrimaryEdgeIndex(original);

        // Create offset polygon
        vector<Point2D> result;
        for (size_t i = 0; i < original.size(); ++i) {
            // Determine if this edge is front, rear, or side
            float setback;
            if (i == primaryEdgeIndex) {
                setback = frontSetback; // Front edge
            }
            else if (i == (primaryEdgeIndex + original.size() / 2) % original.size()) {
                setback = rearSetback; // Opposite to front (rear)
            }
            else {
                setback = sideSetback; // Side edge
            }

            // Calculate inward normal vector for this edge
            size_t next = (i + 1) % original.size();
            Point2D edge = { original[next].x - original[i].x, original[next].y - original[i].y };
            float edgeLength = sqrt(edge.x * edge.x + edge.y * edge.y);

            if (edgeLength < 0.001f) continue; // Skip very short edges

            // Normalize the edge vector
            edge.x /= edgeLength;
            edge.y /= edgeLength;

            // Calculate normal (perpendicular) vector pointing inward
            Point2D normal = { -edge.y, edge.x };

            // Check if normal points towards or away from centroid
            Point2D edgeCenter = {
                (original[i].x + original[next].x) * 0.5f,
                (original[i].y + original[next].y) * 0.5f
            };

            Point2D toCentroid = {
                centroid.x - edgeCenter.x,
                centroid.y - edgeCenter.y
            };

            float dotProduct = normal.x * toCentroid.x + normal.y * toCentroid.y;

            // Flip normal if it points outward
            if (dotProduct < 0) {
                normal.x = -normal.x;
                normal.y = -normal.y;
            }

            // Offset the current point
            Point2D offsetPoint = {
                original[i].x + normal.x * setback,
                original[i].y + normal.y * setback
            };

            result.push_back(offsetPoint);
        }

        return result;
    }

    // Generate a polygon with uniform offset from the original
    vector<Point2D> generateUniformOffsetPolygon(const vector<Point2D>& original, float offset) {
        if (original.size() < 3) return {};

        // Calculate centroid
        Point2D centroid;
        for (const auto& pt : original) {
            centroid.x += pt.x;
            centroid.y += pt.y;
        }
        centroid.x /= original.size();
        centroid.y /= original.size();

        // Create scaled-down polygon (simple approach)
        vector<Point2D> result;
        for (const auto& pt : original) {
            // Calculate vector from centroid to point
            Point2D vec = {
                pt.x - centroid.x,
                pt.y - centroid.y
            };

            // Calculate distance from centroid to point
            float distance = sqrt(vec.x * vec.x + vec.y * vec.y);

            if (distance < offset) {
                // If point is too close to centroid, skip to avoid inversion
                continue;
            }

            // Calculate scaling factor to achieve desired offset
            float scaleFactor = (distance - offset) / distance;

            // Create new point
            Point2D newPoint = {
                centroid.x + vec.x * scaleFactor,
                centroid.y + vec.y * scaleFactor
            };

            result.push_back(newPoint);
        }

        // Ensure we have at least 3 points
        if (result.size() < 3) {
            // Fallback to a simple rectangle or triangle
            float fallbackSize = 1.0f; // 10m x 10m
            result = {
                {centroid.x - fallbackSize / 2, centroid.y - fallbackSize / 2},
                {centroid.x + fallbackSize / 2, centroid.y - fallbackSize / 2},
                {centroid.x + fallbackSize / 2, centroid.y + fallbackSize / 2},
                {centroid.x - fallbackSize / 2, centroid.y + fallbackSize / 2}
            };
        }

        return result;
    }

    // Find the index of the primary edge (longest or road-facing)
    int findPrimaryEdgeIndex(const vector<Point2D>& polygon) {
        if (polygon.size() < 3) return 0;

        int primaryIndex = 0;
        float maxLength = 0;

        for (size_t i = 0; i < polygon.size(); ++i) {
            size_t next = (i + 1) % polygon.size();

            // Calculate edge length
            float dx = polygon[next].x - polygon[i].x;
            float dy = polygon[next].y - polygon[i].y;
            float length = sqrt(dx * dx + dy * dy);

            if (length > maxLength) {
                maxLength = length;
                primaryIndex = i;
            }
        }

        return primaryIndex;
    }

    // Make specific adjustments to the boundary for water/park/road edges
    void adjustBoundaryForEdgeConditions(const Plot& plot, BuildingFootprint& footprint) {
        if (footprint.boundary.size() < 3) return;

        // Find edges that face specific conditions
        vector<int> waterEdges, parkEdges, roadEdges;

        // Identify which edges face which conditions
        // In a production system, this would use more sophisticated detection

        // For now, we'll use a simplified approach based on edge condition flags
        if (footprint.hasWaterFrontage) {
            // Find edge closest to water (simplified)
            int waterEdgeIndex = findPrimaryEdgeIndex(footprint.boundary);
            waterEdges.push_back(waterEdgeIndex);

            // Apply water setback to this edge
            applySetbackToEdge(footprint.boundary, waterEdgeIndex, footprint.waterSetback);
        }

        if (footprint.hasParkFrontage) {
            // In a real implementation, we'd identify the park-facing edge
            // For now, use an edge not already used for water
            int parkEdgeIndex = (findPrimaryEdgeIndex(footprint.boundary) + 1) % footprint.boundary.size();
            parkEdges.push_back(parkEdgeIndex);

            // Apply park setback to this edge
            applySetbackToEdge(footprint.boundary, parkEdgeIndex, footprint.parkSetback);
        }

        if (footprint.hasMajorRoadFrontage) {
            // Identify road-facing edge (in this simplified version, we'll use the longest edge)
            int roadEdgeIndex = findPrimaryEdgeIndex(footprint.boundary);

            // Don't double-apply if already used for water
            if (std::find(waterEdges.begin(), waterEdges.end(), roadEdgeIndex) == waterEdges.end()) {
                roadEdges.push_back(roadEdgeIndex);

                // Apply road setback to this edge
                applySetbackToEdge(footprint.boundary, roadEdgeIndex, footprint.frontSetback);
            }
        }
    }

    // Apply a specific setback to a single edge of the polygon
    void applySetbackToEdge(vector<Point2D>& polygon, int edgeIndex, float setback) {
        if (polygon.size() < 3) return;

        size_t next = (edgeIndex + 1) % polygon.size();

        // Get edge vector
        Point2D edge = {
            polygon[next].x - polygon[edgeIndex].x,
            polygon[next].y - polygon[edgeIndex].y
        };

        // Calculate length
        float length = sqrt(edge.x * edge.x + edge.y * edge.y);

        if (length < 0.001f) return; // Skip very short edges

        // Calculate normal vector
        Point2D normal = { -edge.y / length, edge.x / length };

        // Calculate centroid to determine if normal points inward
        Point2D centroid = { 0, 0 };
        for (const auto& pt : polygon) {
            centroid.x += pt.x;
            centroid.y += pt.y;
        }
        centroid.x /= polygon.size();
        centroid.y /= polygon.size();

        // Determine if normal points inward
        Point2D edgeCenter = {
            (polygon[edgeIndex].x + polygon[next].x) * 0.5f,
            (polygon[edgeIndex].y + polygon[next].y) * 0.5f
        };

        Point2D toCentroid = {
            centroid.x - edgeCenter.x,
            centroid.y - edgeCenter.y
        };

        float dotProduct = normal.x * toCentroid.x + normal.y * toCentroid.y;

        // Ensure normal points inward
        if (dotProduct < 0) {
            normal.x = -normal.x;
            normal.y = -normal.y;
        }

        // Move the two points of the edge
        polygon[edgeIndex].x += normal.x * setback;
        polygon[edgeIndex].y += normal.y * setback;

        polygon[next].x += normal.x * setback;
        polygon[next].y += normal.y * setback;
    }

    // Clean up the boundary by removing duplicate or very close points
    void cleanupBoundary(vector<Point2D>& boundary) {
        if (boundary.size() < 4) return; // Need at least a triangle

        vector<Point2D> cleaned;
        const float MIN_DISTANCE = 0.1f; // Minimum distance to consider points distinct

        for (size_t i = 0; i < boundary.size(); ++i) {
            // Get next point (wrapping around)
            size_t next = (i + 1) % boundary.size();

            // Calculate distance to next point
            float dx = boundary[next].x - boundary[i].x;
            float dy = boundary[next].y - boundary[i].y;
            float distance = sqrt(dx * dx + dy * dy);

            // Only keep this point if it's not too close to the next one
            if (distance > MIN_DISTANCE) {
                cleaned.push_back(boundary[i]);
            }
        }

        // If we have at least 3 points, update the boundary
        if (cleaned.size() >= 3) {
            boundary = cleaned;
        }
    }

    // ================== COVERAGE ADJUSTMENT METHODS ==================

    // Adjust the building footprint to ensure coverage is within limits
    void adjustCoverageIfNeeded(const Plot& plot, BuildingFootprint& footprint) {
        // Calculate current coverage ratio
        float currentCoverage = footprint.area / plot.area;

        // If coverage is outside the desired range, adjust it
        if (currentCoverage > maxCoverageRatio) {
            // Coverage too high, reduce footprint size
            float scaleFactor = sqrt(maxCoverageRatio / currentCoverage);
            scaleFootprint(footprint, scaleFactor);
        }
        else if (currentCoverage < minCoverageRatio) {
            // Coverage too low, increase footprint size
            float scaleFactor = sqrt(minCoverageRatio / currentCoverage);

            // Try to increase, but check if it still fits within the plot
            BuildingFootprint testFootprint = footprint;
            scaleFootprint(testFootprint, scaleFactor);

            // Check if expanded footprint still fits in plot
            if (footprintFitsInPlot(testFootprint, plot)) {
                footprint = testFootprint;
            }
            else {
                // If it doesn't fit, use the original
                if (verboseLogging) {
                    cout << "Warning: Cannot increase footprint size to meet minimum coverage for plot "
                        << plot.id << endl;
                }
            }
        }

        // Recalculate area and coverage
        footprint.calculateBasicProperties();
        footprint.coverage = footprint.area / plot.area;
    }

    // Scale a footprint around its center
    void scaleFootprint(BuildingFootprint& footprint, float scaleFactor) {
        if (footprint.boundary.size() < 3) return;

        // Ensure we have center calculated
        if (footprint.center.x == 0 && footprint.center.y == 0) {
            float cx = 0, cy = 0;
            for (const auto& pt : footprint.boundary) {
                cx += pt.x;
                cy += pt.y;
            }
            footprint.center = { cx / footprint.boundary.size(), cy / footprint.boundary.size() };
        }

        // Scale each point
        for (auto& pt : footprint.boundary) {
            // Vector from center to point
            float dx = pt.x - footprint.center.x;
            float dy = pt.y - footprint.center.y;

            // Scale the vector
            dx *= scaleFactor;
            dy *= scaleFactor;

            // Update point position
            pt.x = footprint.center.x + dx;
            pt.y = footprint.center.y + dy;
        }
    }

    // Check if a footprint fits within a plot
    bool footprintFitsInPlot(const BuildingFootprint& footprint, const Plot& plot) {
        if (footprint.boundary.size() < 3 || plot.boundary.size() < 3) return false;

        // Check if all footprint points are inside the plot
        for (const auto& pt : footprint.boundary) {
            if (!pointInPolygon(pt, plot.boundary)) {
                return false;
            }
        }

        return true;
    }

    // ================== ORIENTATION & ENTRANCE METHODS ==================

    // Determine the primary orientation for the building
    void determinePrimaryOrientation(const Plot& plot, BuildingFootprint& footprint) {
        // Default to road direction if available
        if (plot.primaryRoadDirection.x != 0 || plot.primaryRoadDirection.y != 0) {
            footprint.primaryOrientation = plot.primaryRoadDirection;
            return;
        }

        // Otherwise use the longest edge direction
        if (footprint.boundary.size() >= 3) {
            int primaryEdgeIndex = findPrimaryEdgeIndex(footprint.boundary);
            size_t next = (primaryEdgeIndex + 1) % footprint.boundary.size();

            // Get edge direction
            float dx = footprint.boundary[next].x - footprint.boundary[primaryEdgeIndex].x;
            float dy = footprint.boundary[next].y - footprint.boundary[primaryEdgeIndex].y;
            float length = sqrt(dx * dx + dy * dy);

            if (length > 0.001f) {
                footprint.primaryOrientation = { dx / length, dy / length };
                return;
            }
        }

        // Fallback to default orientation
        footprint.primaryOrientation = { 1.0f, 0.0f }; // East
    }

    // Generate building entrances
    void generateEntrances(BuildingFootprint& footprint) {
        footprint.entrances.clear();

        if (footprint.boundary.size() < 3) return;

        // For now, just add one entrance at the midpoint of the primary (front) edge
        int primaryEdgeIndex = findPrimaryEdgeIndex(footprint.boundary);
        size_t next = (primaryEdgeIndex + 1) % footprint.boundary.size();

        // Midpoint of primary edge
        Point2D entrance = {
            (footprint.boundary[primaryEdgeIndex].x + footprint.boundary[next].x) * 0.5f,
            (footprint.boundary[primaryEdgeIndex].y + footprint.boundary[next].y) * 0.5f
        };

        footprint.entrances.push_back(entrance);

        // For corner buildings, add a second entrance at the corner
        if (footprint.cornerType != SiteCornerType::NO_CORNER) {
            // Find the corner point (simpliifed - just use a point from the boundary)
            Point2D cornerEntrance = footprint.boundary[primaryEdgeIndex];
            footprint.entrances.push_back(cornerEntrance);
        }
    }

    // ================== UTILITY METHODS ==================

    // Check if a point is inside a polygon
    bool pointInPolygon(const Point2D& point, const vector<Point2D>& polygon) {
        if (polygon.size() < 3) return false;

        bool inside = false;
        size_t j = polygon.size() - 1;

        for (size_t i = 0; i < polygon.size(); i++) {
            if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
                (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) /
                    (polygon[j].y - polygon[i].y) + polygon[i].x)) {
                inside = !inside;
            }
            j = i;
        }

        return inside;
    }

    // Check if a polygon is approximately regular (e.g., rectangle)
    bool isRegularPolygon(const vector<Point2D>& polygon, int sides, float tolerance) {
        if (polygon.size() != sides) return false;

        // Calculate edge lengths
        vector<float> edgeLengths;
        for (size_t i = 0; i < polygon.size(); ++i) {
            size_t next = (i + 1) % polygon.size();
            float dx = polygon[next].x - polygon[i].x;
            float dy = polygon[next].y - polygon[i].y;
            edgeLengths.push_back(sqrt(dx * dx + dy * dy));
        }

        // Calculate average length
        float avgLength = 0;
        for (float length : edgeLengths) {
            avgLength += length;
        }
        avgLength /= edgeLengths.size();

        // Check if all edges are within tolerance of average
        for (float length : edgeLengths) {
            if (abs(length - avgLength) / avgLength > tolerance) {
                return false;
            }
        }

        // For rectangles, check for right angles
        if (sides == 4) {
            // Check angles between consecutive edges
            for (size_t i = 0; i < polygon.size(); ++i) {
                size_t j = (i + 1) % polygon.size();
                size_t k = (i + 2) % polygon.size();

                // Get vectors for two consecutive edges
                float dx1 = polygon[j].x - polygon[i].x;
                float dy1 = polygon[j].y - polygon[i].y;

                float dx2 = polygon[k].x - polygon[j].x;
                float dy2 = polygon[k].y - polygon[j].y;

                // Calculate dot product
                float dotProduct = dx1 * dx2 + dy1 * dy2;

                // Calculate magnitudes
                float mag1 = sqrt(dx1 * dx1 + dy1 * dy1);
                float mag2 = sqrt(dx2 * dx2 + dy2 * dy2);

                // Calculate cosine of angle
                float cosAngle = dotProduct / (mag1 * mag2);

                // Check if angle is close to 90 degrees (cos(90) = 0)
                if (abs(cosAngle) > tolerance) {
                    return false;
                }
            }
        }

        return true;
    }

    // ================== ACCESSOR METHODS ==================

    const vector<BuildingFootprint>& getAllFootprints() const {
        return buildingFootprints;
    }

    const BuildingFootprint* getFootprintById(int id) const {
        for (const auto& footprint : buildingFootprints) {
            if (footprint.id == id) {
                return &footprint;
            }
        }
        return nullptr;
    }

    vector<BuildingFootprint> getFootprintsByPlotId(int plotId) const {
        vector<BuildingFootprint> result;
        for (const auto& footprint : buildingFootprints) {
            if (footprint.parentPlotId == plotId) {
                result.push_back(footprint);
            }
        }
        return result;
    }

    // ================== VISUALIZATION METHODS ==================

    void drawAllFootprints() {
        for (const auto& footprint : buildingFootprints) {
            drawFootprint(footprint);
        }
    }

    void drawFootprint(const BuildingFootprint& footprint) {
        if (footprint.boundary.size() < 3) return;

        // Draw filled footprint
        glColor4f(0.7f, 0.7f, 0.7f, 0.6f); // Light gray with transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(footprint.center.x, footprint.center.y, 0.04f);
        for (const auto& pt : footprint.boundary) {
            glVertex3f(pt.x, pt.y, 0.04f);
        }
        glVertex3f(footprint.boundary[0].x, footprint.boundary[0].y, 0.04f);
        glEnd();

        glDisable(GL_BLEND);

        // Draw footprint outline
        glColor3f(0.4f, 0.4f, 0.4f); // Dark gray
        glLineWidth(2.0f);

        glBegin(GL_LINE_LOOP);
        for (const auto& pt : footprint.boundary) {
            glVertex3f(pt.x, pt.y, 0.05f);
        }
        glEnd();

        glLineWidth(1.0f);

        // Draw entrances
        glColor3f(0.9f, 0.3f, 0.3f); // Red
        glPointSize(5.0f);

        glBegin(GL_POINTS);
        for (const auto& entrance : footprint.entrances) {
            glVertex3f(entrance.x, entrance.y, 0.06f);
        }
        glEnd();

        glPointSize(1.0f);

        // Draw orientation vector
        glColor3f(0.0f, 0.0f, 0.8f); // Blue
        glLineWidth(1.5f);

        glBegin(GL_LINES);
        glVertex3f(footprint.center.x, footprint.center.y, 0.06f);
        glVertex3f(footprint.center.x + footprint.primaryOrientation.x * 2.0f,
            footprint.center.y + footprint.primaryOrientation.y * 2.0f,
            0.06f);
        glEnd();

        glLineWidth(1.0f);
    }

    void drawFootprintsByEdgeType() {
        for (const auto& footprint : buildingFootprints) {
            // Color based on edge type
            switch (footprint.primaryEdgeType) {
            case SiteEdgeType::WATER_EDGE:
                glColor4f(0.0f, 0.5f, 0.8f, 0.6f); // Blue
                break;

            case SiteEdgeType::PARK_EDGE:
                glColor4f(0.0f, 0.8f, 0.4f, 0.6f); // Green
                break;

            case SiteEdgeType::STREET_EDGE:
                glColor4f(0.8f, 0.5f, 0.0f, 0.6f); // Orange
                break;

            case SiteEdgeType::MIXED_EDGE:
                glColor4f(0.8f, 0.3f, 0.8f, 0.6f); // Purple
                break;

            default:
                glColor4f(0.7f, 0.7f, 0.7f, 0.6f); // Gray
                break;
            }

            // Draw filled footprint
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (footprint.boundary.size() >= 3) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(footprint.center.x, footprint.center.y, 0.04f);
                for (const auto& pt : footprint.boundary) {
                    glVertex3f(pt.x, pt.y, 0.04f);
                }
                glVertex3f(footprint.boundary[0].x, footprint.boundary[0].y, 0.04f);
                glEnd();
            }

            glDisable(GL_BLEND);

            // Draw outline
            glColor3f(0.3f, 0.3f, 0.3f);
            glLineWidth(2.0f);

            glBegin(GL_LINE_LOOP);
            for (const auto& pt : footprint.boundary) {
                glVertex3f(pt.x, pt.y, 0.05f);
            }
            glEnd();

            glLineWidth(1.0f);
        }
    }

    // ================== VALIDATION & STATISTICS ==================

    bool validateDependencies() const {
        if (!parcelSubdivider) {
            cout << "Error: ParcelSubdivider not initialized" << endl;
            return false;
        }

        return true;
    }

    void printFootprintStatistics() {
        cout << "\n=== BUILDING FOOTPRINT STATISTICS ===" << endl;

        if (buildingFootprints.empty()) {
            cout << "No building footprints generated." << endl;
            return;
        }

        // Calculate total, min, max, and average areas
        float totalArea = 0;
        float minArea = buildingFootprints[0].area;
        float maxArea = buildingFootprints[0].area;

        // Count by edge type
        int waterEdgeCount = 0;
        int parkEdgeCount = 0;
        int streetEdgeCount = 0;
        int mixedEdgeCount = 0;
        int internalEdgeCount = 0;

        for (const auto& footprint : buildingFootprints) {
            totalArea += footprint.area;
            minArea = min(minArea, footprint.area);
            maxArea = max(maxArea, footprint.area);

            // Count by edge type
            switch (footprint.primaryEdgeType) {
            case SiteEdgeType::WATER_EDGE:
                waterEdgeCount++;
                break;

            case SiteEdgeType::PARK_EDGE:
                parkEdgeCount++;
                break;

            case SiteEdgeType::STREET_EDGE:
                streetEdgeCount++;
                break;

            case SiteEdgeType::MIXED_EDGE:
                mixedEdgeCount++;
                break;

            case SiteEdgeType::INTERNAL_EDGE:
                internalEdgeCount++;
                break;
            }
        }

        float avgArea = totalArea / buildingFootprints.size();

        cout << "Building Footprint Count: " << buildingFootprints.size() << endl;
        cout << "Total Footprint Area: " << totalArea << " unit² (" << totalArea * 10 << "m²)" << endl;
        cout << "Average Footprint Area: " << avgArea << " unit² (" << avgArea * 10 << "m²)" << endl;
        cout << "Min Footprint Area: " << minArea << " unit² (" << minArea * 10 << "m²)" << endl;
        cout << "Max Footprint Area: " << maxArea << " unit² (" << maxArea * 10 << "m²)" << endl;

        cout << "\nFootprints by Edge Type:" << endl;
        cout << "  Water Edge: " << waterEdgeCount << endl;
        cout << "  Park Edge: " << parkEdgeCount << endl;
        cout << "  Street Edge: " << streetEdgeCount << endl;
        cout << "  Mixed Edge: " << mixedEdgeCount << endl;
        cout << "  Internal Edge: " << internalEdgeCount << endl;

        cout << "=================================" << endl;
    }

    // ================== PARAMETER ADJUSTMENT ==================

    // Set default setback values
    void setDefaultSetbacks(float front, float rear, float side, float water, float park) {
        defaultFrontSetback = front;
        defaultRearSetback = rear;
        defaultSideSetback = side;
        defaultWaterSetback = water;
        defaultParkSetback = park;

        cout << "Updated default setbacks: front=" << front
            << ", rear=" << rear
            << ", side=" << side
            << ", water=" << water
            << ", park=" << park << endl;
    }

    // Set coverage ratio limits
    void setCoverageRatios(float min, float max) {
        minCoverageRatio = min;
        maxCoverageRatio = max;

        cout << "Updated coverage ratios: min=" << min << ", max=" << max << endl;
    }

    // Set verbosity level
    void setVerboseLogging(bool verbose) {
        verboseLogging = verbose;
        cout << "Verbose logging " << (verbose ? "enabled" : "disabled") << endl;
    }
};

#endif // BUILDING_FOOTPRINT_GENERATOR_H