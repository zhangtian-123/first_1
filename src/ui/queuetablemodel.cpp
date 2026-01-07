#include "queuetablemodel.h"

#include <algorithm>

QueueTableModel::QueueTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void QueueTableModel::clear()
{
    beginResetModel();
    m_headerCols.clear();
    m_actions.clear();
    m_rows.clear();
    m_flowRow.clear();
    m_maxLedIdx = 0;
    endResetModel();
}

void QueueTableModel::setHeaderColumns(const QVector<HeaderCol>& headerCols)
{
    beginResetModel();
    m_headerCols = headerCols;
    m_maxLedIdx = 0;
    for (const auto& hc : m_headerCols)
    {
        if (hc.field == HeaderField::LED)
            m_maxLedIdx = std::max(m_maxLedIdx, hc.ledIndex);
    }
    rebuildRows();
    endResetModel();
}

void QueueTableModel::setActions(const QVector<ActionItem>& actions)
{
    beginResetModel();
    m_actions = actions;
    rebuildRows();
    endResetModel();
}

int QueueTableModel::rowForFlowName(const QString& flowName) const
{
    return m_flowRow.value(flowName, -1);
}

void QueueTableModel::clearStatuses()
{
    if (m_rows.isEmpty())
        return;

    for (auto& r : m_rows)
    {
        r.statusText.clear();
        r.statusBg = QColor();
        r.hasStatusBg = false;
    }
    const int statusCol = columnCount() - 1;
    emit dataChanged(index(0, statusCol), index(rowCount() - 1, statusCol));
}

void QueueTableModel::setStatusForFlowName(const QString& flowName, const QString& text, const QColor& bg)
{
    const int row = rowForFlowName(flowName);
    if (row < 0 || row >= m_rows.size())
        return;

    const int statusCol = columnCount() - 1;
    m_rows[row].statusText = text;
    m_rows[row].statusBg = bg;
    m_rows[row].hasStatusBg = bg.isValid();
    emit dataChanged(index(row, statusCol), index(row, statusCol));
}

int QueueTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int QueueTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 1 + m_headerCols.size() + 1;
}

QVariant QueueTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    if (index.row() < 0 || index.row() >= m_rows.size())
        return {};
    if (index.column() < 0 || index.column() >= columnCount())
        return {};

    const SegmentRow& r = m_rows[index.row()];
    const int col = index.column();
    const int statusCol = columnCount() - 1;

    if (role == Qt::BackgroundRole && col == statusCol && r.hasStatusBg)
        return r.statusBg;

    if (role != Qt::DisplayRole)
        return {};

    if (col == 0)
        return r.flow;

    if (col == statusCol)
        return r.statusText;

    const int hcIdx = col - 1;
    if (hcIdx < 0 || hcIdx >= m_headerCols.size())
        return {};

    const HeaderCol& hc = m_headerCols[hcIdx];
    switch (hc.field)
    {
    case HeaderField::Mode:
        return r.mode;
    case HeaderField::LED:
    {
        const int ledIdx = hc.ledIndex;
        if (ledIdx <= 0 || ledIdx > r.leds.size())
            return {};
        return r.leds[ledIdx - 1];
    }
    case HeaderField::Beep:
        if (!r.hasBeep) return {};
        return r.beepDefault ? QStringLiteral("默认") : QString::number(r.beepDurMs);
    case HeaderField::Voice:
        return r.hasVoice ? r.voice : QString();
    case HeaderField::Delay:
        return r.hasDelay ? QString::number(r.delayMs) : QString();
    case HeaderField::VoiceSet:
        return r.hasVoiceSet ? QString::number(r.voiceSet) : QString();
    }
    return {};
}

QVariant QueueTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    const int statusCol = columnCount() - 1;
    if (section == 0)
        return QStringLiteral("流程");
    if (section == statusCol)
        return QStringLiteral("状态");

    const int hcIdx = section - 1;
    if (hcIdx < 0 || hcIdx >= m_headerCols.size())
        return {};

    const HeaderCol& hc = m_headerCols[hcIdx];
    switch (hc.field)
    {
    case HeaderField::Mode:  return QStringLiteral("工作模式");
    case HeaderField::LED:   return QStringLiteral("LED%1").arg(hc.ledIndex);
    case HeaderField::Beep:  return QStringLiteral("BEEP");
    case HeaderField::Voice: return QStringLiteral("VOICE");
    case HeaderField::Delay: return QStringLiteral("DELAY(ms)");
    case HeaderField::VoiceSet: return QStringLiteral("风格");
    }
    return {};
}

Qt::ItemFlags QueueTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void QueueTableModel::rebuildRows()
{
    m_rows.clear();
    m_flowRow.clear();

    if (m_actions.isEmpty())
        return;

    auto ensureRow = [&](const QString& flow)->int {
        const auto it = m_flowRow.constFind(flow);
        if (it != m_flowRow.constEnd())
            return it.value();

        SegmentRow r;
        r.flow = flow;
        r.leds.resize(m_maxLedIdx, 0);
        const int idx = m_rows.size();
        m_rows.push_back(r);
        m_flowRow.insert(flow, idx);
        return idx;
    };

    for (const auto& a : m_actions)
    {
        const int idx = ensureRow(a.flowName);
        SegmentRow& r = m_rows[idx];
        if (!r.hasVoiceSet)
        {
            r.voiceSet = a.voiceSet;
            r.hasVoiceSet = true;
        }
        switch (a.type)
        {
        case ActionType::L:
            r.mode = a.ledMode;
            for (int i = 0; i < r.leds.size() && i < a.ledColors.size(); ++i)
                r.leds[i] = a.ledColors[i];
            break;
        case ActionType::B:
            r.hasBeep = true;
            r.beepDefault = (a.beepDurMs == 0);
            r.beepDurMs = a.beepDurMs;
            break;
        case ActionType::V:
            r.hasVoice = true;
            r.voice = a.voiceText;
            break;
        case ActionType::D:
            r.hasDelay = true;
            r.delayMs = a.delayMs;
            break;
        default:
            break;
        }
    }
}
