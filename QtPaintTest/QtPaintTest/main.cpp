#include <QtWidgets>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow win;
    win.setWindowTitle("Vecmate - Untitled");
	win.setWindowIcon(QIcon("icons/logo.png"));
    win.show();
    return app.exec();
}

//#include "main.moc"