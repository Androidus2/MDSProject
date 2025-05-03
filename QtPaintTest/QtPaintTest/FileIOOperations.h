#pragma once
#include <QtWidgets>
#include <QSvgGenerator>
#include "MainWindow.h"

class FileIOOperations {
private:
	static QString currentFilePath;
public:
	// File operations
	static void newDrawing(QGraphicsScene& scene, MainWindow& window);
	static void loadDrawing(QGraphicsScene& scene, MainWindow& window);
	static void saveDrawing(QGraphicsScene& scene, MainWindow& window);
	static void saveDrawingAs(QGraphicsScene& scene, MainWindow& window);
	static bool maybeSave(QGraphicsScene& scene, MainWindow& window);

	// Save and load file operations
	static bool saveFile(const QString& fileName, const QGraphicsScene& scene, MainWindow& window);
	static bool loadFile(const QString& fileName, QGraphicsScene& scene, MainWindow& window);
	// Export operations
	static void exportSVG(QGraphicsScene& scene, MainWindow& window);
	static void exportPNG(QGraphicsScene& scene, MainWindow& window);
	static void exportJPEG(QGraphicsScene& scene, MainWindow& window);
};