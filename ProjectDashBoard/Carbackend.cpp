#include "Carbackend.h"
#include <QDebug>
#include <QSerialPort>

// Hàm khởi tạo (Chạy 1 lần khi bật App)
Carbackend::Carbackend(QObject *parent) : QObject(parent) {
    qDebug() << "=== TEST: CARBACKEND LAUNCHING SUCCESS, dia chi" << this;
    m_serial = new QSerialPort(this);

    // Cấu hình cổng COM (Nhớ đổi thành cổng USB to TTL của bạn)
    m_serial->setPortName("COM6");
    m_serial->setBaudRate(QSerialPort::Baud115200);

    if (m_serial->open(QIODevice::ReadWrite)) {
        qDebug() << "Connect success!";
        // Nối dây: Cứ có tín hiệu điện gửi tới là tự gọi hàm readFromSTM32
        connect(m_serial, &QSerialPort::readyRead, this, &Carbackend::readFromSTM32);
    } else {
        qDebug() << "Error open gate:" << m_serial->errorString();
    }
}

// Hàm bóc tách dữ liệu nhị phân (Bạn dán đoạn code giải mã vào đây)
// Hàm bóc tách dữ liệu nhị phân chuẩn 18 bytes
void Carbackend::readFromSTM32() {
    // 1. Đọc dữ liệu từ cổng Serial
    QByteArray data = m_serial->readAll();
    m_buffer.append(data);

    const uint8_t HEAD1 = 0xAA;
    const uint8_t HEAD2 = 0x55;

    // Kích thước chuẩn: 2 (Header) + 1 (Len) + 1 (MsgId) + 12 (Payload) + 1 (CRC) + 1 (EOF) = 18 bytes
    const int EXPECTED_FRAME_SIZE = 18;

    while (m_buffer.size() >= EXPECTED_FRAME_SIZE) {

        // 1. Quét tìm VỊ TRÍ CHÍNH XÁC của cặp 0xAA 0x55
        int syncIdx = -1;
        for (int i = 0; i <= m_buffer.size() - 2; ++i) {
            if (static_cast<uint8_t>(m_buffer[i]) == HEAD1 && static_cast<uint8_t>(m_buffer[i+1]) == HEAD2) {
                syncIdx = i;
                break;
            }
        }

        // 2. Không thấy Header -> Xóa rác, giữ 1 byte cuối đề phòng bị cắt nửa
        if (syncIdx == -1) {
            if (!m_buffer.isEmpty() && static_cast<uint8_t>(m_buffer.back()) == HEAD1) {
                m_buffer = m_buffer.right(1);
            } else {
                m_buffer.clear();
            }
            break;
        }

        // 3. Xóa mọi byte rác đứng trước Header
        if (syncIdx > 0) {
            m_buffer.remove(0, syncIdx);
        }

        // 4. Nếu chưa đủ 18 bytes -> Đợi UART gửi thêm
        if (m_buffer.size() < EXPECTED_FRAME_SIZE) {
            break;
        }

        // 5. Cắt đúng 18 bytes để xử lý
        QByteArray frame = m_buffer.left(EXPECTED_FRAME_SIZE);
        m_buffer.remove(0, EXPECTED_FRAME_SIZE);

        // --- BÓC TÁCH CHUẨN XÁC THEO CẤU TRÚC PAYLOAD 12-BYTE ---
        // frame[0] = 0xAA, frame[1] = 0x55
        // frame[2] = len (12)
        // frame[3] = msgId
        // Payload bắt đầu từ index 4:
        //   - frame[4]       : seq
        //   - frame[5..6]    : cycle_time_s
        //   - frame[7..8]    : speed_x10_kmh
        //   - frame[9..10]   : rpm
        //   - frame[11]      : phase
        //   - frame[12..15]  : last_ms

        uint8_t  seq          = static_cast<uint8_t>(frame[4]);

        uint16_t cycle_time_s = (static_cast<uint8_t>(frame[6]) << 8) | static_cast<uint8_t>(frame[5]);

        uint16_t speed_raw    = (static_cast<uint8_t>(frame[8]) << 8) | static_cast<uint8_t>(frame[7]);
        double   speed_kmh    = speed_raw / 10.0;

        uint16_t rpm          = (static_cast<uint8_t>(frame[10]) << 8) | static_cast<uint8_t>(frame[9]);

        uint8_t  phase        = static_cast<uint8_t>(frame[11]);

        uint32_t last_ms      = (static_cast<uint32_t>(static_cast<uint8_t>(frame[15])) << 24) |
                           (static_cast<uint32_t>(static_cast<uint8_t>(frame[14])) << 16) |
                           (static_cast<uint32_t>(static_cast<uint8_t>(frame[13])) << 8)  |
                           static_cast<uint32_t>(static_cast<uint8_t>(frame[12]));
        emit speedChanged(speed_kmh);
        emit rpmChanged(static_cast<double>(rpm));
        // 6. In ra Terminal kiểm tra kết quả
        qDebug() << "====================";
        qDebug() << "Seq  :" << seq;
        qDebug() << "Speed:" << speed_kmh << "km/h";
        qDebug() << "RPM  :" << rpm << "v/p";
        qDebug() << "Phase:" << phase;
        qDebug() << "Cycle:" << cycle_time_s << "s";
        qDebug() << "Time :" << last_ms << "ms";
    }
}