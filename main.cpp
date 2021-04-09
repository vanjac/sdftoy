#include "mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QColorSpace>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    auto format = QSurfaceFormat::defaultFormat();
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
#ifdef QT_DEBUG
    format.setOption(QSurfaceFormat::DebugContext);
#endif
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);

    MainWindow w;
    w.show();
    return a.exec();
}
