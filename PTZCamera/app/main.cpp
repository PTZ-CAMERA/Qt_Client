#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PTZ Object Tracking Camera"));
    MainWindow window;
    window.show();
    return app.exec();
}
