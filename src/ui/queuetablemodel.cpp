#include "queuetablemodel.h"

#include <algorithm>
#include <QString>

namespace
{
QString excelColumnName(int col)
{
    if (col <= 0)
        return QString();

    QString name;
    int n = col;
    while (n > 0)
    {
        const int rem = (n - 1) % 26;
        name.prepend(QChar('A' + rem));
        n = (n - 1) / 26;
    }
    return name;
}

const QColor kRunningColor(200, 255, 200);
const QColor kDoneColor(220, 220, 220);
const QColor kRerunColor(255, 150, 150);
}

QueueTableModel::QueueTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void QueueTableModel::clear()
{
    beginResetModel();
    m_actions.clear();
    m_rows.clear();
    m_flowRow.clear();
    m_tableColumnStart = 1;
    m_tableColumnCount = 0;
    endResetModel();
}

void QueueTableModel::setTableRows(const QVector<ExcelTableRow>& rows, int columnStart, int columnCount)
{
    beginResetModel();
    m_rows.clear();
    m_flowRow.clear();

    m_tableColumnStart = (columnStart > 0) ? columnStart : 1;
    m_tableColumnCount = std::max(0, columnCount);

    m_rows.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        const auto& src = rows[i];
        DisplayRow r;
        r.isHeader = src.isHeader;
        r.flow = src.flowName;
        r.cells = src.cells;
        r.ledColumns = src.ledColumns;
        r.timeColumns = src.timeColumns;
        r.flowState = FlowState::None;
        r.rerunMarked = false;
        r.timeStates.fill(StepState::None, r.timeColumns.size());
        m_rows.push_back(r);
        if (!r.isHeader && !r.flow.isEmpty())
            m_flowRow.insert(r.flow, i);
    }
    endResetModel();
}

void QueueTableModel::setActions(const QVector<ActionItem>& actions)
{
    beginResetModel();
    m_actions = actions;

    QHash<QString, QVector<int>> ledByFlow;
    for (const auto& a : m_actions)
    {
        if (a.type != ActionType::L)
            continue;
        if (!ledByFlow.contains(a.flowName))
            ledByFlow.insert(a.flowName, a.ledColors);
    }

    for (auto& r : m_rows)
    {
        if (r.isHeader)
            continue;
        const auto it = ledByFlow.constFind(r.flow);
        if (it == ledByFlow.constEnd())
            continue;
        const QVector<int>& colors = it.value();
        for (int i = 0; i < r.ledColumns.size() && i < colors.size(); ++i)
        {
            const int cellIdx = r.ledColumns[i];
            if (cellIdx < 0 || cellIdx >= r.cells.size())
                continue;
            r.cells[cellIdx] = QString::number(colors[i]);
        }
    }

    endResetModel();
}

int QueueTableModel::rowForFlowName(const QString& flowName) const
{
    return m_flowRow.value(flowName, -1);
}

void QueueTableModel::clearFlowStates()
{
    if (m_rows.isEmpty())
        return;

    for (auto& r : m_rows)
    {
        r.flowState = FlowState::None;
        r.rerunMarked = false;
    }

    emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

void QueueTableModel::setFlowRunning(const QString& flowName)
{
    setFlowState(flowName, FlowState::Running);
}

void QueueTableModel::setFlowDone(const QString& flowName)
{
    setFlowState(flowName, FlowState::Done);
}

void QueueTableModel::setFlowRerunMarked(const QString& flowName)
{
    const int rowIdx = rowForFlowName(flowName);
    if (rowIdx < 0 || rowIdx >= m_rows.size())
        return;
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].rerunMarked)
        {
            m_rows[i].rerunMarked = false;
            emit dataChanged(index(i, 0), index(i, 0));
        }
    }

    auto& row = m_rows[rowIdx];
    row.rerunMarked = true;
    emit dataChanged(index(rowIdx, 0), index(rowIdx, 0));
}

void QueueTableModel::clearStepTimes()
{
    if (m_rows.isEmpty())
        return;

    for (int r = 0; r < m_rows.size(); ++r)
    {
        auto& row = m_rows[r];
        if (row.isHeader)
            continue;
        for (int i = 0; i < row.timeColumns.size(); ++i)
        {
            const int colIdx = row.timeColumns[i];
            if (colIdx < 0)
                continue;
            if (colIdx < row.cells.size())
                row.cells[colIdx].clear();
            if (i < row.timeStates.size())
                row.timeStates[i] = StepState::None;
            emitTimeCellChanged(r, colIdx);
        }
    }
}

void QueueTableModel::setStepRunning(const QString& flowName, int stepIndex)
{
    if (stepIndex <= 0)
        return;
    const int rowIdx = rowForFlowName(flowName);
    if (rowIdx < 0 || rowIdx >= m_rows.size())
        return;

    auto& row = m_rows[rowIdx];
    for (int i = 0; i < row.timeStates.size(); ++i)
    {
        if (row.timeStates[i] == StepState::Running)
        {
            row.timeStates[i] = StepState::None;
            const int colIdx = row.timeColumns.value(i, -1);
            emitTimeCellChanged(rowIdx, colIdx);
        }
    }

    const int stepZero = stepIndex - 1;
    if (stepZero < 0 || stepZero >= row.timeStates.size())
        return;
    row.timeStates[stepZero] = StepState::Running;
    emitTimeCellChanged(rowIdx, row.timeColumns.value(stepZero, -1));
}

void QueueTableModel::setStepTime(const QString& flowName, int stepIndex, qint64 deviceMs)
{
    if (stepIndex <= 0)
        return;
    const int rowIdx = rowForFlowName(flowName);
    if (rowIdx < 0 || rowIdx >= m_rows.size())
        return;
    auto& row = m_rows[rowIdx];
    const int stepZero = stepIndex - 1;
    if (stepZero < 0 || stepZero >= row.timeColumns.size())
        return;
    const int colIdx = row.timeColumns[stepZero];
    if (colIdx < 0)
        return;
    if (colIdx >= row.cells.size())
        row.cells.resize(colIdx + 1);
    row.cells[colIdx] = QString::number(deviceMs);
    if (stepZero < row.timeStates.size())
        row.timeStates[stepZero] = StepState::Done;
    emitTimeCellChanged(rowIdx, colIdx);
}

int QueueTableModel::stepCountForFlow(const QString& flowName) const
{
    const int rowIdx = rowForFlowName(flowName);
    if (rowIdx < 0 || rowIdx >= m_rows.size())
        return 0;
    return m_rows[rowIdx].timeColumns.size();
}

void QueueTableModel::setLedColorMap(const QHash<int, QColor>& colors)
{
    m_ledColorMap = colors;
    if (!m_rows.isEmpty())
        emit dataChanged(index(0, 1), index(rowCount() - 1, columnCount() - 1));
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
    return 1 + m_tableColumnCount;
}

QVariant QueueTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    if (index.row() < 0 || index.row() >= m_rows.size())
        return {};
    if (index.column() < 0 || index.column() >= columnCount())
        return {};

    const DisplayRow& r = m_rows[index.row()];
    const int col = index.column();

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;

    if (role == Qt::BackgroundRole)
    {
        if (col == 0)
        {
            if (r.flowState == FlowState::Running)
                return kRunningColor;
            if (r.rerunMarked)
                return kRerunColor;
            if (r.flowState == FlowState::Done)
                return kDoneColor;
            return {};
        }

        const int cellIdx = col - 1;
        const int stepIdx = r.timeColumns.indexOf(cellIdx);
        if (stepIdx >= 0 && stepIdx < r.timeStates.size())
        {
            if (r.timeStates[stepIdx] == StepState::Running)
                return kRunningColor;
            if (r.timeStates[stepIdx] == StepState::Done)
                return kDoneColor;
        }
        if (!r.isHeader && r.ledColumns.contains(cellIdx))
        {
            bool ok = false;
            const int v = r.cells.value(cellIdx).toInt(&ok);
            if (ok && v > 0 && m_ledColorMap.contains(v))
                return m_ledColorMap.value(v);
        }
        return {};
    }

    if (role != Qt::DisplayRole)
        return {};

    if (col == 0)
        return r.flow;

    const int cellIdx = col - 1;
    if (cellIdx < 0 || cellIdx >= r.cells.size())
        return {};

    if (!r.isHeader && r.ledColumns.contains(cellIdx))
    {
        bool ok = false;
        const int v = r.cells[cellIdx].toInt(&ok);
        if (ok && v <= 0)
            return {};
    }

    return r.cells[cellIdx];
}

QVariant QueueTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    if (section == 0)
        return QStringLiteral("流程");

    const int excelCol = m_tableColumnStart + (section - 1);
    return excelColumnName(excelCol);
}

Qt::ItemFlags QueueTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void QueueTableModel::setFlowState(const QString& flowName, FlowState state)
{
    const int rowIdx = rowForFlowName(flowName);
    if (rowIdx < 0 || rowIdx >= m_rows.size())
        return;
    auto& row = m_rows[rowIdx];
    row.flowState = state;
    if (state == FlowState::Done)
        row.rerunMarked = false;
    emit dataChanged(index(rowIdx, 0), index(rowIdx, 0));
}

void QueueTableModel::emitTimeCellChanged(int rowIdx, int cellIdx)
{
    if (rowIdx < 0 || rowIdx >= m_rows.size())
        return;
    if (cellIdx < 0)
        return;
    const int modelCol = cellIdx + 1;
    if (modelCol < 1 || modelCol >= columnCount())
        return;
    emit dataChanged(index(rowIdx, modelCol), index(rowIdx, modelCol));
}
