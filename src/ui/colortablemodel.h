#pragma once
/**
 * @file colortablemodel.h
 * @brief Color table model (1..N) backed by SettingsData.
 */

#include <QAbstractTableModel>
#include <QColor>
#include <QVector>

#include "../config/appsettings.h" // ColorItem

class ColorTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ColorTableModel(QObject* parent = nullptr);

    void setColors(const QVector<ColorItem>& colors);
    const QVector<ColorItem>& colors() const { return m_colors; }

    bool addColor(const QColor& c, QString& errMsg);
    bool removeRowAt(int row);
    bool updateColorByIndex(int colorIndex, const QColor& c);
    void clearAll();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    static QVector<ColorItem> normalize(const QVector<ColorItem>& in);
    static QString toHex6(const QColor& c);

private:
    QVector<ColorItem> m_colors; // always indexed 1..N sequentially
};
