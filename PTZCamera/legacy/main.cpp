// 이전 Pi 직접 연결 화면의 별도 실행점이다.
#include "LegacyMainWindow.h"
#include <QApplication>
int main(int argc, char *argv[])
{
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    QApplication app(argc, argv);
    LegacyMainWindow window;
    window.show();
    return app.exec();
}
