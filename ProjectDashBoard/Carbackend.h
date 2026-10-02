#ifndef CARBACKEND_H
#define CARBACKEND_H

#include <QObject>
#include <QQmlEngine>
#include <QSerialPort>

#pragma pack(push, 1)
typedef struct
{
    uint8_t  seq;
    uint16_t cycle_time_s;
    uint16_t speed_x10_kmh;
    uint16_t rpm;
    uint8_t  phase;
    uint32_t last_ms;
} UART_CycleSnapshot_t;
#pragma pack(pop)

class Carbackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit Carbackend(QObject *parent = nullptr);
signals:
// Tín hiệu đẩy lên QML
    void rpmChanged(double rpm);
    void speedChanged(double speed);

private slots:
// Slot tự động chạy khi có dữ liệu từ cáp USB
    void readFromSTM32();

private:
    QSerialPort *m_serial;
    QByteArray m_buffer;
};

#endif
