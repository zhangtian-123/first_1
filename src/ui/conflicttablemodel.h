#pragma once
/**
 * @file conflicttablemodel.h
 * @brief Conflict triple table model (3 columns) backed by SettingsData.
 */

#include <QAbstractTableModel>
#include <QVector>

#include "../config/appsettings.h" // ConflictTriple

class ConflictTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ConflictTableModel(QObject* parent = nullptr);

    void setTriples(const QVector<ConflictTriple>& triples);
    const QVector<ConflictTriple>& triples() const { return m_triples; }

    void setMaxColorIndex(int maxIdx) { m_maxColorIndex = maxIdx; }
    void clearAll();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

private:
    bool isValidColorIndex(int v) const;

private:
    QVector<ConflictTriple> m_triples;
    int m_maxColorIndex = 0; // 0 means allow only 0
};

