#pragma once
/**
 * @file protocol.h
 * @brief Serial protocol helpers (CRLF framed).
 *
 * Formats per product spec:
 * - LEDSET:<ledCount>,<onMs>,<intervalMs>,<brightness>,<colorCount>,<hex1>,...<hexN>\r\n
 * - VOICESET1:<announcer>,<style>,<speed>,<pitch>,<volume>\r\n
 * - VOICESET2:<announcer>,<style>,<speed>,<pitch>,<volume>\r\n
 * - BEEPSET;<durationMs>,<freqHz>\r\n (params separated by ',')
 * - WORK:<action1>;<action2>;...\r\n where actions follow Excel header order:
 *   * LED action: LED,<order1..N>,<color1..N> (order zeros for ALL/SEQ; shuffled for RAND)
 *   * DELAY action: DELAY,<ms>
 *   * VOICE action: VOICE,<gb2312_hex_bytes_with_spaces>,<style>
 *   * BEEP action: BEEP
 * - RX progress: SETPRUN:<currentStep>,<startTimeMs>\r\n
 * - VOICETEST:<gb2312_hex_bytes_with_spaces>,<style>\r\n
 */

#include <QString>
#include <QStringList>
#include <QVector>

#include "../config/appsettings.h"
#include "models.h"

namespace Protocol
{
    QString escapeVoiceText(const QString& text);

    QString packLedConfig(const DeviceProps& dev, const QVector<ColorItem>& colors);
    QString packVoiceConfig1(const VoiceProps& v);
    QString packVoiceConfig2(const VoiceProps& v);
    QString packBeepConfig(const DeviceProps& dev);
    QString packBeepTest(const DeviceProps& dev);

    QString packWork(const QVector<ActionItem>& actions, const DeviceProps& dev);
    QString packTestSolid(int colorIndex);
    QString packTestAllOff();
    QString packVoiceTest(const QString& text, int style);

    struct SetpRun
    {
        bool ok = false;
        int currentStep = -1;
        qint64 startTimeMs = 0;
    };
    SetpRun parseSetpRun(const QString& line);
}
