#include <QtWidgets/QApplication>
#include <QtGui/QScreen>

#include "ui/MainWindow.h"

#include <windows.h>

int main(int argc, char *argv[]) {

    AllocConsole();
    freopen("CON", "w", stdout);

    QApplication app(argc, argv);

    MainWindow w;
    QScreen   *pScreen = app.primaryScreen();

    w.move((pScreen->size().width() - w.width()) / 2,
           (pScreen->size().height() - w.height()) / 2);
    w.show();

    return app.exec();
}
