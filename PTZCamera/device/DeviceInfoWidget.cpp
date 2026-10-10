// 장치 상태를 표시하고 조회 요청 의도만 전달한다. ONVIF SOAP를 직접 호출하지 않는다.
#include "DeviceInfoWidget.h"
#include <QGridLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
DeviceInfoWidget::DeviceInfoWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this); auto *grid = new QGridLayout;
    const QStringList names = {QStringLiteral("Camera"), QStringLiteral("Name"), QStringLiteral("IP"), QStringLiteral("Status"),
        QStringLiteral("ONVIF"), QStringLiteral("RTSP"), QStringLiteral("Recording"),
        QStringLiteral("Codec"), QStringLiteral("Resolution"), QStringLiteral("FPS"), QStringLiteral("Packets"), QStringLiteral("Bytes")};
    auto *cameraHeading = new QLabel(QStringLiteral("CAMERA STATUS"), this);
    auto *streamHeading = new QLabel(QStringLiteral("STREAM INFO"), this);
    cameraHeading->setObjectName(QStringLiteral("panelHeading"));
    streamHeading->setObjectName(QStringLiteral("panelHeading"));
    grid->addWidget(cameraHeading, 0, 0, 1, 2);
    grid->addWidget(streamHeading, 0, 2, 1, 2);
    for (int i = 0; i < names.size(); ++i) {
        auto *value = new QLabel(QStringLiteral("—"), this); m_values.append(value);
        value->setObjectName(QStringLiteral("infoValue"));
        // 기존 두 묶음 배치를 유지하며 장치 상태와 스트림 정보를 구분한다.
        grid->addWidget(new QLabel(names[i], this), i % 7 + 1, (i / 7) * 2);
        grid->addWidget(value, i % 7 + 1, (i / 7) * 2 + 1);
    }
    auto *details = new QHBoxLayout; details->addLayout(grid, 1);
    auto *form = new QFormLayout;
    m_devices = new QComboBox(this); m_devices->setObjectName(QStringLiteral("onvifDevices"));
    m_service = new QLineEdit(QStringLiteral("http://192.168.0.92:8080/onvif/device_service"), this); m_service->setObjectName(QStringLiteral("onvifService"));
    m_username = new QLineEdit(this); m_username->setPlaceholderText(QStringLiteral("Use saved account if empty"));
    m_password = new QLineEdit(this); m_password->setEchoMode(QLineEdit::Password);
    m_profile = new QLineEdit(QStringLiteral("main"), this);
    auto *discoveryRow = new QWidget(this); auto *discoveryLayout = new QHBoxLayout(discoveryRow);
    discoveryLayout->setContentsMargins(0, 0, 0, 0);
    m_devices->setMinimumWidth(0); m_devices->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_streamUri = new QLineEdit(this); m_streamUri->setObjectName(QStringLiteral("vmsRtspUri"));
    m_streamUri->setReadOnly(true); m_streamUri->setMinimumWidth(0); m_streamUri->setPlaceholderText(QStringLiteral("RTSP address"));
    m_streamState = new QLabel(this); m_streamState->setObjectName(QStringLiteral("discoveredStreamState"));
    discoveryLayout->addWidget(m_devices, 1); discoveryLayout->addWidget(m_streamUri, 2); discoveryLayout->addWidget(m_streamState);
    form->addRow(QStringLiteral("DISCOVERED CAMERAS"), discoveryRow);
    form->addRow(QStringLiteral("ONVIF address"), m_service);
    form->addRow(QStringLiteral("Account"), m_username); form->addRow(QStringLiteral("Password"), m_password);
    form->addRow(QStringLiteral("Profile"), m_profile);
    auto *registerButton = new QPushButton(QStringLiteral("ADD CAMERA"), this);
    registerButton->setObjectName(QStringLiteral("registerOnvifCamera")); form->addRow(registerButton);
    details->addLayout(form, 2); layout->addLayout(details);
    connect(m_devices, &QComboBox::currentIndexChanged, this, [this](int index) {
        clearStreamUri();
        if (index < 0 || index >= m_discovered.size()) return;
        const auto &device = m_discovered[index]; m_selectedDeviceId = device.cameraId;
        m_service->setText(device.deviceServiceUrl);
        const QUrl uri(device.rtspUri);
        if (!uri.isValid() || uri.scheme() != QStringLiteral("rtsp") || uri.host().isEmpty() || !uri.userInfo().isEmpty()) {
            m_streamUri->setPlaceholderText(QStringLiteral("VMS address unavailable")); return;
        }
        m_streamUri->setText(uri.toString(QUrl::FullyEncoded)); m_streamUri->setToolTip(m_streamUri->text());
        m_streamUri->setCursorPosition(0);
        m_streamState->setText(!device.registered ? QStringLiteral("Not added") : device.ready ? QStringLiteral("Ready") : QStringLiteral("Waiting"));
        m_streamState->setToolTip(QStringLiteral("ADD CAMERA registers the device and starts the VMS stream. Address display alone does not mean video is ready."));
    });
    connect(m_service, &QLineEdit::textEdited, this, [this] {
        m_devices->setCurrentIndex(-1); m_devices->setPlaceholderText(QStringLiteral("Manual ONVIF address"));
        clearStreamUri(); m_streamUri->setPlaceholderText(QStringLiteral("Address after ADD CAMERA"));
    });
    connect(registerButton, &QPushButton::clicked, this, [this] {
        emit registerRequested(m_service->text().trimmed(), m_username->text(), m_password->text(), m_profile->text().trimmed());
        m_password->clear();
    });
    auto *buttons = new QHBoxLayout;
    auto *refresh = new QPushButton(QStringLiteral("REFRESH"), this);
    auto *discover = new QPushButton(QStringLiteral("DISCOVER CAMERAS"), this);
    discover->setObjectName(QStringLiteral("discoverOnvifCameras")); m_discover = discover;
    auto *list = new QPushButton(QStringLiteral("CAMERA LIST"), this);
    connect(list, &QPushButton::clicked, this, &DeviceInfoWidget::listRequested);
    buttons->addWidget(refresh); buttons->addWidget(discover); buttons->addWidget(list); buttons->addStretch(); layout->addLayout(buttons);
    connect(refresh, &QPushButton::clicked, this, [this] { if (!m_cameraId.isEmpty()) emit refreshRequested(m_cameraId); });
    connect(discover, &QPushButton::clicked, this, &DeviceInfoWidget::discoverRequested);
}
void DeviceInfoWidget::setCamera(const CameraInfo &camera) {
    m_cameraId = camera.id;
    const QStringList values = {camera.id, camera.name, camera.ipAddress,
        camera.id.isEmpty() ? QString() : camera.online ? QStringLiteral("ONLINE") : camera.status,
        camera.onvifStatus, camera.rtspStatus, camera.recording ? QStringLiteral("ACTIVE") : camera.recordingState,
        camera.codec, camera.resolution.isValid() ? QStringLiteral("%1 × %2").arg(camera.resolution.width()).arg(camera.resolution.height()) : QString(),
        camera.fps > 0 ? QString::number(camera.fps) : QString(), QString::number(camera.packets), QString::number(camera.bytes)};
    for (int i = 0; i < m_values.size(); ++i)
        m_values[i]->setText(camera.id.isEmpty() || values[i].isEmpty() || values[i] == QStringLiteral("NOT_IMPLEMENTED")
            ? QStringLiteral("—") : values[i]);
}

void DeviceInfoWidget::setDiscoveredCameras(const QList<DiscoveredCamera> &devices) {
    setDiscoveryState(false, devices.isEmpty() ? QStringLiteral("No cameras found; enter ONVIF address") : QString());
    m_discovered = devices;
    for (const auto &device : devices) m_devices->addItem(device.name + QStringLiteral(" / ") + device.address + QStringLiteral(" / ") + device.source, device.deviceServiceUrl);
    if (!devices.isEmpty()) m_devices->setCurrentIndex(0);
}

void DeviceInfoWidget::setStreamUri(const QString &cameraId, const QUrl &uri) {
    if (cameraId != m_selectedDeviceId || cameraId.isEmpty() || !uri.isValid() || uri.scheme() != QStringLiteral("rtsp")
        || uri.host().isEmpty()) return;
    // 직접 RTSP 인증은 player의 메모리에서만 사용한다. 화면·복사·tooltip에는 계정이 없다.
    auto display = uri; display.setUserInfo(QString());
    m_streamUri->setText(display.toString(QUrl::FullyEncoded)); m_streamUri->setToolTip(m_streamUri->text());
    m_streamUri->setCursorPosition(0);
    m_streamState->setText(QStringLiteral("Ready"));
    for (auto &device : m_discovered) if (device.cameraId == cameraId) {
        device.rtspUri = m_streamUri->text(); device.registered = true; device.ready = true;
    }
}
void DeviceInfoWidget::clearStreamUri() {
    m_selectedDeviceId.clear(); m_streamUri->clear(); m_streamUri->setToolTip(QString());
    m_streamUri->setPlaceholderText(QStringLiteral("RTSP address")); m_streamState->clear();
}
void DeviceInfoWidget::cameraRegistered(const QString &cameraId) {
    m_selectedDeviceId = cameraId; m_streamState->setText(QStringLiteral("Connecting"));
    const int index = m_devices->currentIndex();
    if (index >= 0 && index < m_discovered.size() && m_service->text().trimmed() == m_discovered[index].deviceServiceUrl) {
        m_discovered[index].cameraId = cameraId; m_discovered[index].registered = true; m_discovered[index].ready = false;
    }
}
void DeviceInfoWidget::setStreamQueryError(const QString &cameraId, const QString &message) {
    if (cameraId != m_selectedDeviceId || cameraId.isEmpty()) return;
    m_streamState->setText(QStringLiteral("Stream error")); m_streamState->setToolTip(message);
}

void DeviceInfoWidget::setDiscoveryState(bool busy, const QString &message) {
    m_discover->setEnabled(!busy);
    m_discover->setText(busy ? QStringLiteral("SEARCHING...") : QStringLiteral("DISCOVER CAMERAS"));
    m_discovered.clear(); m_devices->clear(); clearStreamUri(); m_devices->setPlaceholderText(message); m_devices->setCurrentIndex(-1);
}
