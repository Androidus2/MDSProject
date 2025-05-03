#include "StrokeItem.h"

StrokeItem::StrokeItem(const QColor& color, qreal width)
    : m_color(color), m_width(width), m_isOutlined(false)
{
    QPen pen(m_color, m_width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    setPen(pen);
	if (width > 0) {
		setBrush(Qt::NoBrush); // No fill for stroke
	}
	else {
		setBrush(QBrush(m_color)); // Fill with color if width is 0
	}
}

void StrokeItem::setOutlined(bool outlined) {
    m_isOutlined = outlined;
}

void StrokeItem::convertToFilledPath() {
    if (m_isOutlined) return;

    // Create a stroker to convert the path to an outline
    QPainterPathStroker stroker;
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setWidth(m_width);

    // Get the stroked outline path
    QPainterPath outlinePath = stroker.createStroke(path());

    // Convert to Clipper2 format
    Clipper2Lib::Path64 clipperPath = DrawingEngineUtils::convertPathToClipper(outlinePath);

    // Create a paths collection with our path
    Clipper2Lib::Paths64 subj;
    subj.push_back(clipperPath);

    // Use Clipper2 to simplify via union operation
    Clipper2Lib::Clipper64 clipper;
    clipper.PreserveCollinear(true); // Keep collinear points to preserve shape
    clipper.AddSubject(subj);

    Clipper2Lib::Paths64 solution;
    clipper.Execute(Clipper2Lib::ClipType::Union,
        Clipper2Lib::FillRule::NonZero,
        solution);

    // If we got results, convert the paths back
    if (!solution.empty()) {
        QPainterPath simplifiedPath;

        // Process each resulting path
        for (const auto& resultPath : solution) {
            QPainterPath subPath = DrawingEngineUtils::convertSingleClipperPath(resultPath);

            // First path is the outline, subsequent paths are holes
            if (simplifiedPath.isEmpty()) {
                simplifiedPath = subPath;
            }
            else {
                // Check orientation to determine if it's a hole
                // Using area calculation to determine if inside or outside
                if (Clipper2Lib::Area(resultPath) < 0) {
                    simplifiedPath = simplifiedPath.subtracted(subPath);
                }
                else {
                    simplifiedPath = simplifiedPath.united(subPath);
                }
            }
        }

        setPath(simplifiedPath);
    }

    // Update appearance - fill with color, thin outline
    setBrush(QBrush(m_color));
    setPen(QPen(m_color.darker(120), 0.5)); // Thin outline for definition

    m_isOutlined = true;
}

QColor StrokeItem::color() const { return m_color; }
qreal StrokeItem::width() const { return m_width; }
bool StrokeItem::isOutlined() const { return m_isOutlined; }

void StrokeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    // Draw the regular path first
    QGraphicsPathItem::paint(painter, option, widget);
}