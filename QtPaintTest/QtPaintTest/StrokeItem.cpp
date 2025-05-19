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

StrokeItem::StrokeItem(const QColor& fillColor)
    : m_color(fillColor), m_width(0), m_isOutlined(true)  // Note: isOutlined=true for filled shapes
{
    // Set up a proper filled shape
    setBrush(QBrush(fillColor));

    // Thin outline for better definition
    QPen outlinePen(fillColor.darker(120), 0.5);
    outlinePen.setJoinStyle(Qt::RoundJoin);
    setPen(outlinePen);
}

StrokeItem::StrokeItem(const StrokeItem& other)
	: m_color(other.m_color), m_width(other.m_width),
	m_isOutlined(other.m_isOutlined),
	m_originalPen(other.m_originalPen)
{
	setPen(m_originalPen);
	setBrush(brush());

	// Copy the path
	setPath(other.path());

	// Copy the transform
	setTransform(other.transform());
	setPos(other.pos());
	setRotation(other.rotation());
	setScale(other.scale());
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

StrokeItem* StrokeItem::clone() const {
	StrokeItem* clone = new StrokeItem(m_color, m_width);
	clone->setPath(path());
	clone->setPen(pen());
	clone->setBrush(brush());
	clone->setOutlined(m_isOutlined);

	// Copy selection state
	clone->m_isSelected = m_isSelected;
	clone->m_originalPen = m_originalPen;

	// Copy transform
	clone->setTransform(transform());
	clone->setPos(pos());
	clone->setRotation(rotation());
	clone->setScale(scale());

	return clone;
}

void StrokeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    // Draw the regular path first
    QGraphicsPathItem::paint(painter, option, widget);
}

void StrokeItem::setSelected(bool selected) {
    if (selected == m_isSelected) return;

    m_isSelected = selected;

    if (selected) {
        // Store original pen
        m_originalPen = pen();

        // Create highlight pen
        QPen highlightPen = m_originalPen;
        highlightPen.setColor(Qt::blue);
        highlightPen.setWidth(m_originalPen.width() + 1);
        highlightPen.setStyle(Qt::DashLine);

        setPen(highlightPen);
    }
    else {
        // Restore original pen
        setPen(m_originalPen);
    }

    update();
}