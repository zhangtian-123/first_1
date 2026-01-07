#include "protocol.h"
#include "protocol.h"

#include <algorithm>
#include <QRandomGenerator>

#ifdef _WIN32
    #include <windows.h>
#else
    #if __has_include(<QTextCodec>)
        #include <QTextCodec>
    #endif
#endif

namespace
{
QString joinInts(const QVector<int>& vals)
{
    QStringList parts;
    parts.reserve(vals.size());
    for (int v : vals) parts << QString::number(v);
    return parts.join(",");
}

QString toHex6(const QColor& c)
{
    return QString("%1%2%3")
        .arg(c.red(),   2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(),  2, 16, QChar('0'))
        .toUpper();
}

QString bytesToSpacedHex(const QByteArray& bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (unsigned char b : bytes)
        parts << QString::number(b, 16).rightJustified(2, QChar('0')).toUpper();
    return parts.join(' ');
}

QByteArray toGb2312Bytes(const QString& text)
{
#ifdef _WIN32
    if (text.isEmpty())
        return QByteArray();
    const int wideLen = text.size();
    const wchar_t* wide = reinterpret_cast<const wchar_t*>(text.utf16());
    int len = WideCharToMultiByte(936, 0, wide, wideLen, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return QByteArray();
    QByteArray out;
    out.resize(len);
    WideCharToMultiByte(936, 0, wide, wideLen, out.data(), len, nullptr, nullptr);
    return out;
#else
    #if __has_include(<QTextCodec>)
    if (auto* codec = QTextCodec::codecForName("GB2312"))
        return codec->fromUnicode(text);
    if (auto* codec = QTextCodec::codecForName("GBK"))
        return codec->fromUnicode(text);
    #endif
    return text.toUtf8();
#endif
}
}

namespace Protocol
{
QString escapeVoiceText(const QString &text)
{
    QString out;
    out.reserve(text.size() * 2);
    for (const QChar ch : text)
    {
        if (ch == '\\') out += "\\\\";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\n') out += "\\n";
        else if (ch == ';') out += "\\;";
        else if (ch == ',') out += "\\,";
        else out += ch;
    }
    return out;
}

QString packLedConfig(const DeviceProps &dev, const QVector<ColorItem> &colors)
{
    QString out = QStringLiteral("LEDSET:%1,%2,%3,%4,%5")
                      .arg(dev.ledCount)
                      .arg(dev.onMs)
                      .arg(dev.gapMs)
                      .arg(dev.brightness)
                      .arg(colors.size());

    for (const auto& c : colors)
        out += QStringLiteral(",%1").arg(toHex6(c.rgb));

    return out + "\r\n";
}

QString packVoiceConfig1(const VoiceProps &v)
{
    return QStringLiteral("VOICESET1:%1,%2,%3,%4,%5\r\n")
        .arg(v.announcer)
        .arg(v.voiceStyle)
        .arg(v.voiceSpeed)
        .arg(v.voicePitch)
        .arg(v.voiceVolume);
}

QString packVoiceConfig2(const VoiceProps &v)
{
    return QStringLiteral("VOICESET2:%1,%2,%3,%4,%5\r\n")
        .arg(v.announcer)
        .arg(v.voiceStyle)
        .arg(v.voiceSpeed)
        .arg(v.voicePitch)
        .arg(v.voiceVolume);
}

QString packBeepConfig(const DeviceProps &dev)
{
    return QStringLiteral("BEEPSET:%1,%2\r\n")
        .arg(dev.buzzerDurMs)
        .arg(dev.buzzerFreq);
}

QString packBeepTest(const DeviceProps &dev)
{
    Q_UNUSED(dev);
    return QStringLiteral("BEEPTEST\r\n");
}

QString packTestSolid(int colorIndex)
{
    return QStringLiteral("LEDTEST:%1\r\n").arg(colorIndex);
}

QString packTestAllOff()
{
    return QStringLiteral("LEDTEST:0\r\n");
}

QString packVoiceTest(const QString &text, int style)
{
    const QString hex = bytesToSpacedHex(toGb2312Bytes(text));
    return QStringLiteral("VOICETEST:%1,%2\r\n").arg(hex).arg(style);
}

static QVector<int> buildOrders(const QString& mode, int ledCount)
{
    QVector<int> orders;
    orders.resize(ledCount);
    if (mode == QStringLiteral("RAND"))
    {
        QVector<int> indices;
        indices.reserve(ledCount);
        for (int i = 1; i <= ledCount; ++i)
            indices.push_back(i);
        std::shuffle(indices.begin(), indices.end(), *QRandomGenerator::global());
        orders = indices;
    }
    else
    {
        for (int i = 0; i < ledCount; ++i)
            orders[i] = 0;
    }
    return orders;
}

QString packWork(const QVector<ActionItem> &actions, const DeviceProps &dev)
{
    QStringList parts;

    // assume actions already follow header order
    for (const auto& a : actions)
    {
        if (a.type == ActionType::L)
        {
            const int ledCount = dev.ledCount;
            QVector<int> colors = a.ledColors;
            if (colors.size() < ledCount)
                colors.resize(ledCount, 0);
            else if (colors.size() > ledCount)
                colors.resize(ledCount);

            const QVector<int> orders = buildOrders(a.ledMode, ledCount);
            QStringList ledFields;
            ledFields << QStringLiteral("LED")
                      << joinInts(orders)
                      << joinInts(colors);
            parts << ledFields.join(',');
        }
        else if (a.type == ActionType::D)
        {
            parts << QStringLiteral("DELAY,%1").arg(a.delayMs);
        }
        else if (a.type == ActionType::V)
        {
            const int style = (a.voiceSet == 2) ? 2 : 1;
            const QString hex = bytesToSpacedHex(toGb2312Bytes(a.voiceText));
            parts << QStringLiteral("VOICE,%1,%2").arg(hex).arg(style);
        }
        else if (a.type == ActionType::B)
        {
            parts << QStringLiteral("BEEP");
        }
    }

    return QStringLiteral("WORK:%1;\r\n").arg(parts.join(';'));
}

SetpRun parseSetpRun(const QString &line)
{
    SetpRun r;
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QStringLiteral("SETPRUN:")))
        return r;

    const QString body = trimmed.mid(QStringLiteral("SETPRUN:").length());
    const QStringList parts = body.split(',', Qt::KeepEmptyParts);
    if (parts.size() < 2)
        return r;

    bool ok1=false, ok2=false;
    const int step = parts[0].toInt(&ok1);
    const qint64 ts = parts[1].toLongLong(&ok2);
    if (ok1 && ok2)
    {
        r.ok = true;
        r.currentStep = step;
        r.startTimeMs = ts;
    }
    return r;
}

} // namespace Protocol
