#pragma once
/**
 * @file serialservice.h
 * @brief 串口服务：封装 QSerialPort，负责端口枚举、打开关闭、收发帧、切帧
 *
 * ✅ 需求对齐：
 * - 上位机 Windows-only，用 QSerialPort
 * - 下位机分动作指令；回包 *E 表示动作完成（带错误信息）
 * - 这里负责“字节流 -> 帧字符串”的切帧（按 '*' ... '#'）
 *
 * ⚠️ 你之前报过：
 *   Use of undeclared identifier 'QShortcut'
 * 这类错误通常是头文件没 include。
 * 同理，QSerialPort 相关错误需要：
 *   #include <QSerialPort>
 *   #include <QSerialPortInfo>
 * 且 CMakeLists/Qt 模块要 link：Qt::SerialPort
 */

#include <QObject>
#include <QString>
#include <QStringList>

#include <QSerialPort>

class SerialService : public QObject
{
    Q_OBJECT
public:
    explicit SerialService(QObject* parent = nullptr);

    /**
     * @brief 枚举串口端口名（例如 COM3）
     */
    void refreshPorts();

    /**
     * @brief 打开串口
     * @param portName 例如 "COM3"
     * @param baud 波特率
     * @param dataBits 7/8
     * @param parityText "None"/"Even"/"Odd"
     * @param stopBits 1/2
     */
    void openPort(const QString& portName,
                  int baud,
                  int dataBits,
                  const QString& parityText,
                  int stopBits);

    /**
     * @brief 关闭串口
     */
    void closePort();

    bool isOpen() const { return m_port.isOpen(); }

    /**
     * @brief 发送一条已打包好的帧（形如 "*L,1,ALL,350,0,5,1,2,3,4,5#"）
     * @note 这里不做协议层校验（由 Protocol 负责）；SerialService 只负责发送与回显信号
     */
    void sendFrame(const QString& frame);

signals:
    // 端口枚举完成
    void portsUpdated(const QStringList& ports);

    // 打开/关闭
    void opened(bool ok, const QString& err);
    void closed();

    // 原始帧收发（已切帧）
    void rxRaw(const QString& frame);
    void txRaw(const QString& frame);

    // 串口错误
    void error(const QString& err);

private slots:
    void onReadyRead();
    void onPortErrorOccurred(QSerialPort::SerialPortError e);

private:
    // parity 解析
    static QSerialPort::Parity parseParity(const QString& parityText);
    static QSerialPort::DataBits parseDataBits(int dataBits);
    static QSerialPort::StopBits parseStopBits(int stopBits);

private:
    QSerialPort m_port;

    // 切帧缓冲（字节流 -> 多帧）
    QByteArray m_rxBuffer;
};
