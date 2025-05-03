#include <QtWidgets>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow win;
    win.setWindowTitle("Qt Vector Drawing - Untitled");
    win.show();
    return app.exec();
}

//#include "main.moc"