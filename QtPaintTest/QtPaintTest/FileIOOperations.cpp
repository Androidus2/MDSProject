#include "FileIOOperations.h"
#include "DrawingScene.h"
#include "DrawingManager.h"
#include "GifEncoder.h"

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
void FileIOOperations::exportApp(MainWindow& window) {
    // Get frames and settings (same as before)
    QList<DrawingScene*> frames = window.getFrames();

    // Create a QMovie to write the GIF
    QList<QImage> images;

    // Render each frame
    for (int i = 0; i < frames.size(); i++) {
        QImage frameImage(frames[i]->sceneRect().size().toSize(), QImage::Format_ARGB32);
        frameImage.fill(Qt::transparent);

        QPainter painter(&frameImage);
        painter.setRenderHint(QPainter::Antialiasing);
        frames[i]->render(&painter);
        painter.end();

        images.append(frameImage);
    }

    // Export individual frames and create a basic HTML page to preview the animation
    QTemporaryDir tempDir;
    QFile htmlFile(tempDir.path() + "/animation.html");
    if (htmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&htmlFile);
        out << "<!DOCTYPE html>\n<html>\n<head>\n<title>Application Preview</title>\n";
        out << "<style>\n.animation { width: 100%; text-align: center; }\n";
        out << "img { border: 1px solid #ddd; }\n</style>\n</head>\n<body>\n";
        out << "<div class='animation'>\n";
        out << "<img src='frame_0000.png' id='animframe'>\n";
        out << "<p>Frame: <span id='framenum'>1</span> / " << images.size() << "</p>\n";
        out << "<button onclick='toggleAnimation()'>Play/Pause</button>\n";
        out << "</div>\n";
        out << "<script>\n";
        out << "let frames = [";

        for (int i = 0; i < images.size(); i++) {
            QString framePath = QString("frame_%1.png").arg(i, 4, 10, QChar('0'));
            images[i].save(tempDir.path() + "/" + framePath, "PNG");

            if (i > 0) out << ", ";
            out << "'" << framePath << "'";
        }

        out << "];\n";
        out << "let currentFrame = 0;\n";
        out << "let isPlaying = false;\n";
        out << "let interval;\n";
        out << "function updateFrame() {\n";
        out << "  document.getElementById('animframe').src = frames[currentFrame];\n";
        out << "  document.getElementById('framenum').textContent = currentFrame + 1;\n";
        out << "  currentFrame = (currentFrame + 1) % frames.length;\n";
        out << "}\n";
        out << "function toggleAnimation() {\n";
        out << "  if (isPlaying) {\n";
        out << "    clearInterval(interval);\n";
        out << "    isPlaying = false;\n";
        out << "  } else {\n";
        out << "    interval = setInterval(updateFrame, " << (1000 / 8) << ");\n";
        out << "    isPlaying = true;\n";
        out << "  }\n";
        out << "}\n";
        out << "</script>\n";
        out << "</body>\n</html>";
        htmlFile.close();

        // Open the preview
        QDesktopServices::openUrl(QUrl::fromLocalFile(htmlFile.fileName()));
    }
}