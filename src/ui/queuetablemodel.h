#pragma once
/**
 * @file queuetablemodel.h
 * @brief Read-only table model for the Excel-driven work queue (Segment list).
 */

#include <QAbstractTableModel>
#include <QColor>
#include <QHash>
#include <QVector>

#include "../core/excelimporter.h"
#include "../core/models.h"

class QueueTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit QueueTableModel(QObject* parent = nullptr);

    void clear();
    void setTableRows(const QVector<ExcelTableRow>& rows, int columnStart, int columnCount);
    void setActions(const QVector<ActionItem>& actions);

    int rowForFlowName(const QString& flowName) const;
    void clearFlowStates();
    void setFlowRunning(const QString& flowName);
    void setFlowDone(const QString& flowName);
    void setFlowRerunMarked(const QString& flowName);

    void clearStepTimes();
    void setStepRunning(const QString& flowName, int stepIndex);
    void setStepTime(const QString& flowName, int stepIndex, qint64 deviceMs);
    int stepCountForFlow(const QString& flowName) const;
    void setLedColorMap(const QHash<int, QColor>& colors);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    enum class FlowState { None, Running, Done };
    enum class StepState { None, Running, Done };

    struct DisplayRow
    {
        bool isHeader = false;
        QString flow;
        QVector<QString> cells;
        QVector<int> ledColumns;
        QVector<int> timeColumns;
        FlowState flowState = FlowState::None;
        bool rerunMarked = false;
        QVector<StepState> timeStates;
    };

    void setFlowState(const QString& flowName, FlowState state);
    void emitTimeCellChanged(int rowIdx, int cellIdx);

    QVector<ActionItem> m_actions;
    QVector<DisplayRow> m_rows;
    QHash<QString, int> m_flowRow;
    QHash<int, QColor> m_ledColorMap;
    int m_tableColumnStart = 1;
    int m_tableColumnCount = 0;
};
