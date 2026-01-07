/**
 * @file serialservice.cpp
 * @brief Serial service: enumerate ports, open/close, send/receive with CRLF framing.
 */

#include "serialservice.h"

#include <QSerialPortInfo>

SerialService::SerialService(QObject *parent)
    : QObject(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialService::onReadyRead);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialService::onPortErrorOccurred);
#else
    connect(&m_port, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(onPortErrorOccurred(QSerialPort::SerialPortError)));
#endif
}

void SerialService::refreshPorts()
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto& info : infos)
        ports << info.portName();
    emit portsUpdated(ports);
}

QSerialPort::Parity SerialService::parseParity(const QString& parityText)
{
    const QString p = parityText.trimmed().toLower();
    if (p == "even") return QSerialPort::EvenParity;
    if (p == "odd")  return QSerialPort::OddParity;
    return QSerialPort::NoParity;
}

QSerialPort::DataBits SerialService::parseDataBits(int dataBits)
{
    if (dataBits == 7) return QSerialPort::Data7;
    return QSerialPort::Data8;
}

QSerialPort::StopBits SerialService::parseStopBits(int stopBits)
{
    if (stopBits == 2) return QSerialPort::TwoStop;
    return QSerialPort::OneStop;
}

void SerialService::openPort(const QString &portName,
                             int baud,
                             int dataBits,
                             const QString &parityText,
                             int stopBits)
{
    if (m_port.isOpen())
        m_port.close();

    m_port.setPortName(portName);
    m_port.setBaudRate(baud);
    m_port.setDataBits(parseDataBits(dataBits));
    m_port.setParity(parseParity(parityText));
    m_port.setStopBits(parseStopBits(stopBits));
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite))
    {
        emit opened(false, QStringLiteral("打开串口失败：%1").arg(m_port.errorString()));
        return;
    }

    m_rxBuffer.clear();
    emit opened(true, QString());
}

void SerialService::closePort()
{
    if (!m_port.isOpen())
    {
        emit closed();
        return;
    }

    m_port.close();
    m_rxBuffer.clear();
    emit closed();
}

void SerialService::sendFrame(const QString &frame)
{
    if (!m_port.isOpen())
    {
        emit error(QStringLiteral("串口未打开，无法发送"));
        return;
    }

    QString payload = frame;
    if (!payload.endsWith(QStringLiteral("\r\n")))
        payload += QStringLiteral("\r\n");

    const QByteArray data = payload.toUtf8();
    const qint64 n = m_port.write(data);
    if (n < 0)
    {
        emit error(QStringLiteral("串口发送失败：%1").arg(m_port.errorString()));
        return;
    }

    emit txRaw(payload.trimmed());
}

void SerialService::onReadyRead()
{
    if (!m_port.isOpen())
        return;

    m_rxBuffer.append(m_port.readAll());

    // CRLF framing
    while (true)
    {
        const int end = m_rxBuffer.indexOf('\n');
        if (end < 0)
        {
            if (m_rxBuffer.size() > 8192)
            {
                m_rxBuffer.clear();
                emit error(QStringLiteral("接收缓冲过长，已清空"));
            }
            return;
        }

        const QByteArray line = m_rxBuffer.left(end + 1);
        m_rxBuffer.remove(0, end + 1);

        const QString frame = QString::fromUtf8(line).trimmed();
        if (!frame.isEmpty())
            emit rxRaw(frame);
    }
}

void SerialService::onPortErrorOccurred(QSerialPort::SerialPortError e)
{
    if (e == QSerialPort::NoError)
        return;

    emit error(QStringLiteral("串口错误：%1").arg(m_port.errorString()));
}
