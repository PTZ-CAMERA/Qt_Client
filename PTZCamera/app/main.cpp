// 프로그램 시작점이다. Qt 이벤트 루프를 만들고 메인 창을 표시한다.
// 영상 재생 백엔드 선택은 이곳에서, 실제 화면과 통신 연결은 각 클래스에서 담당한다.
#include "MainWindow.h"

#include <QApplication>

// QApplication은 위젯의 입력, 화면 갱신, 타이머와 비동기 신호를 처리한다.
// app.exec()가 실행되는 동안 창이 사용자 입력과 네트워크 응답에 반응한다.
int main(int argc, char *argv[])
{
    // RTSP TCP/UDP 전송 설정을 처리하는 FFmpeg 백엔드를 앱 생성 전에 지정한다.
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Mini VMS Desktop Client"));
    // 기본 Dummy Mode; --no-dummy로 시작하면 DEVICE에서 VMS에 연결한다.
    MainWindow window(nullptr, !app.arguments().contains(QStringLiteral("--no-dummy")));
    window.show();
    // 이벤트 루프가 종료되면 지역 변수인 window와 app도 순서대로 정리된다.
    return app.exec();
}
