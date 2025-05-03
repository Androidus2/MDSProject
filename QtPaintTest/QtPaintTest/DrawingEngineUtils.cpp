#include "DrawingEngineUtils.h"

// Function to convert QPainterPath to Clipper2Lib::PathsD
Clipper2Lib::Path64 DrawingEngineUtils::convertPathToClipper(const QPainterPath& path) {
    Clipper2Lib::Path64 result;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element& el = path.elementAt(i);
        result.emplace_back(
            static_cast<int64_t>(el.x * CLIPPER_SCALING),
            static_cast<int64_t>(el.y * CLIPPER_SCALING)
        );
    }
    return result;
}
// Function to convert Clipper2Lib::PathsD to QPainterPath
QPainterPath DrawingEngineUtils::convertSingleClipperPath(const Clipper2Lib::Path64& path) {
    QPainterPath result;
    if (path.empty()) return result;

    result.moveTo(
        static_cast<qreal>(path[0].x) / CLIPPER_SCALING,
        static_cast<qreal>(path[0].y) / CLIPPER_SCALING
    );

    for (size_t i = 1; i < path.size(); ++i) {
        result.lineTo(
            static_cast<qreal>(path[i].x) / CLIPPER_SCALING,
            static_cast<qreal>(path[i].y) / CLIPPER_SCALING
        );
    }

    if (path.size() > 2) {
        result.closeSubpath();
    }

    return result;
}