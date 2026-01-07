#pragma once
/**
 * @file workflowengine.h
 * @brief Segment-level scheduler that builds WORK frames and pushes them via SerialService.
 */

#include <QObject>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>

#include "models.h"
#include "../config/appsettings.h"

class SerialService;

class WorkflowEngine : public QObject
{
    Q_OBJECT
public:
    explicit WorkflowEngine(QObject* parent = nullptr);

    void setSerialService(SerialService* s);

    void setDeviceProps(const DeviceProps& props) { m_device = props; }
    void setColors(const QVector<ColorItem>& colors) { m_colors = colors; }
    void setVoiceSets(const VoiceProps& v1, const VoiceProps& v2) { m_voice1 = v1; m_voice2 = v2; }

    void loadPlan(const QVector<ActionItem>& actions);

    bool hasPlan() const { return !m_actions.isEmpty(); }
    const QVector<ActionItem>& plan() const { return m_actions; }

    void beginRun(); // create log file + reset time base (per Start)

    bool runNextSegment();
    void resetRun();
    void markCurrentOrPreviousSegmentForRerun();

    void sendConfigs(); // LEDSET/VOICESET/BEEPSET
    void logTestTx(const QString& frame);

signals:
    void idle();
    void segmentStarted(const QString& segmentName, int startRow, int endRow);
    void actionStarted(int row, const QString& actionType, const QString& paramText);
    void actionFinished(int row, bool ok, int code, const QString& msg);
    void progressUpdated(int currentStep, qint64 deviceMs);
    void logLine(const QString& line);

public slots:
    void onSerialFrame(const QString& frame);

private:
    void rebuildSegments();
    int pickNextSegmentIndex() const;
    void writeLogLine(const QString& line);
    void logStructured(const QString& direction,
                       const QString& type,
                       int segmentIndex,
                       const QString& rawLine);
    qint64 nowDeviceMs() const;
    void startNewRunLog();

private:
    SerialService* m_serial = nullptr;
    DeviceProps m_device;
    QVector<ColorItem> m_colors;
    VoiceProps m_voice1;
    VoiceProps m_voice2;

    QVector<ActionItem> m_actions;
    QVector<Segment> m_segments;

    int m_currentSegmentIndex = -1;
    bool m_segmentRunning = false;
    int m_markedRerunSegment = -1;

    QFile m_logFile;
    QTextStream m_logStream;
    bool m_logReady = false;

    QElapsedTimer m_runTimer;
    bool m_haveDeviceBase = false;
    qint64 m_deviceBaseMs = 0;
    qint64 m_hostBaseElapsedMs = 0;
};
