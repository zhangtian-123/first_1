#include "conflicttablemodel.h"

#include <algorithm>

ConflictTableModel::ConflictTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void ConflictTableModel::setTriples(const QVector<ConflictTriple>& triples)
{
    beginResetModel();
    m_triples = triples;
    endResetModel();
}

void ConflictTableModel::clearAll()
{
    beginResetModel();
    m_triples.clear();
    endResetModel();
}

int ConflictTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_triples.size();
}

int ConflictTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 3;
}

bool ConflictTableModel::isValidColorIndex(int v) const
{
    if (v == 0)
        return true;
    if (v < 0)
        return false;
    return v <= m_maxColorIndex;
}

QVariant ConflictTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    if (index.row() < 0 || index.row() >= m_triples.size())
        return {};
    if (index.column() < 0 || index.column() >= 3)
        return {};

    const auto& t = m_triples[index.row()];
    int v = 0;
    if (index.column() == 0) v = t.c1;
    if (index.column() == 1) v = t.c2;
    if (index.column() == 2) v = t.c3;

    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return v;
    return {};
}

QVariant ConflictTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    if (section == 0) return QStringLiteral("颜色1");
    if (section == 1) return QStringLiteral("颜色2");
    if (section == 2) return QStringLiteral("颜色3");
    return {};
}

Qt::ItemFlags ConflictTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool ConflictTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole)
        return false;
    if (!index.isValid())
        return false;
    if (index.row() < 0 || index.row() >= m_triples.size())
        return false;
    if (index.column() < 0 || index.column() >= 3)
        return false;

    bool ok = false;
    const int v = value.toInt(&ok);
    if (!ok)
        return false;
    if (!isValidColorIndex(v))
        return false;

    auto& t = m_triples[index.row()];
    if (index.column() == 0) t.c1 = v;
    if (index.column() == 1) t.c2 = v;
    if (index.column() == 2) t.c3 = v;

    emit dataChanged(index, index);
    return true;
}

bool ConflictTableModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid())
        return false;
    if (count <= 0)
        return false;

    const int maxRow = static_cast<int>(m_triples.size());
    const int r = std::clamp(row, 0, maxRow);
    beginInsertRows(QModelIndex(), r, r + count - 1);
    for (int i = 0; i < count; ++i)
        m_triples.insert(r, ConflictTriple{0, 0, 0});
    endInsertRows();
    return true;
}

bool ConflictTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid())
        return false;
    if (count <= 0)
        return false;
    if (row < 0 || row + count > m_triples.size())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for (int i = 0; i < count; ++i)
        m_triples.removeAt(row);
    endRemoveRows();
    return true;
}
