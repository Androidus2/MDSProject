#pragma once
#include <vector>
#include <QtWidgets>
#include "DrawingEngineUtils.h"
#include "DrawingScene.h"
#include "ClipboardItem.h"

#include <fstream>

void log(const QString& message);

class DrawingManager : QObject {
	Q_OBJECT
private:
	DrawingManager();
	DrawingManager(const DrawingManager&) = delete;
	DrawingManager& operator=(const DrawingManager&) = delete;
public:
	static DrawingManager& getInstance() {
		static DrawingManager instance;
		return instance;
	}

	void setScene(DrawingScene* scene) {
		// Reset selection state if the current tool is SelectTool
		if (m_currentTool && m_currentTool->toolName() == "Select") {
			SelectTool* selectTool = dynamic_cast<SelectTool*>(m_currentTool);
			if (selectTool) {
				selectTool->resetSelectionState();
			}
		}
		m_scene = scene;
	}
	DrawingScene* getScene() const {
		return m_scene;
	}

	void setCurrentTool(QString toolName) {
		// Reset selection state if the current tool is SelectTool
		if (m_currentTool && m_currentTool->toolName() == "Select") {
			SelectTool* selectTool = dynamic_cast<SelectTool*>(m_currentTool);
			if (selectTool) {
				selectTool->resetSelectionState();
			}
		}

		for (BaseTool* tool : m_tools) {
			if (tool->toolName() == toolName) {
				m_currentTool = tool;
				break;
			}
		}
	}
	BaseTool* getCurrentTool() const {
		return m_currentTool;
	}

	BaseTool* getToolByName(const QString& toolName) {
		for (BaseTool* tool : m_tools) {
			if (tool->toolName() == toolName) {
				return tool;
			}
		}
		return nullptr;
	}

	void setWidth(qreal width) {
		m_width = width;
	}
	qreal getWidth() const {
		return m_width;
	}

	void setColor(const QColor& color) {
		m_color = color;
	}
	QColor getColor() const {
		return m_color;
	}

	void copySelection();
	void cutSelection();
	void pasteClipboard();

	void mousePressEvent(QGraphicsSceneMouseEvent* event);
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
	void keyPressEvent(QKeyEvent* event);

	// Command helper function
	void pushCommand(QUndoCommand* command);

	// Undo/Redo functionality
	void setUndoStack(QUndoStack* stack);
private:

	DrawingScene* m_scene = nullptr;
	BaseTool* m_currentTool = nullptr;

	std::vector<BaseTool*> m_tools;

	QColor m_color;
	qreal m_width;

	QList<ClipboardItem> m_clipboard;
	QPointF m_lastSceneMousePos; // Store last known mouse position for paste operation

	QUndoStack* m_undoStack = nullptr;
};
	
