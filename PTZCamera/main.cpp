#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_FFMPEG_RTSP_TRANSPORT", "tcp");
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PTZ Object Tracking Camera"));
    MainWindow window;
    window.show();
    return app.exec();
}
