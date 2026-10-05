#ifndef CARBACKEND_H
#define CARBACKEND_H
#include <QObject>
#include <QElapsedTimer>
#include <QSerialPort>
#include <QTimer>
#include <QVariantList>

class Carbackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double speed MEMBER m_speed NOTIFY changed)
    Q_PROPERTY(int rpm MEMBER m_rpm NOTIFY changed)
    Q_PROPERTY(int cycleTime MEMBER m_cycle NOTIFY changed)
    Q_PROPERTY(int phase MEMBER m_phase NOTIFY changed)
    Q_PROPERTY(int seq MEMBER m_seq NOTIFY changed)
    Q_PROPERTY(bool connected MEMBER m_connected NOTIFY changed)
    Q_PROPERTY(bool linkOk MEMBER m_linkOk NOTIFY changed)      // false = CAN stale > 3 s
    Q_PROPERTY(int faultCode MEMBER m_fault NOTIFY changed)     // 0 = no fault
    Q_PROPERTY(int totalFrames MEMBER m_total NOTIFY changed)
    Q_PROPERTY(int crcErrors MEMBER m_crcErr NOTIFY changed)
    Q_PROPERTY(int fps MEMBER m_fps NOTIFY changed)
    Q_PROPERTY(QString portName MEMBER m_portName NOTIFY changed)
    Q_PROPERTY(QVariantList frames MEMBER m_frames NOTIFY changed)
public:
    explicit Carbackend(QObject *parent = nullptr);
    Q_INVOKABLE void open(const QString &port);
    Q_INVOKABLE void clearLog();
signals:
    void changed();
private slots:
    void readFromSTM32();
    void tick();
private:
    void handleFrame(uint8_t id, const QByteArray &p);
    void log(const QString &canId, const QString &name, const QByteArray &data, bool ok);
    static uint8_t crc8(const QByteArray &d);

    QSerialPort *m_serial;
    QByteArray m_buf;
    QTimer m_timer;
    QElapsedTimer m_since;
    QVariantList m_frames;
    QString m_portName = "COM6";
    double m_speed = 0;
    int m_rpm = 0, m_cycle = 0, m_phase = 0, m_seq = 0, m_fault = 0;
    int m_total = 0, m_crcErr = 0, m_fps = 0, m_lastTotal = 0;
    bool m_connected = false, m_linkOk = false;
};
#endif
