#include "colortablemodel.h"

#include <algorithm>

ColorTableModel::ColorTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

QVector<ColorItem> ColorTableModel::normalize(const QVector<ColorItem>& in)
{
    QVector<ColorItem> out = in;
    std::sort(out.begin(), out.end(), [](const ColorItem& a, const ColorItem& b){
        return a.index < b.index;
    });
    if (out.size() > 100)
        out.resize(100);
    for (int i = 0; i < out.size(); ++i)
        out[i].index = i + 1;
    return out;
}

QString ColorTableModel::toHex6(const QColor& c)
{
    return QString("%1%2%3")
        .arg(c.red(),   2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(),  2, 16, QChar('0'))
        .toUpper();
}

void ColorTableModel::setColors(const QVector<ColorItem>& colors)
{
    beginResetModel();
    m_colors = normalize(colors);
    endResetModel();
}

bool ColorTableModel::addColor(const QColor& c, QString& errMsg)
{
    errMsg.clear();
    if (!c.isValid())
        return false;
    if (m_colors.size() >= 100)
    {
        errMsg = QStringLiteral("颜色数量已达上限(100)");
        return false;
    }

    const int row = m_colors.size();
    beginInsertRows(QModelIndex(), row, row);
    ColorItem it;
    it.index = row + 1;
    it.rgb = c;
    m_colors.push_back(it);
    endInsertRows();
    return true;
}

bool ColorTableModel::removeRowAt(int row)
{
    if (row < 0 || row >= m_colors.size())
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    m_colors.removeAt(row);
    endRemoveRows();

    // renumber
    for (int i = 0; i < m_colors.size(); ++i)
        m_colors[i].index = i + 1;

    if (!m_colors.isEmpty())
        emit dataChanged(index(0, 0), index(m_colors.size() - 1, 0));
    return true;
}

void ColorTableModel::clearAll()
{
    beginResetModel();
    m_colors.clear();
    endResetModel();
}

int ColorTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_colors.size();
}

int ColorTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 2; // index, hex
}

QVariant ColorTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    if (index.row() < 0 || index.row() >= m_colors.size())
        return {};

    const auto& c = m_colors[index.row()];
    const int col = index.column();

    if (role == Qt::BackgroundRole && col == 1)
        return c.rgb;

    if (role != Qt::DisplayRole)
        return {};

    if (col == 0)
        return c.index;
    if (col == 1)
        return toHex6(c.rgb);
    return {};
}

QVariant ColorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    if (section == 0) return QStringLiteral("编号");
    if (section == 1) return QStringLiteral("HEX");
    return {};
}

Qt::ItemFlags ColorTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

