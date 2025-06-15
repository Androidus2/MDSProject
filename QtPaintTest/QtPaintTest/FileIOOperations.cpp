#include "FileIOOperations.h"
#include "DrawingScene.h"
#include "DrawingManager.h"

QString FileIOOperations::currentFilePath = "";

bool isFFmpegAvailable() {
    QProcess which;
    which.start("ffmpeg", QStringList() << "-version");
    return which.waitForStarted(1000);
}

void FileIOOperations::newDrawing(QGraphicsScene& scene, MainWindow& window) {
    if (maybeSave(scene, window)) {
        // Reset selection state first
        if (auto* drawingScene = dynamic_cast<DrawingScene*>(&scene)) {
            if (DrawingManager::getInstance().getCurrentTool()->toolName() == "Select") {
                SelectTool* selectTool = dynamic_cast<SelectTool*>(DrawingManager::getInstance().getCurrentTool());
                if (selectTool) {
                    selectTool->resetSelectionState();
                }
            }
        }

        scene.clear();
        currentFilePath = "";
        window.setWindowTitle("Vecmate - Untitled");
    }
}
void FileIOOperations::loadDrawing(QGraphicsScene& scene, MainWindow& window) {
    if (maybeSave(scene, window)) {
        QString fileName = QFileDialog::getOpenFileName(&window,
            "Open Drawing", "", "Vecmate (*.qvd)");

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
        "Save Drawing", "", "Vecmate (*.qvd)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".qvd", Qt::CaseInsensitive)) {
            fileName += ".qvd";
        }
        saveFile(fileName, scene, window);
    }
}
bool FileIOOperations::maybeSave(QGraphicsScene& scene, MainWindow& window) {
    if (!DrawingManager::getInstance().hasModifications()) {
        return true;
    }

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

            // Store position
            itemObj["posX"] = stroke->pos().x();
            itemObj["posY"] = stroke->pos().y();

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
        else if (RasterItem* raster = dynamic_cast<RasterItem*>(item)) {
            QJsonObject itemObj;

            // Store type
            itemObj["type"] = "raster";

            // Store position and transform
            itemObj["posX"] = raster->pos().x();
            itemObj["posY"] = raster->pos().y();
            itemObj["rotation"] = raster->rotation();
            itemObj["scaleX"] = raster->scale();
            itemObj["scaleY"] = raster->scale();

            // Store image data (convert to Base64)
            QBuffer buffer;
            buffer.open(QIODevice::WriteOnly);
            raster->getImage().save(&buffer, "PNG");
            itemObj["imageData"] = QString(buffer.data().toBase64());

            // Add to items array
            items.append(itemObj);
        }
    }

    root["items"] = items;

    // Write JSON to file
    QJsonDocument doc(root);
    file.write(doc.toJson());

    currentFilePath = fileName;
    window.setWindowTitle("Vecmate - " + QFileInfo(fileName).fileName());
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

    // Reset selection state first - this prevents crashes with the selection tool
    if (auto* drawingScene = dynamic_cast<DrawingScene*>(&scene)) {
        if (DrawingManager::getInstance().getCurrentTool()->toolName() == "Select") {
            SelectTool* selectTool = dynamic_cast<SelectTool*>(DrawingManager::getInstance().getCurrentTool());
            if (selectTool) {
                selectTool->resetSelectionState();
            }
        }
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

        if (type == "filled" || type == "stroke") {
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

            // Set position if available
            if (itemObj.contains("posX") && itemObj.contains("posY")) {
                item->setPos(itemObj["posX"].toDouble(), itemObj["posY"].toDouble());
            }

            scene.addItem(item);
        }
        else if (type == "raster") {
            // Get the Base64 image data and convert it back to QImage
            QString imageData = itemObj["imageData"].toString();
            QByteArray byteArray = QByteArray::fromBase64(imageData.toLatin1());
            QImage image;
            image.loadFromData(byteArray);

            RasterItem* raster = new RasterItem(image);

            // Set position and transform
            if (itemObj.contains("posX") && itemObj.contains("posY")) {
                raster->setPos(itemObj["posX"].toDouble(), itemObj["posY"].toDouble());
            }

            if (itemObj.contains("rotation")) {
                raster->setRotation(itemObj["rotation"].toDouble());
            }

            if (itemObj.contains("scaleX") && itemObj.contains("scaleY")) {
                qreal scaleX = itemObj["scaleX"].toDouble();
                qreal scaleY = itemObj["scaleY"].toDouble();
                raster->setScale(scaleX); // Assuming uniform scaling in this implementation
            }

            scene.addItem(raster);
        }
    }

    currentFilePath = fileName;
    window.setWindowTitle("Vecmate - " + QFileInfo(fileName).fileName());
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

        // Check if there are any RasterItems in the scene
        bool hasRasterItems = false;
        for (QGraphicsItem* item : scene.items()) {
            if (dynamic_cast<RasterItem*>(item)) {
                hasRasterItems = true;
                break;
            }
        }

        if (hasRasterItems) {
            QMessageBox::information(&window, "SVG Export",
                "The scene contains raster images which will be embedded in the SVG. "
                "This may increase file size significantly.");
        }

        QSvgGenerator generator;
        generator.setFileName(fileName);
        generator.setSize(QSize(scene.width(), scene.height()));
        generator.setViewBox(QRect(0, 0, scene.width(), scene.height()));
        generator.setTitle("Vecmate Drawing");
        generator.setDescription("Created with Vecmate");

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
void FileIOOperations::exportGIF(MainWindow& window) {
    QString fileName = QFileDialog::getSaveFileName(&window,
        "Export Animated GIF", "", "GIF Files (*.gif)");

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(".gif", Qt::CaseInsensitive)) {
        fileName += ".gif";
    }

    // Get frames from the MainWindow
    QList<DrawingScene*> frames = window.getFrames();

    if (frames.isEmpty()) {
        QMessageBox::warning(&window, "Export Error", "No frames available for animation export");
        return;
    }

    // Create a dialog for settings
    QDialog settingsDialog(&window);
    settingsDialog.setWindowTitle("Animated GIF Settings");
    settingsDialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&settingsDialog);

    // Frame rate setting
    QHBoxLayout* fpsLayout = new QHBoxLayout();
    QLabel* fpsLabel = new QLabel("Frame Rate (FPS):", &settingsDialog);
    QSpinBox* fpsInput = new QSpinBox(&settingsDialog);
    fpsInput->setRange(1, 60);
    fpsInput->setValue(24); // Default 24 FPS
    fpsLayout->addWidget(fpsLabel);
    fpsLayout->addWidget(fpsInput);

    // Resolution settings
    QHBoxLayout* resLayout = new QHBoxLayout();
    QLabel* resLabel = new QLabel("Resolution:", &settingsDialog);

    // Get the first frame's dimensions as default
    QRectF sceneRect = frames.first()->sceneRect();

    QSpinBox* widthInput = new QSpinBox(&settingsDialog);
    widthInput->setRange(1, 10000);
    widthInput->setValue(sceneRect.width());

    QSpinBox* heightInput = new QSpinBox(&settingsDialog);
    heightInput->setRange(1, 10000);
    heightInput->setValue(sceneRect.height());

    resLayout->addWidget(resLabel);
    resLayout->addWidget(widthInput);
    resLayout->addWidget(new QLabel("x", &settingsDialog));
    resLayout->addWidget(heightInput);

    // Frame range
    QHBoxLayout* rangeLayout = new QHBoxLayout();
    QLabel* rangeLabel = new QLabel("Frame Range:", &settingsDialog);
    QSpinBox* startFrameInput = new QSpinBox(&settingsDialog);
    startFrameInput->setRange(1, frames.size());
    startFrameInput->setValue(1);

    QSpinBox* endFrameInput = new QSpinBox(&settingsDialog);
    endFrameInput->setRange(1, frames.size());
    endFrameInput->setValue(frames.size());

    rangeLayout->addWidget(rangeLabel);
    rangeLayout->addWidget(startFrameInput);
    rangeLayout->addWidget(new QLabel("to", &settingsDialog));
    rangeLayout->addWidget(endFrameInput);

    // Loop settings
    QCheckBox* loopCheckbox = new QCheckBox("Loop animation", &settingsDialog);
    loopCheckbox->setChecked(true);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &settingsDialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &settingsDialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &settingsDialog, &QDialog::reject);

    // Add all widgets to dialog
    layout->addLayout(fpsLayout);
    layout->addLayout(resLayout);
    layout->addLayout(rangeLayout);
    layout->addWidget(loopCheckbox);
    layout->addWidget(buttonBox);

    // Show dialog and proceed if accepted
    if (settingsDialog.exec() == QDialog::Accepted) {
        int fps = fpsInput->value();
        int width = widthInput->value();
        int height = heightInput->value();
        int startFrame = startFrameInput->value() - 1; // Convert to 0-based index
        int endFrame = endFrameInput->value() - 1;     // Convert to 0-based index
        bool loop = loopCheckbox->isChecked();

        // Create a temporary directory for the frames
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            QMessageBox::warning(&window, "Export Error", "Failed to create temporary directory");
            return;
        }

        // Get the temporary directory path
        QString tempDirPath = QDir::toNativeSeparators(tempDir.path());

        // Create progress dialog
        QProgressDialog progress("Creating animated GIF...", "Cancel", 0, (endFrame - startFrame + 1) + 1);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        // Step 1: Render each frame to a PNG file
        for (int i = startFrame; i <= endFrame; i++) {
            progress.setValue(i - startFrame);
            QApplication::processEvents();

            if (progress.wasCanceled()) {
                return;
            }

            // Create the frame image
            QImage frameImage(width, height, QImage::Format_ARGB32);
            frameImage.fill(Qt::transparent);

            QPainter painter(&frameImage);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            frames[i]->render(&painter, QRectF(0, 0, width, height), frames[i]->sceneRect());
            painter.end();

            // Save the frame as a PNG file
            QString framePath = QString("%1/frame_%2.png").arg(tempDirPath).arg(i - startFrame, 4, 10, QChar('0'));
            frameImage.save(framePath, "PNG");
        }

        if (!isFFmpegAvailable()) {
            QMessageBox::StandardButton result = QMessageBox::question(&window,
                "FFmpeg Not Found",
                "FFmpeg is required for animated GIF export but was not found on your system. "
                "Would you like to export individual frames instead?",
                QMessageBox::Yes | QMessageBox::No);

            if (result == QMessageBox::Yes) {
                // Export individual frames
                for (int i = startFrame; i <= endFrame; i++) {
                    // Export frame code...
                }

                // Open the directory
                QDesktopServices::openUrl(QUrl::fromLocalFile(tempDirPath));

                QMessageBox::information(&window, "Frames Exported",
                    "Individual frames have been exported. To create an animated GIF:\n\n"
                    "1. Install FFmpeg from https://ffmpeg.org/download.html\n"
                    "2. Run this command in the terminal:\n\n"
                    "ffmpeg -framerate " + QString::number(fps) + " -i " + tempDirPath + "/frame_%04d.png " +
                    "-vf \"scale=" + QString::number(width) + ":" + QString::number(height) +
                    ":flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" " +
                    "-loop " + (loop ? "0" : "-1") + " " + fileName);

                return;
            }
            else {
                return;
            }
        }

        // Step 2: Use FFmpeg to create the animated GIF
        QProcess ffmpeg;

        // Build the FFmpeg command
        QStringList arguments;

        // Input framerate
        arguments << "-framerate" << QString::number(fps);

        // Input pattern (use frame_%04d.png to match our naming scheme)
        arguments << "-i" << QString("%1/frame_%2.png").arg(tempDirPath).arg("%04d");

        // GIF settings for good quality
        // Use palettegen filter to create an optimal palette
        arguments << "-vf"
            << QString("scale=%1:%2:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse").arg(width).arg(height);

        // Set loop - 0 means infinite loop, -1 means no loop
        if (loop) {
            arguments << "-loop" << "0";
        }
        else {
            arguments << "-loop" << "-1";
        }

        // Output file (overwrite if exists)
        arguments << "-y" << QDir::toNativeSeparators(fileName);

        // Start the FFmpeg process
        ffmpeg.start("ffmpeg", arguments);

        // Wait for it to start
        if (!ffmpeg.waitForStarted(-1)) {
            QMessageBox::warning(&window, "Export Error",
                "Failed to start FFmpeg. Make sure FFmpeg is installed and in your PATH.");
            return;
        }

        // Update progress dialog for FFmpeg processing
        progress.setLabelText("Processing with FFmpeg...");
        progress.setValue(endFrame - startFrame + 1);

        // Wait for FFmpeg to finish
        if (!ffmpeg.waitForFinished(-1)) {
            QMessageBox::warning(&window, "Export Error", "FFmpeg process failed.");
            return;
        }

        // Check for errors
        if (ffmpeg.exitCode() != 0) {
            QString errorOutput = QString::fromLocal8Bit(ffmpeg.readAllStandardError());
            QMessageBox::warning(&window, "FFmpeg Error",
                "FFmpeg exited with an error:\n" + errorOutput);
            return;
        }

        // Success
        window.statusBar()->showMessage("Exported to animated GIF", 2000);
    }
}
void FileIOOperations::exportMP4(MainWindow& window) {
    QString fileName = QFileDialog::getSaveFileName(&window,
        "Export MP4 Video", "", "MP4 Files (*.mp4)");

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(".mp4", Qt::CaseInsensitive)) {
        fileName += ".mp4";
    }

    // Get frames from the MainWindow
    QList<DrawingScene*> frames = window.getFrames();

    if (frames.isEmpty()) {
        QMessageBox::warning(&window, "Export Error", "No frames available for video export");
        return;
    }

    // Create a dialog for settings
    QDialog settingsDialog(&window);
    settingsDialog.setWindowTitle("MP4 Export Settings");
    settingsDialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&settingsDialog);

    // Frame rate setting
    QHBoxLayout* fpsLayout = new QHBoxLayout();
    QLabel* fpsLabel = new QLabel("Frame Rate (FPS):", &settingsDialog);
    QSpinBox* fpsInput = new QSpinBox(&settingsDialog);
    fpsInput->setRange(1, 60);
    fpsInput->setValue(24); // Default 24 FPS
    fpsLayout->addWidget(fpsLabel);
    fpsLayout->addWidget(fpsInput);

    // Resolution settings
    QHBoxLayout* resLayout = new QHBoxLayout();
    QLabel* resLabel = new QLabel("Resolution:", &settingsDialog);

    // Get the first frame's dimensions as default
    QRectF sceneRect = frames.first()->sceneRect();

    QSpinBox* widthInput = new QSpinBox(&settingsDialog);
    widthInput->setRange(1, 7680); // Up to 8K
    widthInput->setValue(sceneRect.width());

    QSpinBox* heightInput = new QSpinBox(&settingsDialog);
    heightInput->setRange(1, 4320); // Up to 8K
    heightInput->setValue(sceneRect.height());

    resLayout->addWidget(resLabel);
    resLayout->addWidget(widthInput);
    resLayout->addWidget(new QLabel("x", &settingsDialog));
    resLayout->addWidget(heightInput);

    // Quality setting
    QHBoxLayout* qualityLayout = new QHBoxLayout();
    QLabel* qualityLabel = new QLabel("Quality:", &settingsDialog);
    QComboBox* qualityCombo = new QComboBox(&settingsDialog);
    qualityCombo->addItem("Low (Fast Encoding)", "23");
    qualityCombo->addItem("Medium", "20");
    qualityCombo->addItem("High (Slower Encoding)", "18");
    qualityCombo->addItem("Very High (Slowest Encoding)", "15");
    qualityCombo->setCurrentIndex(1); // Medium quality default
    qualityLayout->addWidget(qualityLabel);
    qualityLayout->addWidget(qualityCombo);

    // Frame range
    QHBoxLayout* rangeLayout = new QHBoxLayout();
    QLabel* rangeLabel = new QLabel("Frame Range:", &settingsDialog);
    QSpinBox* startFrameInput = new QSpinBox(&settingsDialog);
    startFrameInput->setRange(1, frames.size());
    startFrameInput->setValue(1);

    QSpinBox* endFrameInput = new QSpinBox(&settingsDialog);
    endFrameInput->setRange(1, frames.size());
    endFrameInput->setValue(frames.size());

    rangeLayout->addWidget(rangeLabel);
    rangeLayout->addWidget(startFrameInput);
    rangeLayout->addWidget(new QLabel("to", &settingsDialog));
    rangeLayout->addWidget(endFrameInput);

    // Background color option
    QHBoxLayout* bgLayout = new QHBoxLayout();
    QLabel* bgLabel = new QLabel("Background:", &settingsDialog);
    QComboBox* bgCombo = new QComboBox(&settingsDialog);
    bgCombo->addItem("Transparent (with alpha)", "transparent");
    bgCombo->addItem("White", "white");
    bgCombo->addItem("Black", "black");
    bgCombo->addItem("Custom Color...", "custom");
    bgLayout->addWidget(bgLabel);
    bgLayout->addWidget(bgCombo);

    // Custom color widget (hidden initially)
    QColorDialog* colorDialog = new QColorDialog(&settingsDialog);
    colorDialog->setOption(QColorDialog::ShowAlphaChannel, true);
    colorDialog->setCurrentColor(Qt::white);
    QPushButton* colorButton = new QPushButton("Choose Color...", &settingsDialog);
    colorButton->setVisible(false);
    bgLayout->addWidget(colorButton);

    // Connect color dialog
    QObject::connect(bgCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
        colorButton->setVisible(bgCombo->currentData().toString() == "custom");
        });

    QObject::connect(colorButton, &QPushButton::clicked, [=]() {
        colorDialog->exec();
        });

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &settingsDialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &settingsDialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &settingsDialog, &QDialog::reject);

    // Add all widgets to dialog
    layout->addLayout(fpsLayout);
    layout->addLayout(resLayout);
    layout->addLayout(qualityLayout);
    layout->addLayout(rangeLayout);
    layout->addLayout(bgLayout);
    layout->addWidget(buttonBox);

    // Show dialog and proceed if accepted
    if (settingsDialog.exec() == QDialog::Accepted) {
        int fps = fpsInput->value();
        int width = widthInput->value();
        int height = heightInput->value();
        int startFrame = startFrameInput->value() - 1; // Convert to 0-based index
        int endFrame = endFrameInput->value() - 1;     // Convert to 0-based index
        QString quality = qualityCombo->currentData().toString();

        // Get background color
        QColor bgColor;
        QString bgType = bgCombo->currentData().toString();
        if (bgType == "transparent") {
            bgColor = Qt::transparent;
        }
        else if (bgType == "white") {
            bgColor = Qt::white;
        }
        else if (bgType == "black") {
            bgColor = Qt::black;
        }
        else if (bgType == "custom") {
            bgColor = colorDialog->currentColor();
        }

        // Create temporary directory for the frames
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            QMessageBox::warning(&window, "Export Error", "Failed to create temporary directory");
            return;
        }

        // Get the temporary directory path
        QString tempDirPath = QDir::toNativeSeparators(tempDir.path());

        // Create progress dialog
        QProgressDialog progress("Creating MP4 video...", "Cancel", 0, (endFrame - startFrame + 2), &window);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        // Step 1: Render each frame to a PNG file
        for (int i = startFrame; i <= endFrame; i++) {
            progress.setValue(i - startFrame);
            QApplication::processEvents();

            if (progress.wasCanceled()) {
                return;
            }

            // Create the frame image
            QImage frameImage(width, height, QImage::Format_ARGB32);
            frameImage.fill(bgColor);

            QPainter painter(&frameImage);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            frames[i]->render(&painter, QRectF(0, 0, width, height), frames[i]->sceneRect());
            painter.end();

            // Save the frame as a PNG file
            QString framePath = QString("%1/frame_%2.png").arg(tempDirPath).arg(i - startFrame, 4, 10, QChar('0'));
            frameImage.save(framePath, "PNG");
        }

        // Step 2: Find the FFmpeg executable
        QString ffmpegPath = "ffmpeg";

        // Step 3: Use FFmpeg to create the MP4
        QStringList arguments;

        // Input framerate
        arguments << "-framerate" << QString::number(fps);

        // Input pattern
        arguments << "-i" << QString("%1/frame_%2.png").arg(tempDirPath).arg("%04d");

        // Video codec settings
        arguments << "-c:v" << "libx264";
        arguments << "-crf" << quality;
        arguments << "-preset" << "medium";

        // Set pixel format (yuv420p is widely compatible)
        arguments << "-pix_fmt" << "yuv420p";

        // Additional video settings
        arguments << "-vf" << QString("scale=%1:%2").arg(width).arg(height);

        // Output file (overwrite if exists)
        arguments << "-y" << QDir::toNativeSeparators(fileName);

        // Show command in debug output
        qDebug() << "FFmpeg command:" << ffmpegPath << arguments.join(" ");

        // Update progress dialog
        progress.setLabelText("Processing with FFmpeg...");
        progress.setValue(endFrame - startFrame + 1);

        // Start the FFmpeg process
        QProcess ffmpeg;
        ffmpeg.start(ffmpegPath, arguments);

        // Wait for it to start
        if (!ffmpeg.waitForStarted(3000)) {
            QMessageBox::warning(&window, "Export Error",
                "Failed to start FFmpeg. Make sure FFmpeg is installed and in your PATH.");
            return;
        }

        // Wait for FFmpeg to finish
        if (!ffmpeg.waitForFinished(-1)) {
            QMessageBox::warning(&window, "Export Error", "FFmpeg process failed.");
            return;
        }

        // Check for errors
        if (ffmpeg.exitCode() != 0) {
            QString errorOutput = QString::fromLocal8Bit(ffmpeg.readAllStandardError());
            QMessageBox::warning(&window, "FFmpeg Error",
                "FFmpeg exited with an error:\n" + errorOutput);
            return;
        }

        // Complete progress
        progress.setValue(endFrame - startFrame + 2);

        // Success
        window.statusBar()->showMessage("Exported to MP4 video", 2000);

        // Ask if the user wants to open the video
        QMessageBox::StandardButton reply = QMessageBox::question(&window,
            "Export Complete",
            "MP4 video exported successfully. Would you like to open it now?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
        }
    }
}
void FileIOOperations::exportApp(MainWindow& window) {
    // First prompt for the save location
    QString saveDir = QFileDialog::getExistingDirectory(&window,
        "Choose Export Directory",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (saveDir.isEmpty()) {
        return; // User canceled
    }

    // Optional: Ask for project name (used for the folder and HTML title)
    bool ok;
    QString projectName = QInputDialog::getText(&window, "Project Name",
        "Enter a name for your animation:",
        QLineEdit::Normal,
        "MyAnimation", &ok);
    if (!ok || projectName.isEmpty()) {
        projectName = "Animation";
    }

    // Create a subdirectory with the project name
    QDir dir(saveDir);
    if (!dir.mkdir(projectName) && !dir.exists(projectName)) {
        QMessageBox::warning(&window, "Export Error",
            "Could not create directory: " + saveDir + "/" + projectName);
        return;
    }

    // Enter the project directory
    dir.cd(projectName);
    QString exportDir = dir.absolutePath();

    // Get frames from the MainWindow
    QList<DrawingScene*> frames = window.getFrames();

    if (frames.isEmpty()) {
        QMessageBox::warning(&window, "Export Error", "No frames available for export");
        return;
    }

    // Create settings dialog
    QDialog settingsDialog(&window);
    settingsDialog.setWindowTitle("Web Animation Settings");
    settingsDialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&settingsDialog);

    // FPS setting
    QHBoxLayout* fpsLayout = new QHBoxLayout();
    QLabel* fpsLabel = new QLabel("Frame Rate (FPS):", &settingsDialog);
    QSpinBox* fpsInput = new QSpinBox(&settingsDialog);
    fpsInput->setRange(1, 60);
    fpsInput->setValue(24); // Default 24 FPS
    fpsLayout->addWidget(fpsLabel);
    fpsLayout->addWidget(fpsInput);

    // Resolution settings
    QHBoxLayout* resLayout = new QHBoxLayout();
    QLabel* resLabel = new QLabel("Resolution:", &settingsDialog);

    // Get the first frame's dimensions as default
    QRectF sceneRect = frames.first()->sceneRect();

    QSpinBox* widthInput = new QSpinBox(&settingsDialog);
    widthInput->setRange(1, 10000);
    widthInput->setValue(sceneRect.width());

    QSpinBox* heightInput = new QSpinBox(&settingsDialog);
    heightInput->setRange(1, 10000);
    heightInput->setValue(sceneRect.height());

    resLayout->addWidget(resLabel);
    resLayout->addWidget(widthInput);
    resLayout->addWidget(new QLabel("x", &settingsDialog));
    resLayout->addWidget(heightInput);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &settingsDialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &settingsDialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &settingsDialog, &QDialog::reject);

    // Add all widgets to dialog
    layout->addLayout(fpsLayout);
    layout->addLayout(resLayout);
    layout->addWidget(buttonBox);

    // Show dialog and proceed if accepted
    if (settingsDialog.exec() != QDialog::Accepted) {
        return;
    }

    int fps = fpsInput->value();
    int width = widthInput->value();
    int height = heightInput->value();

    // Create progress dialog
    QProgressDialog progress("Exporting web animation...", "Cancel", 0, frames.size(), &window);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    // Render each frame
    QList<QString> frameFiles;
    for (int i = 0; i < frames.size(); i++) {
        progress.setValue(i);
        QApplication::processEvents();

        if (progress.wasCanceled()) {
            return;
        }

        // Create the frame image
        QImage frameImage(width, height, QImage::Format_ARGB32);
        frameImage.fill(Qt::transparent);

        QPainter painter(&frameImage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        frames[i]->render(&painter, QRectF(0, 0, width, height), frames[i]->sceneRect());
        painter.end();

        // Save the frame as a PNG file
        QString frameFilename = QString("frame_%1.png").arg(i, 4, 10, QChar('0'));
        QString framePath = exportDir + "/" + frameFilename;
        frameImage.save(framePath, "PNG");
        frameFiles.append(frameFilename); // Just the filename, not the full path
    }

    // Create HTML file
    QString htmlPath = exportDir + "/index.html";
    QFile htmlFile(htmlPath);

    if (!htmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(&window, "Export Error",
            "Failed to create HTML file: " + htmlPath);
        return;
    }

    QTextStream out(&htmlFile);

    // Write improved HTML with better UI
    out << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "    <meta charset=\"UTF-8\">\n"
        << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "    <title>" << projectName << "</title>\n"
        << "    <style>\n"
        << "        body {\n"
        << "            font-family: Arial, sans-serif;\n"
        << "            background-color: #f5f5f5;\n"
        << "            margin: 0;\n"
        << "            padding: 20px;\n"
        << "            text-align: center;\n"
        << "        }\n"
        << "        .container {\n"
        << "            max-width: 900px;\n"
        << "            margin: 0 auto;\n"
        << "            background-color: white;\n"
        << "            border-radius: 8px;\n"
        << "            box-shadow: 0 2px 10px rgba(0,0,0,0.1);\n"
        << "            padding: 20px;\n"
        << "        }\n"
        << "        h1 {\n"
        << "            color: #333;\n"
        << "        }\n"
        << "        .animation-container {\n"
        << "            margin: 20px 0;\n"
        << "            border: 1px solid #ddd;\n"
        << "            background-color: #fafafa;\n"
        << "            padding: 10px;\n"
        << "            border-radius: 4px;\n"
        << "        }\n"
        << "        img {\n"
        << "            max-width: 100%;\n"
        << "            height: auto;\n"
        << "            display: block;\n"
        << "            margin: 0 auto;\n"
        << "        }\n"
        << "        .controls {\n"
        << "            margin: 15px 0;\n"
        << "        }\n"
        << "        button {\n"
        << "            background-color: #4CAF50;\n"
        << "            border: none;\n"
        << "            color: white;\n"
        << "            padding: 8px 16px;\n"
        << "            text-align: center;\n"
        << "            text-decoration: none;\n"
        << "            display: inline-block;\n"
        << "            font-size: 16px;\n"
        << "            margin: 4px 2px;\n"
        << "            cursor: pointer;\n"
        << "            border-radius: 4px;\n"
        << "        }\n"
        << "        button:hover {\n"
        << "            background-color: #45a049;\n"
        << "        }\n"
        << "        .frame-info {\n"
        << "            margin: 10px 0;\n"
        << "            font-size: 14px;\n"
        << "            color: #666;\n"
        << "        }\n"
        << "        .slider-container {\n"
        << "            margin: 15px 0;\n"
        << "        }\n"
        << "        input[type=range] {\n"
        << "            width: 80%;\n"
        << "            max-width: 500px;\n"
        << "        }\n"
        << "    </style>\n"
        << "</head>\n"
        << "<body>\n"
        << "    <div class=\"container\">\n"
        << "        <h1>" << projectName << "</h1>\n"
        << "        <div class=\"animation-container\">\n"
        << "            <img src=\"" << frameFiles[0] << "\" id=\"animation\" alt=\"Animation frame\">\n"
        << "        </div>\n"
        << "        <div class=\"frame-info\">\n"
        << "            Frame: <span id=\"frameNumber\">1</span> / " << frames.size() << "\n"
        << "        </div>\n"
        << "        <div class=\"slider-container\">\n"
        << "            <input type=\"range\" min=\"0\" max=\"" << (frames.size() - 1) << "\" value=\"0\" class=\"slider\" id=\"frameSlider\">\n"
        << "        </div>\n"
        << "        <div class=\"controls\">\n"
        << "            <button id=\"prevBtn\">Previous</button>\n"
        << "            <button id=\"playBtn\">Play</button>\n"
        << "            <button id=\"nextBtn\">Next</button>\n"
        << "        </div>\n"
        << "    </div>\n"
        << "\n"
        << "    <script>\n"
        << "        // Animation frames\n"
        << "        const frames = [";

    // Add frame filenames
    for (int i = 0; i < frameFiles.size(); i++) {
        if (i > 0) out << ", ";
        out << "'" << frameFiles[i] << "'";
    }

    out << "];\n\n"
        << "        // Animation variables\n"
        << "        let currentFrame = 0;\n"
        << "        let isPlaying = false;\n"
        << "        let animationInterval;\n"
        << "        const frameDelay = " << (1000 / fps) << ";\n"
        << "\n"
        << "        // DOM elements\n"
        << "        const animationImg = document.getElementById('animation');\n"
        << "        const frameNumber = document.getElementById('frameNumber');\n"
        << "        const frameSlider = document.getElementById('frameSlider');\n"
        << "        const playBtn = document.getElementById('playBtn');\n"
        << "        const prevBtn = document.getElementById('prevBtn');\n"
        << "        const nextBtn = document.getElementById('nextBtn');\n"
        << "\n"
        << "        // Update the display with the current frame\n"
        << "        function updateDisplay() {\n"
        << "            animationImg.src = frames[currentFrame];\n"
        << "            frameNumber.textContent = currentFrame + 1;\n"
        << "            frameSlider.value = currentFrame;\n"
        << "        }\n"
        << "\n"
        << "        // Go to the next frame\n"
        << "        function nextFrame() {\n"
        << "            currentFrame = (currentFrame + 1) % frames.length;\n"
        << "            updateDisplay();\n"
        << "        }\n"
        << "\n"
        << "        // Go to the previous frame\n"
        << "        function prevFrame() {\n"
        << "            currentFrame = (currentFrame - 1 + frames.length) % frames.length;\n"
        << "            updateDisplay();\n"
        << "        }\n"
        << "\n"
        << "        // Toggle play/pause\n"
        << "        function togglePlay() {\n"
        << "            isPlaying = !isPlaying;\n"
        << "            if (isPlaying) {\n"
        << "                playBtn.textContent = 'Pause';\n"
        << "                animationInterval = setInterval(nextFrame, frameDelay);\n"
        << "            } else {\n"
        << "                playBtn.textContent = 'Play';\n"
        << "                clearInterval(animationInterval);\n"
        << "            }\n"
        << "        }\n"
        << "\n"
        << "        // Event listeners\n"
        << "        playBtn.addEventListener('click', togglePlay);\n"
        << "        prevBtn.addEventListener('click', () => {\n"
        << "            if (isPlaying) togglePlay();\n"
        << "            prevFrame();\n"
        << "        });\n"
        << "        nextBtn.addEventListener('click', () => {\n"
        << "            if (isPlaying) togglePlay();\n"
        << "            nextFrame();\n"
        << "        });\n"
        << "        frameSlider.addEventListener('input', () => {\n"
        << "            if (isPlaying) togglePlay();\n"
        << "            currentFrame = parseInt(frameSlider.value);\n"
        << "            updateDisplay();\n"
        << "        });\n"
        << "\n"
        << "        // Initialize with the first frame\n"
        << "        updateDisplay();\n"
        << "    </script>\n"
        << "</body>\n"
        << "</html>";

    htmlFile.close();

    // Complete progress
    progress.setValue(frames.size());

    // Open the animation in the browser
    QDesktopServices::openUrl(QUrl::fromLocalFile(htmlPath));

    QMessageBox::information(&window, "Web Export Complete",
        "Animation exported to: " + htmlPath);
}