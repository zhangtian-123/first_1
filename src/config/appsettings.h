#pragma once
/**
 * @file appsettings.h
 * @brief 本地配置（QSettings/ini）数据结构与读写接口声明
 *
 * ✅ 需求对齐：
 * 1) Excel 只负责流程逻辑；以下内容全部走本地配置（QSettings/ini）：
 *    - 颜色表（初始为空，用户手动添加，可增加到 8/9/10...）
 *    - 冲突表（固定三元组/行）
 *    - 设备属性（点亮时长/点亮间隔/LED数，LED数以设置页为准）
 *    - 快捷键（仅窗口获得焦点时生效，QShortcut）
 *    - 串口参数（便于下次打开记住）
 *    - 最近一次 Excel 路径
 *
 * 2) “保存”与“清空”均会写入 ini（清空=覆盖保存）
 *
 * 💡 ini 建议：
 * - 使用应用程序目录下的 ./config.ini（便于拷贝部署，Windows-only）
 * - 具体实现放在 appsettings.cpp
 */

#include <QString>
#include <QStringList>
#include <QVector>
#include <QColor>
#include <QKeySequence>

/**
 * @brief 串口配置（UI 选择的值）
 * 说明：parity 使用文本 "None"/"Even"/"Odd"，具体映射在 SerialService 内处理
 */
struct SerialConfig
{
    QString portName;          ///< 例如 "COM3"
    int baud = 115200;         ///< 波特率
    int dataBits = 8;          ///< 7/8
    QString parity = "None";   ///< "None"/"Even"/"Odd"
    int stopBits = 1;          ///< 1/2
    bool autoSetup = false;    ///< 自动设置（截图里有）
};

/**
 * @brief 设备属性（单位 ms）
 * - onMs     : 点亮时长（对 ALL/SEQ/RAND 都生效）
 * - gapMs    : 点亮间隔（对 SEQ/RAND 生效）
 * - ledCount : LED 个数（以设置页为准）
 */
struct DeviceProps
{
    int onMs = 350;
    int gapMs = 0;
    int ledCount = 5;
    int brightness = 100;      ///< 0-255
    int buzzerFreq = 1500;     ///< Hz
    int buzzerDurMs = 500;     ///< ms
};

/**
 * @brief 语音参数（VOICESET1/VOICESET2）
 */
struct VoiceProps
{
    int announcer = 0;     ///< 0-10
    int voiceStyle = 2;    ///< 0-2
    int voiceSpeed = 5;    ///< 0-10
    int voicePitch = 5;    ///< 0-10
    int voiceVolume = 5;   ///< 0-10
};

/**
 * @brief 冲突三元组：固定 3 个颜色编号
 * ✅ 规则：
 * - 同一次 L 动作生成的颜色集合里，不允许同时出现同组内任意两个颜色
 * - 若随机不可解：点击“开始”时预检弹窗警告并阻止开始
 */
struct ConflictTriple
{
    int c1 = 0;
    int c2 = 0;
    int c3 = 0;
};

/**
 * @brief 快捷键配置（仅窗口获得焦点时生效）
 * - keyNext：顺序执行（等价“下一步”）
 * - keyRerun：标记“此段/上段”需重做
 * - keyQuickColor：颜色1~7常亮（测试功能：点击开始后不生效）
 * - keyAllOff：全灭（测试功能：点击开始后不生效）
 */
struct HotkeyConfig
{
    QKeySequence keyNext;
    QKeySequence keyRerun;

    QVector<QKeySequence> keyQuickColor; ///< size=7（颜色1..7）
    QKeySequence keyAllOff;
};

/**
 * @brief 颜色表项（编号从 1 开始递增）
 * UI 文本显示要求：R:102,G:255,B:85
 */
struct ColorItem
{
    int index = 0;     ///< 1..N
    QColor rgb;        ///< RGB
};

/**
 * @brief 设置总数据
 */
struct SettingsData
{
    QString lastExcelPath;          ///< 最近一次 Excel 路径

    SerialConfig serial;
    DeviceProps device;
    VoiceProps voice1;
    VoiceProps voice2;
    HotkeyConfig hotkeys;

    QVector<ColorItem> colors;           ///< 颜色表（可为空）
    QVector<ConflictTriple> conflicts;   ///< 冲突表（可为空）
};

/**
 * @brief AppSettings：QSettings/ini 的读写封装
 *
 * 注意：
 * - 这里仅声明接口；实现写在 appsettings.cpp
 * - 将 ini 放在程序目录，便于部署（Windows-only）
 */
class AppSettings
{
public:
    // ---- 全量读写 ----
    static SettingsData load();
    static void save(const SettingsData& data);

    // ---- 分块保存（便于按钮“保存/清空”直接落盘） ----
    static void saveColors(const QVector<ColorItem>& colors);
    static void saveConflicts(const QVector<ConflictTriple>& conflicts);
    static void saveDevice(const DeviceProps& device);
    static void saveVoiceSets(const VoiceProps& voice1, const VoiceProps& voice2);
    static void saveHotkeys(const HotkeyConfig& hotkeys);
    static void saveSerial(const SerialConfig& serial);
    static void saveLastExcelPath(const QString& path);

public:
    // ---- 小工具 ----
    /**
     * @brief QColor -> "FF00FF" (6-digit HEX, no '#')
     */
    static QString colorToText(const QColor& c);

    static QColor makeColor(int r, int g, int b);
};
