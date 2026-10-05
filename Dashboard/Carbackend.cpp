#include "Carbackend.h"
#include <QDateTime>

Carbackend::Carbackend(QObject *parent) : QObject(parent)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &Carbackend::readFromSTM32);
    connect(&m_timer, &QTimer::timeout, this, &Carbackend::tick);
    m_timer.start(1000);
    m_since.start();
    open(m_portName);
}

void Carbackend::open(const QString &port)
{
    if (m_serial->isOpen()) m_serial->close();
    m_portName = port;
    m_serial->setPortName(port);
    m_serial->setBaudRate(QSerialPort::Baud115200);   // 8N1
    m_connected = m_serial->open(QIODevice::ReadOnly);
    emit changed();
}

void Carbackend::clearLog() { m_frames.clear(); emit changed(); }

// CRC-8 poly 0x07, init 0, over MSG_ID + PAYLOAD (same as gateway)
uint8_t Carbackend::crc8(const QByteArray &d)
{
    uint8_t crc = 0;
    for (uchar b : d) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

// Frame: AA 55 LEN MSG_ID PAYLOAD[LEN] CRC8 0A  -> LEN + 6 bytes (variable length)
void Carbackend::readFromSTM32()
{
    m_buf.append(m_serial->readAll());
    while (true) {
        int s = m_buf.indexOf("\xAA\x55", 0);
        if (s < 0) { if (m_buf.size() > 1) m_buf = m_buf.right(1); break; }
        if (s > 0) m_buf.remove(0, s);
        if (m_buf.size() < 3) break;
        int len = uchar(m_buf[2]);
        if (len > 32) { m_buf.remove(0, 2); continue; }          // invalid length -> resync
        int total = len + 6;
        if (m_buf.size() < total) break;                          // wait for more bytes

        QByteArray body = m_buf.mid(3, len + 1);                  // MSG_ID + PAYLOAD
        bool ok = uchar(m_buf[4 + len]) == crc8(body) && uchar(m_buf[5 + len]) == 0x0A;
        if (ok) handleFrame(uchar(body[0]), body.mid(1));
        else { ++m_crcErr; log("—", "CRC_ERROR", m_buf.left(total), false); }
        m_buf.remove(0, ok ? total : 2);                          // bad frame -> skip header only
    }
    emit changed();
}

void Carbackend::handleFrame(uint8_t id, const QByteArray &p)
{
    m_since.restart();
    ++m_total;
    if (id == 0x01 && p.size() >= 8) {                            // CYCLE_SNAPSHOT
        auto u16 = [&](int i) { return int(uchar(p[i]) | (uchar(p[i + 1]) << 8)); };
        m_seq = uchar(p[0]); m_cycle = u16(1); m_speed = u16(3) / 10.0;
        m_rpm = u16(5);      m_phase = uchar(p[7]);
        m_linkOk = true;
        log("0x088", "VEHICLE_CYCLE", p.left(8), true);           // = original CAN payload
    } else if (id == 0x03 && p.size() >= 1) {                     // FAULT
        m_fault = uchar(p[0]);
        QByteArray d = p.left(1); d.append(QByteArray(7, 0));
        log("0x0A0", "SIM_FAULT", d, false);
    } else {
        log("—", QString("UART_MSG_%1").arg(id, 2, 16, QChar('0')), p, true);
    }
}

void Carbackend::log(const QString &canId, const QString &name, const QByteArray &data, bool ok)
{
    QVariantMap f;
    f["time"] = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    f["id"] = canId; f["name"] = name; f["dlc"] = data.size(); f["ok"] = ok;
    f["data"] = QString::fromLatin1(data.toHex(' ').toUpper());
    m_frames.prepend(f);
    while (m_frames.size() > 100) m_frames.removeLast();
}

void Carbackend::tick()
{
    m_fps = m_total - m_lastTotal; m_lastTotal = m_total;
    if (m_since.elapsed() > 3000) m_linkOk = false;               // matches gateway stale timeout
    emit changed();
}
