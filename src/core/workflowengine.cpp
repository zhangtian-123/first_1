#include "workflowengine.h"

#include "../services/serialservice.h"
#include "protocol.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHash>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    #include <QStringConverter>
#endif

WorkflowEngine::WorkflowEngine(QObject* parent)
    : QObject(parent)
{
}

void WorkflowEngine::setSerialService(SerialService* s)
{
    m_serial = s;
}

void WorkflowEngine::beginRun()
{
    startNewRunLog();
}

void WorkflowEngine::loadPlan(const QVector<ActionItem>& actions)
{
    m_actions = actions;
    rebuildSegments();
    resetRun();
}

void WorkflowEngine::resetRun()
{
    m_currentSegmentIndex = -1;
    m_segmentRunning = false;
    m_markedRerunSegment = -1;
    emit idle();
}

void WorkflowEngine::rebuildSegments()
{
    m_segments.clear();
    if (m_actions.isEmpty())
        return;

    QString currentFlow = m_actions[0].flowName;
    int start = 0;
    QHash<QString, int> flowCount;

    auto flush = [&](int s, int e, const QString& flowName) {
        Segment seg;
        seg.startIndex = s;
        seg.endIndex = e;
        const int cnt = flowCount.value(flowName, 0) + 1;
        flowCount[flowName] = cnt;
        seg.name = QString("%1#%2").arg(flowName).arg(cnt);
        m_segments.push_back(seg);
    };

    for (int i = 1; i < m_actions.size(); ++i)
    {
        if (m_actions[i].flowName != currentFlow)
        {
            flush(start, i - 1, currentFlow);
            currentFlow = m_actions[i].flowName;
            start = i;
        }
    }
    flush(start, m_actions.size() - 1, currentFlow);
}

int WorkflowEngine::pickNextSegmentIndex() const
{
    if (m_segments.isEmpty())
        return -1;
    if (m_markedRerunSegment >= 0 && m_markedRerunSegment < m_segments.size())
        return m_markedRerunSegment;

    const int next = m_currentSegmentIndex + 1;
    if (next < 0) return 0;
    if (next >= m_segments.size()) return -1;
    return next;
}

bool WorkflowEngine::runNextSegment()
{
    if (m_actions.isEmpty() || m_segments.isEmpty())
        return false;
    if (!m_serial || !m_serial->isOpen())
    {
        logStructured(QStringLiteral("TX"), QStringLiteral("ERROR"), -1, QStringLiteral("Serial not open"));
        return false;
    }
    if (m_segmentRunning)
        return false;

    const int idx = pickNextSegmentIndex();
    if (idx < 0)
    {
        logStructured(QStringLiteral("TX"), QStringLiteral("WORK"), -1, QStringLiteral("No next segment"));
        emit idle();
        return false;
    }

    if (idx == m_markedRerunSegment)
        m_markedRerunSegment = -1;

    m_currentSegmentIndex = idx;
    const Segment seg = m_segments[idx];
    m_segmentRunning = true;

    emit segmentStarted(seg.name, seg.startIndex, seg.endIndex);

    QVector<ActionItem> segActions;
    segActions.reserve(seg.endIndex - seg.startIndex + 1);
    for (int i = seg.startIndex; i <= seg.endIndex; ++i)
        segActions.push_back(m_actions[i]);

    for (int i = seg.startIndex; i <= seg.endIndex; ++i)
        emit actionStarted(i, actionTypeToString(m_actions[i].type), m_actions[i].rawParamText);

    const QString frame = Protocol::packWork(segActions, m_device);
    logStructured(QStringLiteral("TX"), QStringLiteral("WORK"), idx, frame.trimmed());
    m_serial->sendFrame(frame);

    // No per-action ack in this protocol version.
    for (int i = seg.startIndex; i <= seg.endIndex; ++i)
        emit actionFinished(i, true, 0, QStringLiteral("OK"));

    m_segmentRunning = false;
    emit idle();
    return true;
}

void WorkflowEngine::markCurrentOrPreviousSegmentForRerun()
{
    int target = -1;
    if (m_segmentRunning)
    {
        if (m_currentSegmentIndex >= 0)
            target = m_currentSegmentIndex;
    }
    else if (m_currentSegmentIndex >= 0)
    {
        target = m_currentSegmentIndex;
    }

    if (target < 0 || target >= m_segments.size())
        return;
    m_markedRerunSegment = target;

    const Segment& seg = m_segments[target];
    if (seg.startIndex >= 0 && seg.startIndex < m_actions.size())
    {
        const QString flowName = m_actions[seg.startIndex].flowName;
        if (!flowName.isEmpty())
            emit rerunMarked(flowName);
    }
}

void WorkflowEngine::sendConfigs()
{
    if (!m_serial || !m_serial->isOpen())
        return;

    const QString ledCfg = Protocol::packLedConfig(m_device, m_colors);
    const QString voiceCfg1 = Protocol::packVoiceConfig1(m_voice1);
    const QString voiceCfg2 = Protocol::packVoiceConfig2(m_voice2);
    const QString beepCfg = Protocol::packBeepConfig(m_device);

    m_serial->sendFrame(ledCfg);
    logStructured(QStringLiteral("TX"), QStringLiteral("CONFIG"), -1, ledCfg.trimmed());

    m_serial->sendFrame(voiceCfg1);
    logStructured(QStringLiteral("TX"), QStringLiteral("CONFIG"), -1, voiceCfg1.trimmed());

    m_serial->sendFrame(voiceCfg2);
    logStructured(QStringLiteral("TX"), QStringLiteral("CONFIG"), -1, voiceCfg2.trimmed());

    m_serial->sendFrame(beepCfg);
    logStructured(QStringLiteral("TX"), QStringLiteral("CONFIG"), -1, beepCfg.trimmed());
}

void WorkflowEngine::logTestTx(const QString& frame)
{
    if (!m_logReady)
        return;
    logStructured(QStringLiteral("TX"), QStringLiteral("TEST"), -1, frame.trimmed());
}

void WorkflowEngine::onSerialFrame(const QString& frame)
{
    logStructured(QStringLiteral("RX"), QStringLiteral("WORK"), m_currentSegmentIndex, frame.trimmed());

    const auto pr = Protocol::parseSetpRun(frame);
    if (pr.ok)
    {
        if (!m_haveDeviceBase)
        {
            m_haveDeviceBase = true;
            m_deviceBaseMs = pr.startTimeMs;
            m_hostBaseElapsedMs = m_runTimer.isValid() ? m_runTimer.elapsed() : 0;
        }
        emit progressUpdated(pr.currentStep, pr.startTimeMs);
    }
}

void WorkflowEngine::startNewRunLog()
{
    if (m_logFile.isOpen())
        m_logFile.close();

    m_runTimer.restart();
    m_haveDeviceBase = false;
    m_deviceBaseMs = 0;
    m_hostBaseElapsedMs = 0;

    const QString baseDir = QCoreApplication::applicationDirPath();
    QDir d(baseDir);
    if (!d.exists("logs"))
        d.mkpath("logs");

    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString filePath = d.filePath(QString("logs/%1.log").arg(ts));

    m_logFile.setFileName(filePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_logReady = false;
        emit logLine(QStringLiteral("日志文件创建失败：%1").arg(filePath));
        return;
    }

    m_logStream.setDevice(&m_logFile);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    m_logStream.setEncoding(QStringConverter::Utf8);
#else
    m_logStream.setCodec("UTF-8");
#endif

    m_logReady = true;
}

qint64 WorkflowEngine::nowDeviceMs() const
{
    if (!m_haveDeviceBase)
        return -1;
    const qint64 nowElapsed = m_runTimer.isValid() ? m_runTimer.elapsed() : 0;
    return m_deviceBaseMs + (nowElapsed - m_hostBaseElapsedMs);
}

void WorkflowEngine::logStructured(const QString& direction,
                                  const QString& type,
                                  int segmentIndex,
                                  const QString& rawLine)
{
    const qint64 devMs = nowDeviceMs();
    const QString line = QStringLiteral("[%1] [%2] [%3] [%4] [%5]")
                             .arg(devMs)
                             .arg(direction)
                             .arg(type)
                             .arg(segmentIndex)
                             .arg(rawLine);
    writeLogLine(line);
}

void WorkflowEngine::writeLogLine(const QString& line)
{
    emit logLine(line);

    if (m_logReady && m_logFile.isOpen())
    {
        m_logStream << line << "\n";
        m_logStream.flush();
    }
}
