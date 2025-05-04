#include "FileIOOperations.h"

QString FileIOOperations::currentFilePath = "";

void FileIOOperations::newDrawing(QGraphicsScene& scene, MainWindow& window) {
    if (maybeSave(scene, window)) {
        scene.clear();
        currentFilePath = "";
        window.setWindowTitle("Qt Vector Drawing - Untitled");
    }
}
void FileIOOperations::loadDrawing(QGraphicsScene& scene, MainWindow& window) {
    if (maybeSave(scene, window)) {
        QString fileName = QFileDialog::getOpenFileName(&window,
            "Open Drawing", "", "Qt Vector Drawing (*.qvd)");

        if (!fileName.isEmpty()) {
            loadFile(fileName, scene, window);
        }
    }
}
void FileIOOperations::saveDrawing(QGraphicsScene& scene, MainWindow& window) {
    if (currentFilePath.isEmpty()) {
        saveDrawingAs(scene, window);
    }
    else {
        saveFile(currentFilePath, scene, window);
    }
}
void FileIOOperations::saveDrawingAs(QGraphicsScene& scene, MainWindow& window) {
    QString fileName = QFileDialog::getSaveFileName(&window,
        "Save Drawing", "", "Qt Vector Drawing (*.qvd)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".qvd", Qt::CaseInsensitive)) {
            fileName += ".qvd";
        }
        saveFile(fileName, scene, window);
    }
}
bool FileIOOperations::maybeSave(QGraphicsScene& scene, MainWindow& window) {
    QMessageBox::StandardButton response = QMessageBox::question(
        &window, "Save Changes", "Do you want to save your changes?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    if (response == QMessageBox::Save) {
        return saveDrawing(scene, window), true;
    }
    else if (response == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

bool FileIOOperations::saveFile(const QString& fileName, const QGraphicsScene& scene, MainWindow& window) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(&window, "Save Error",
            "Unable to open file for writing: " + file.errorString());
        return false;
    }

    // Create JSON document to store drawing
    QJsonObject root;
    QJsonArray items;

    // Store each drawing item
    for (QGraphicsItem* item : scene.items()) {
        if (StrokeItem* stroke = dynamic_cast<StrokeItem*>(item)) {
            QJsonObject itemObj;

            // Store type
            itemObj["type"] = stroke->isOutlined() ? "filled" : "stroke";

            // Store color
            QColor color = stroke->color();
            itemObj["color"] = color.name();
            itemObj["alpha"] = color.alpha();

            // Store width
            itemObj["width"] = stroke->width();

            // Store path data
            QJsonArray pathData;
            QPainterPath path = stroke->path();
            for (int i = 0; i < path.elementCount(); ++i) {
                const QPainterPath::Element& el = path.elementAt(i);
                QJsonObject point;
                point["x"] = el.x;
                point["y"] = el.y;
                point["type"] = static_cast<int>(el.type);
                pathData.append(point);
            }
            itemObj["path"] = pathData;

            // Add to items array
            items.append(itemObj);
        }
    }

    root["items"] = items;

    // Write JSON to file
    QJsonDocument doc(root);
    file.write(doc.toJson());

    currentFilePath = fileName;
    window.setWindowTitle("Qt Vector Drawing - " + QFileInfo(fileName).fileName());
    window.statusBar()->showMessage("Drawing saved", 2000);
    return true;
}
bool FileIOOperations::loadFile(const QString& fileName, QGraphicsScene& scene, MainWindow& window) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(&window, "Load Error",
            "Unable to open file: " + file.errorString());
        return false;
    }

    // Read JSON data
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        QMessageBox::warning(&window, "Load Error", "Invalid file format");
        return false;
    }

    // Clear current scene
    scene.clear();

    // Parse JSON and recreate items
    QJsonObject root = doc.object();
    QJsonArray items = root["items"].toArray();

    for (const QJsonValue& itemValue : items) {
        QJsonObject itemObj = itemValue.toObject();

        // Get type
        QString type = itemObj["type"].toString();

        // Get color
        QColor color(itemObj["color"].toString());
        color.setAlpha(itemObj["alpha"].toInt(255));

        // Get width
        qreal width = itemObj["width"].toDouble();

        // Create appropriate item
        StrokeItem* item;
        if (type == "filled")
            width = 0;
        item = new StrokeItem(color, width);

        // Reconstruct path
        QPainterPath path;
        QJsonArray pathData = itemObj["path"].toArray();
        bool firstPoint = true;

        for (int i = 0; i < pathData.size(); ++i) {
            QJsonObject point = pathData[i].toObject();
            qreal x = point["x"].toDouble();
            qreal y = point["y"].toDouble();
            int elementType = point["type"].toInt();

            switch (elementType) {
            case QPainterPath::MoveToElement:
                path.moveTo(x, y);
                firstPoint = false;
                break;
            case QPainterPath::LineToElement:
                if (firstPoint) {
                    path.moveTo(x, y);
                    firstPoint = false;
                }
                else {
                    path.lineTo(x, y);
                }
                break;
            case QPainterPath::CurveToElement:
                if (i + 2 < pathData.size()) {
                    QJsonObject c1 = pathData[i].toObject();
                    QJsonObject c2 = pathData[i + 1].toObject();
                    QJsonObject endPoint = pathData[i + 2].toObject();

                    path.cubicTo(
                        c1["x"].toDouble(), c1["y"].toDouble(),
                        c2["x"].toDouble(), c2["y"].toDouble(),
                        endPoint["x"].toDouble(), endPoint["y"].toDouble()
                    );

                    i += 2; // Skip the next two points as we've used them
                }
                break;
            }
        }

        item->setPath(path);
        if (type == "filled") {
            item->setOutlined(true);
        }

        scene.addItem(item);
    }

    currentFilePath = fileName;
    window.setWindowTitle("Qt Vector Drawing - " + QFileInfo(fileName).fileName());
    window.statusBar()->showMessage("Drawing loaded", 2000);
    return true;
}

void FileIOOperations::exportSVG(QGraphicsScene& scene, MainWindow& window) {
    QString fileName = QFileDialog::getSaveFileName(&window,
        "Export SVG", "", "SVG Files (*.svg)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".svg", Qt::CaseInsensitive)) {
            fileName += ".svg";
        }

        QSvgGenerator generator;
        generator.setFileName(fileName);
        generator.setSize(QSize(scene.width(), scene.height()));
        generator.setViewBox(QRect(0, 0, scene.width(), scene.height()));
        generator.setTitle("Qt Vector Drawing");
        generator.setDescription("Created with Qt Vector Drawing App");

        QPainter painter;
        painter.begin(&generator);
        painter.setRenderHint(QPainter::Antialiasing);
        scene.render(&painter);
        painter.end();

        window.statusBar()->showMessage("Exported to SVG", 2000);
    }
}
void FileIOOperations::exportPNG(QGraphicsScene& scene, MainWindow& window) {
    QString fileName = QFileDialog::getSaveFileName(&window,
        "Export PNG", "", "PNG Files (*.png)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".png", Qt::CaseInsensitive)) {
            fileName += ".png";
        }

        QRectF sceneRect = scene.sceneRect();

        // Create a dialog for resolution input
        QDialog resDialog(&window);
        resDialog.setWindowTitle("Set Export Resolution");
        resDialog.setModal(true);

        QVBoxLayout* layout = new QVBoxLayout(&resDialog);

        // Add width input
        QHBoxLayout* widthLayout = new QHBoxLayout();
        QLabel* widthLabel = new QLabel("Width:", &resDialog);
        QSpinBox* widthInput = new QSpinBox(&resDialog);
        widthInput->setRange(1, 10000);
        widthInput->setValue(sceneRect.width());
        widthLayout->addWidget(widthLabel);
        widthLayout->addWidget(widthInput);

        // Add height input
        QHBoxLayout* heightLayout = new QHBoxLayout();
        QLabel* heightLabel = new QLabel("Height:", &resDialog);
        QSpinBox* heightInput = new QSpinBox(&resDialog);
        heightInput->setRange(1, 10000);
        heightInput->setValue(sceneRect.height());
        heightLayout->addWidget(heightLabel);
        heightLayout->addWidget(heightInput);

        // Add aspect ratio checkbox
        QCheckBox* keepAspectRatio = new QCheckBox("Keep aspect ratio", &resDialog);
        keepAspectRatio->setChecked(true);

        // Connect signals to maintain aspect ratio if checked
        double aspectRatio = static_cast<double>(sceneRect.width()) / sceneRect.height();
        QObject::connect(widthInput, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value) {
            if (keepAspectRatio->isChecked()) {
                heightInput->blockSignals(true);
                heightInput->setValue(qRound(value / aspectRatio));
                heightInput->blockSignals(false);
            }
            });

        QObject::connect(heightInput, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value) {
            if (keepAspectRatio->isChecked()) {
                widthInput->blockSignals(true);
                widthInput->setValue(qRound(value * aspectRatio));
                widthInput->blockSignals(false);
            }
            });

        // Add buttons
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &resDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &resDialog, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, &resDialog, &QDialog::reject);

        // Add all widgets to dialog
        layout->addLayout(widthLayout);
        layout->addLayout(heightLayout);
        layout->addWidget(keepAspectRatio);
        layout->addWidget(buttonBox);

        // Show dialog and proceed if accepted
        if (resDialog.exec() == QDialog::Accepted) {
            int width = widthInput->value();
            int height = heightInput->value();

            QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);

            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);

            // Use the overloaded render method that maps source rectangle to target rectangle
            QRectF targetRect(0, 0, width, height);
            scene.render(&painter, targetRect, sceneRect, Qt::IgnoreAspectRatio);
            painter.end();

            image.save(fileName);
            window.statusBar()->showMessage("Exported to PNG", 2000);
        }
    }
}

void FileIOOperations::exportJPEG(QGraphicsScene& scene, MainWindow& window) {
    QString fileName = QFileDialog::getSaveFileName(&window,
        "Export JPEG", "", "JPEG Files (*.jpg)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".jpg", Qt::CaseInsensitive) &&
            !fileName.endsWith(".jpeg", Qt::CaseInsensitive)) {
            fileName += ".jpg";
        }

        QRectF sceneRect = scene.sceneRect();

        // Create a dialog for resolution input
        QDialog resDialog(&window);
        resDialog.setWindowTitle("Set Export Resolution");
        resDialog.setModal(true);

        QVBoxLayout* layout = new QVBoxLayout(&resDialog);

        // Add width input
        QHBoxLayout* widthLayout = new QHBoxLayout();
        QLabel* widthLabel = new QLabel("Width:", &resDialog);
        QSpinBox* widthInput = new QSpinBox(&resDialog);
        widthInput->setRange(1, 10000);
        widthInput->setValue(sceneRect.width());
        widthLayout->addWidget(widthLabel);
        widthLayout->addWidget(widthInput);

        // Add height input
        QHBoxLayout* heightLayout = new QHBoxLayout();
        QLabel* heightLabel = new QLabel("Height:", &resDialog);
        QSpinBox* heightInput = new QSpinBox(&resDialog);
        heightInput->setRange(1, 10000);
        heightInput->setValue(sceneRect.height());
        heightLayout->addWidget(heightLabel);
        heightLayout->addWidget(heightInput);

        // Add aspect ratio checkbox
        QCheckBox* keepAspectRatio = new QCheckBox("Keep aspect ratio", &resDialog);
        keepAspectRatio->setChecked(true);

        // Connect signals to maintain aspect ratio if checked
        double aspectRatio = static_cast<double>(sceneRect.width()) / sceneRect.height();
        QObject::connect(widthInput, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value) {
            if (keepAspectRatio->isChecked()) {
                heightInput->blockSignals(true);
                heightInput->setValue(qRound(value / aspectRatio));
                heightInput->blockSignals(false);
            }
            });

        QObject::connect(heightInput, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value) {
            if (keepAspectRatio->isChecked()) {
                widthInput->blockSignals(true);
                widthInput->setValue(qRound(value * aspectRatio));
                widthInput->blockSignals(false);
            }
            });

        // Add buttons
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &resDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &resDialog, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, &resDialog, &QDialog::reject);

        // Add all widgets to dialog
        layout->addLayout(widthLayout);
        layout->addLayout(heightLayout);
        layout->addWidget(keepAspectRatio);
        layout->addWidget(buttonBox);

        // Show dialog and proceed if accepted
        if (resDialog.exec() == QDialog::Accepted) {
            int width = widthInput->value();
            int height = heightInput->value();

            // Show quality dialog
            bool ok;
            int quality = QInputDialog::getInt(&window, "JPEG Quality",
                "Select quality (0-100):", 90, 0, 100, 1, &ok);

            if (ok) {
                QImage image(width, height, QImage::Format_RGB32);
                image.fill(Qt::white); // JPEG doesn't support transparency

                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);

                // Use the overloaded render method that maps source rectangle to target rectangle
                QRectF targetRect(0, 0, width, height);
                scene.render(&painter, targetRect, sceneRect, Qt::IgnoreAspectRatio);
                painter.end();

                image.save(fileName, "JPEG", quality);
                window.statusBar()->showMessage("Exported to JPEG", 2000);
            }
        }
    }
}