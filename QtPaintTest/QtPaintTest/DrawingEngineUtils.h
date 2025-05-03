#pragma once
#include <QColor>
#include <QtWidgets>
#include <clipper2/clipper.h>

// Color constants
const QColor DEFAULT_BRUSH_COLOR = Qt::black;
const QColor DEFAULT_FILL_COLOR = Qt::yellow;
const QColor DEFAULT_ERASER_COLOR = Qt::red;

// Brush size constants
const int MIN_BRUSH_SIZE = 1;
const int MAX_BRUSH_SIZE = 100;
const int DEFAULT_BRUSH_SIZE = 15;

// Other constants
const int JPEG_QUALITY_DEFAULT = 90;
const double CLIPPER_SCALING = 1000.0;

enum ToolType { Brush, Eraser, Fill,Select };

class DrawingEngineUtils {
public:
	static Clipper2Lib::Path64 convertPathToClipper(const QPainterPath& path);
	static QPainterPath convertSingleClipperPath(const Clipper2Lib::Path64& path);
};