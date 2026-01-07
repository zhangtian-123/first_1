#pragma once
/**
 * @file queuetablemodel.h
 * @brief Read-only table model for the Excel-driven work queue (Segment list).
 */

#include <QAbstractTableModel>
#include <QColor>
#include <QHash>
#include <QVector>

#include "../core/excelimporter.h" // HeaderCol/HeaderField
#include "../core/models.h"        // ActionItem

class QueueTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit QueueTableModel(QObject* parent = nullptr);

    void clear();
    void setHeaderColumns(const QVector<HeaderCol>& headerCols);
    void setActions(const QVector<ActionItem>& actions);

    int rowForFlowName(const QString& flowName) const;
    void clearStatuses();
    void setStatusForFlowName(const QString& flowName, const QString& text, const QColor& bg);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    struct SegmentRow
    {
        QString flow;
        QString mode;
        QVector<int> leds; // size = max LED index

        bool hasBeep = false;
        bool beepDefault = false;
        int beepDurMs = 0;

        bool hasVoice = false;
        QString voice;
        int voiceSet = 1;
        bool hasVoiceSet = false;

        bool hasDelay = false;
        int delayMs = 0;

        QString statusText;
        QColor statusBg;
        bool hasStatusBg = false;
    };

    void rebuildRows();

private:
    QVector<HeaderCol> m_headerCols;
    int m_maxLedIdx = 0;

    QVector<ActionItem> m_actions;
    QVector<SegmentRow> m_rows;
    QHash<QString, int> m_flowRow;
};
