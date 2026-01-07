/**
 * @file appsettings.cpp
 * @brief AppSettings 实现：使用 QSettings(ini) 持久化保存
 *
 * ✅ 需求对齐：
 * - 颜色表/冲突表/设备属性/快捷键/串口参数/最近 Excel 路径：全部存本地 ini
 * - “保存”与“清空”都会覆盖写入 ini
 * - Windows-only 部署友好：默认把 ini 放在程序目录 ./config.ini
 */

#include "appsettings.h"

#include <algorithm>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

// ==============================
// ini 路径：程序目录 ./config.ini
// ==============================
static QString iniPath()
{
    const QString dir = QCoreApplication::applicationDirPath();
    return QDir(dir).filePath("config.ini");
}

static QSettings makeSettings()
{
    // IniFormat：便于直接查看/拷贝/版本控制
    return QSettings(iniPath(), QSettings::IniFormat);
}

// ==============================
// Key 定义（集中管理，避免手写字符串错误）
// ==============================
namespace Keys
{
    // app
    static const char* kLastExcelPath = "app/lastExcelPath";

    // serial
    static const char* kSerialPort    = "serial/portName";
    static const char* kSerialBaud    = "serial/baud";
    static const char* kSerialDataBits= "serial/dataBits";
    static const char* kSerialParity  = "serial/parity";
    static const char* kSerialStopBits= "serial/stopBits";
    static const char* kSerialAuto    = "serial/autoSetup";

    // device
    static const char* kDevOnMs       = "device/onMs";
    static const char* kDevGapMs      = "device/gapMs";
    static const char* kDevLedCount   = "device/ledCount";
    static const char* kDevBrightness = "device/brightness";
    static const char* kDevBuzzFreq   = "device/buzzFreq";
    static const char* kDevBuzzDur    = "device/buzzDur";
    // voice1
    static const char* kVoice1Announcer  = "voice1/announcer";
    static const char* kVoice1Style      = "voice1/style";
    static const char* kVoice1Speed      = "voice1/speed";
    static const char* kVoice1Pitch      = "voice1/pitch";
    static const char* kVoice1Volume     = "voice1/volume";

    // voice2
    static const char* kVoice2Announcer  = "voice2/announcer";
    static const char* kVoice2Style      = "voice2/style";
    static const char* kVoice2Speed      = "voice2/speed";
    static const char* kVoice2Pitch      = "voice2/pitch";
    static const char* kVoice2Volume     = "voice2/volume";

    // hotkeys
    static const char* kHotNext       = "hotkeys/next";
    static const char* kHotRerun      = "hotkeys/rerun";
    static const char* kHotAllOff     = "hotkeys/allOff";
    // quickColor 使用数组：hotkeys/quickColor/size + hotkeys/quickColor/i
    static const char* kHotQuickSize  = "hotkeys/quickColor/size";
    static const char* kHotQuickItem  = "hotkeys/quickColor/%1";

    // colors 数组
    static const char* kColorsArray   = "colors/items";

    // conflicts 数组
    static const char* kConfArray     = "conflicts/triples";
}

// ==============================
// 小工具：确保 quickColor 至少有 7 项
// ==============================
static void ensureQuickColorSize(HotkeyConfig& hk)
{
    if (hk.keyQuickColor.size() < 7)
    {
        const int need = 7 - hk.keyQuickColor.size();
        for (int i = 0; i < need; ++i) hk.keyQuickColor.push_back(QKeySequence());
    }
    else if (hk.keyQuickColor.size() > 7)
    {
        hk.keyQuickColor.resize(7);
    }
}

// ==============================
// 全量读取
// ==============================
SettingsData AppSettings::load()
{
    QSettings s = makeSettings();
    SettingsData d;

    // app
    d.lastExcelPath = s.value(Keys::kLastExcelPath, "").toString();

    // serial
    d.serial.portName  = s.value(Keys::kSerialPort, "").toString();
    d.serial.baud      = s.value(Keys::kSerialBaud, 115200).toInt();
    d.serial.dataBits  = s.value(Keys::kSerialDataBits, 8).toInt();
    d.serial.parity    = s.value(Keys::kSerialParity, "None").toString();
    d.serial.stopBits  = s.value(Keys::kSerialStopBits, 1).toInt();
    d.serial.autoSetup = s.value(Keys::kSerialAuto, false).toBool();

    // device
    d.device.onMs     = s.value(Keys::kDevOnMs, 350).toInt();
    d.device.gapMs    = s.value(Keys::kDevGapMs, 0).toInt();
    d.device.ledCount = s.value(Keys::kDevLedCount, 5).toInt();
    d.device.brightness = s.value(Keys::kDevBrightness, 100).toInt();
    d.device.buzzerFreq = s.value(Keys::kDevBuzzFreq, 1500).toInt();
    d.device.buzzerDurMs= s.value(Keys::kDevBuzzDur, 500).toInt();

    d.voice1.announcer   = s.value(Keys::kVoice1Announcer, 0).toInt();
    d.voice1.voiceStyle  = s.value(Keys::kVoice1Style, 1).toInt();
    d.voice1.voiceSpeed  = s.value(Keys::kVoice1Speed, 5).toInt();
    d.voice1.voicePitch  = s.value(Keys::kVoice1Pitch, 5).toInt();
    d.voice1.voiceVolume = s.value(Keys::kVoice1Volume, 5).toInt();

    d.voice2.announcer   = s.value(Keys::kVoice2Announcer, 0).toInt();
    d.voice2.voiceStyle  = s.value(Keys::kVoice2Style, 1).toInt();
    d.voice2.voiceSpeed  = s.value(Keys::kVoice2Speed, 5).toInt();
    d.voice2.voicePitch  = s.value(Keys::kVoice2Pitch, 5).toInt();
    d.voice2.voiceVolume = s.value(Keys::kVoice2Volume, 5).toInt();

    // hotkeys
    d.hotkeys.keyNext  = QKeySequence(s.value(Keys::kHotNext, "").toString());
    d.hotkeys.keyRerun = QKeySequence(s.value(Keys::kHotRerun, "").toString());
    d.hotkeys.keyAllOff= QKeySequence(s.value(Keys::kHotAllOff, "").toString());

    // quick colors
    const int qs = s.value(Keys::kHotQuickSize, 0).toInt();
    d.hotkeys.keyQuickColor.clear();
    for (int i = 0; i < qs; ++i)
    {
        const QString k = QString(Keys::kHotQuickItem).arg(i);
        d.hotkeys.keyQuickColor.push_back(QKeySequence(s.value(k, "").toString()));
    }
    ensureQuickColorSize(d.hotkeys);

    // colors array
    d.colors.clear();
    const int colorCount = s.beginReadArray(Keys::kColorsArray);
    for (int i = 0; i < colorCount; ++i)
    {
        s.setArrayIndex(i);
        ColorItem item;
        item.index = s.value("index", i + 1).toInt();
        const int r = s.value("r", 255).toInt();
        const int g = s.value("g", 255).toInt();
        const int b = s.value("b", 255).toInt();
        item.rgb = QColor(r, g, b);
        d.colors.push_back(item);
    }
    s.endArray();

    // conflicts array
    d.conflicts.clear();
    const int confCount = s.beginReadArray(Keys::kConfArray);
    for (int i = 0; i < confCount; ++i)
    {
        s.setArrayIndex(i);
        ConflictTriple t;
        t.c1 = s.value("c1", 0).toInt();
        t.c2 = s.value("c2", 0).toInt();
        t.c3 = s.value("c3", 0).toInt();
        d.conflicts.push_back(t);
    }
    s.endArray();

    // Normalize: colors are 1..N sequential and conflicts must refer to existing indices.
    std::sort(d.colors.begin(), d.colors.end(), [](const ColorItem& a, const ColorItem& b){
        return a.index < b.index;
    });
    if (d.colors.size() > 100)
        d.colors.resize(100);
    for (int i = 0; i < d.colors.size(); ++i)
        d.colors[i].index = i + 1;

    const int maxIdx = d.colors.size();
    for (auto& t : d.conflicts)
    {
        auto clamp = [&](int& v){
            if (v < 0 || v > maxIdx) v = 0;
        };
        clamp(t.c1); clamp(t.c2); clamp(t.c3);
    }

    return d;
}

// ==============================
// 全量保存（覆盖）
// ==============================
void AppSettings::save(const SettingsData &data)
{
    QSettings s = makeSettings();

    // app
    s.setValue(Keys::kLastExcelPath, data.lastExcelPath);

    // serial
    s.setValue(Keys::kSerialPort, data.serial.portName);
    s.setValue(Keys::kSerialBaud, data.serial.baud);
    s.setValue(Keys::kSerialDataBits, data.serial.dataBits);
    s.setValue(Keys::kSerialParity, data.serial.parity);
    s.setValue(Keys::kSerialStopBits, data.serial.stopBits);
    s.setValue(Keys::kSerialAuto, data.serial.autoSetup);

    // device
    s.setValue(Keys::kDevOnMs, data.device.onMs);
    s.setValue(Keys::kDevGapMs, data.device.gapMs);
    s.setValue(Keys::kDevLedCount, data.device.ledCount);
    s.setValue(Keys::kDevBrightness, data.device.brightness);
    s.setValue(Keys::kDevBuzzFreq, data.device.buzzerFreq);
    s.setValue(Keys::kDevBuzzDur, data.device.buzzerDurMs);

    s.setValue(Keys::kVoice1Announcer, data.voice1.announcer);
    s.setValue(Keys::kVoice1Style, data.voice1.voiceStyle);
    s.setValue(Keys::kVoice1Speed, data.voice1.voiceSpeed);
    s.setValue(Keys::kVoice1Pitch, data.voice1.voicePitch);
    s.setValue(Keys::kVoice1Volume, data.voice1.voiceVolume);

    s.setValue(Keys::kVoice2Announcer, data.voice2.announcer);
    s.setValue(Keys::kVoice2Style, data.voice2.voiceStyle);
    s.setValue(Keys::kVoice2Speed, data.voice2.voiceSpeed);
    s.setValue(Keys::kVoice2Pitch, data.voice2.voicePitch);
    s.setValue(Keys::kVoice2Volume, data.voice2.voiceVolume);

    // hotkeys
    HotkeyConfig hk = data.hotkeys;
    ensureQuickColorSize(hk);

    s.setValue(Keys::kHotNext, hk.keyNext.toString());
    s.setValue(Keys::kHotRerun, hk.keyRerun.toString());
    s.setValue(Keys::kHotAllOff, hk.keyAllOff.toString());

    s.setValue(Keys::kHotQuickSize, hk.keyQuickColor.size());
    for (int i = 0; i < hk.keyQuickColor.size(); ++i)
    {
        const QString k = QString(Keys::kHotQuickItem).arg(i);
        s.setValue(k, hk.keyQuickColor[i].toString());
    }

    // colors
    s.beginWriteArray(Keys::kColorsArray);
    for (int i = 0; i < data.colors.size(); ++i)
    {
        s.setArrayIndex(i);
        const auto& c = data.colors[i];
        s.setValue("index", c.index);
        s.setValue("r", c.rgb.red());
        s.setValue("g", c.rgb.green());
        s.setValue("b", c.rgb.blue());
    }
    s.endArray();

    // conflicts
    s.beginWriteArray(Keys::kConfArray);
    for (int i = 0; i < data.conflicts.size(); ++i)
    {
        s.setArrayIndex(i);
        const auto& t = data.conflicts[i];
        s.setValue("c1", t.c1);
        s.setValue("c2", t.c2);
        s.setValue("c3", t.c3);
    }
    s.endArray();

    s.sync(); // 立即落盘
}

// ==============================
// 分块保存（按钮用）
// ==============================
void AppSettings::saveColors(const QVector<ColorItem> &colors)
{
    QSettings s = makeSettings();

    s.remove("colors"); // 覆盖保存（清空旧内容）
    s.beginWriteArray(Keys::kColorsArray);
    for (int i = 0; i < colors.size(); ++i)
    {
        s.setArrayIndex(i);
        const auto& c = colors[i];
        s.setValue("index", c.index);
        s.setValue("r", c.rgb.red());
        s.setValue("g", c.rgb.green());
        s.setValue("b", c.rgb.blue());
    }
    s.endArray();
    s.sync();
}

void AppSettings::saveConflicts(const QVector<ConflictTriple> &conflicts)
{
    QSettings s = makeSettings();

    s.remove("conflicts"); // 覆盖保存
    s.beginWriteArray(Keys::kConfArray);
    for (int i = 0; i < conflicts.size(); ++i)
    {
        s.setArrayIndex(i);
        const auto& t = conflicts[i];
        s.setValue("c1", t.c1);
        s.setValue("c2", t.c2);
        s.setValue("c3", t.c3);
    }
    s.endArray();
    s.sync();
}

void AppSettings::saveDevice(const DeviceProps &device)
{
    QSettings s = makeSettings();

    s.setValue(Keys::kDevOnMs, device.onMs);
    s.setValue(Keys::kDevGapMs, device.gapMs);
    s.setValue(Keys::kDevLedCount, device.ledCount);
    s.setValue(Keys::kDevBrightness, device.brightness);
    s.setValue(Keys::kDevBuzzFreq, device.buzzerFreq);
    s.setValue(Keys::kDevBuzzDur, device.buzzerDurMs);
    s.sync();
}

void AppSettings::saveVoiceSets(const VoiceProps &voice1, const VoiceProps &voice2)
{
    QSettings s = makeSettings();

    s.setValue(Keys::kVoice1Announcer, voice1.announcer);
    s.setValue(Keys::kVoice1Style, voice1.voiceStyle);
    s.setValue(Keys::kVoice1Speed, voice1.voiceSpeed);
    s.setValue(Keys::kVoice1Pitch, voice1.voicePitch);
    s.setValue(Keys::kVoice1Volume, voice1.voiceVolume);

    s.setValue(Keys::kVoice2Announcer, voice2.announcer);
    s.setValue(Keys::kVoice2Style, voice2.voiceStyle);
    s.setValue(Keys::kVoice2Speed, voice2.voiceSpeed);
    s.setValue(Keys::kVoice2Pitch, voice2.voicePitch);
    s.setValue(Keys::kVoice2Volume, voice2.voiceVolume);

    s.sync();
}

void AppSettings::saveHotkeys(const HotkeyConfig &hotkeys)
{
    QSettings s = makeSettings();

    HotkeyConfig hk = hotkeys;
    ensureQuickColorSize(hk);

    s.setValue(Keys::kHotNext, hk.keyNext.toString());
    s.setValue(Keys::kHotRerun, hk.keyRerun.toString());
    s.setValue(Keys::kHotAllOff, hk.keyAllOff.toString());

    s.setValue(Keys::kHotQuickSize, hk.keyQuickColor.size());
    for (int i = 0; i < hk.keyQuickColor.size(); ++i)
    {
        const QString k = QString(Keys::kHotQuickItem).arg(i);
        s.setValue(k, hk.keyQuickColor[i].toString());
    }

    s.sync();
}

void AppSettings::saveSerial(const SerialConfig &serial)
{
    QSettings s = makeSettings();

    s.setValue(Keys::kSerialPort, serial.portName);
    s.setValue(Keys::kSerialBaud, serial.baud);
    s.setValue(Keys::kSerialDataBits, serial.dataBits);
    s.setValue(Keys::kSerialParity, serial.parity);
    s.setValue(Keys::kSerialStopBits, serial.stopBits);
    s.setValue(Keys::kSerialAuto, serial.autoSetup);

    s.sync();
}

void AppSettings::saveLastExcelPath(const QString &path)
{
    QSettings s = makeSettings();
    s.setValue(Keys::kLastExcelPath, path);
    s.sync();
}

// ==============================
// 小工具
// ==============================
QString AppSettings::colorToText(const QColor &c)
{
    return QString("%1%2%3")
        .arg(c.red(),   2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(),  2, 16, QChar('0'))
        .toUpper();
}

QColor AppSettings::makeColor(int r, int g, int b)
{
    return QColor(r, g, b);
}
