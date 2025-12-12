#include <QApplication>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Son Fusion");
    app.setApplicationVersion("2.1");

    // Initialiser les ressources
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    MainWindow window;
    window.show();

    return app.exec();
}
