#include "DrawingManager.h"
#include "RemoveCommand.h"
#include "AddCommand.h"

void log(const QString& message) {
	// Open the file and append the message
	std::ofstream logFile("../log.txt", std::ios::app);
	logFile << message.toStdString() << std::endl;
}

DrawingManager::DrawingManager() : m_scene(nullptr), m_width(15) {
	//log("DrawingManager initialized");
	// Initialize default color
	m_color = Qt::black;

	// Initialize tools
	m_tools.push_back(new BrushTool());
	m_tools.push_back(new EraserTool());
	m_tools.push_back(new FillTool());
	m_tools.push_back(new SelectTool());

	// Set the default tool to Brush
	m_currentTool = m_tools[0];
}

void DrawingManager::pushCommand(QUndoCommand* command) {
    if (m_undoStack) {
        m_undoStack->push(command);
    }
    else {
        command->redo();
        delete command;
    }
}

void DrawingManager::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;

    if (m_undoStack) {
		SelectTool* selectTool = dynamic_cast<SelectTool*>(m_tools[3]);
        if (selectTool) {
            connect(m_undoStack, &QUndoStack::indexChanged, selectTool, &SelectTool::updateSelectionUI);
        }
    }
}

// Clipboard Operations
void DrawingManager::copySelection() {
	if (m_currentTool->toolName() != "Select") return;
    m_clipboard.clear();

	QList<BaseItem*> selectedItems = static_cast<SelectTool*>(m_currentTool)->getSelectedItems();

    for (BaseItem* baseItem : selectedItems) {
		StrokeItem* item = static_cast<StrokeItem*>(baseItem);
        if (!item->isOutlined()) {
            item->convertToFilledPath();
        }
        m_clipboard.append({ item->path(), item->color(), item->width(), item->isOutlined() });
    }
}

void DrawingManager::cutSelection() {
	if (m_currentTool->toolName() != "Select") return;

    copySelection();

    // We need to create a copy of the selected items since RemoveCommand will modify the scene
	QList<BaseItem*> itemsToRemove = static_cast<SelectTool*>(m_currentTool)->getSelectedItems();

    for (BaseItem* item : itemsToRemove) {
        RemoveCommand* cmd = new RemoveCommand(m_scene, item);
        pushCommand(cmd);
    }

	// Clear the selection
	static_cast<SelectTool*>(m_currentTool)->clearSelection();
}

void DrawingManager::pasteClipboard() {
    
    if (m_clipboard.isEmpty()) return;

	if (m_currentTool->toolName() != "Select")
		m_currentTool = m_tools[3]; // Switch to SelectTool for pasting
    static_cast<SelectTool*>(m_currentTool)->clearSelection();

    // Calculate the bounding rect of all clipboard items
    QRectF clipboardBounds;
    for (const auto& ci : m_clipboard) {
        if (clipboardBounds.isNull()) {
            clipboardBounds = ci.path.boundingRect();
        }
        else {
            clipboardBounds = clipboardBounds.united(ci.path.boundingRect());
        }
    }

    // Calculate the offset to apply to all items 
    QPointF clipboardCenter = clipboardBounds.center();
    QPointF offsetToApply = m_lastSceneMousePos - clipboardCenter;

    QList<BaseItem*> pastedItems;

    for (const auto& ci : m_clipboard) {
        StrokeItem* item = new StrokeItem(ci.color, ci.width);
        item->setOutlined(ci.outlined);

        QPainterPath movedPath = ci.path;
        movedPath.translate(offsetToApply);
        item->setPath(movedPath);

        if (ci.outlined) {
            item->setBrush(QBrush(ci.color));
            item->setPen(QPen(ci.color.darker(120), 0.5));
        }
        else {
            QPen pen(ci.color, ci.width);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            item->setPen(pen);
            item->setBrush(Qt::NoBrush);
        }

        AddCommand* cmd = new AddCommand(m_scene, item);
        pushCommand(cmd);
        pastedItems.append(item);
    }

	static_cast<SelectTool*>(m_currentTool)->setSelectedItems(pastedItems);
}

void DrawingManager::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	if (m_scene) {
		m_currentTool->mousePressEvent(event);
	}
}
void DrawingManager::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
	m_lastSceneMousePos = event->scenePos(); // Store the last known mouse position

	if (m_scene) {
		m_currentTool->mouseMoveEvent(event);
	}
}
void DrawingManager::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
	if (m_scene) {
		m_currentTool->mouseReleaseEvent(event);
	}
}

void DrawingManager::keyReleaseEvent(QKeyEvent* event) {
	if (m_scene) {
		m_currentTool->keyReleaseEvent(event);
	}
}
void DrawingManager::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_C) {
            if (m_currentTool->toolName() == "Select") {
                copySelection();
                event->accept();
                return;
            }
        }
        if (event->key() == Qt::Key_X) {
			if (m_currentTool->toolName() == "Select") {
				cutSelection();
				event->accept();
				return;
			}
        }
        if (event->key() == Qt::Key_V) {
            pasteClipboard();
            event->accept();
            return;
        }
        // Add Ctrl+Z and Ctrl+Y for undo/redo
        if (event->key() == Qt::Key_Z && m_undoStack) {
            m_undoStack->undo();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Y && m_undoStack) {
            m_undoStack->redo();
            event->accept();
            return;
        }
    }

	if (m_scene) {
		m_currentTool->keyPressEvent(event);
	}
}