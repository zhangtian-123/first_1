#include "conflictcolordelegate.h"

#include <QComboBox>

#include "colortablemodel.h"

ConflictColorDelegate::ConflictColorDelegate(ColorTableModel* colorsModel, QObject* parent)
    : QStyledItemDelegate(parent), m_colorsModel(colorsModel)
{
}

QWidget* ConflictColorDelegate::createEditor(QWidget* parent,
                                            const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    auto* cb = new QComboBox(parent);
    cb->setEditable(false);
    cb->addItem(QStringLiteral("0"), 0);
    if (m_colorsModel)
    {
        const int n = m_colorsModel->rowCount();
        for (int i = 1; i <= n; ++i)
            cb->addItem(QString::number(i), i);
    }
    return cb;
}

void ConflictColorDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* cb = qobject_cast<QComboBox*>(editor);
    if (!cb)
        return;
    const int v = index.model()->data(index, Qt::EditRole).toInt();
    const int idx = cb->findData(v);
    if (idx >= 0)
        cb->setCurrentIndex(idx);
}

void ConflictColorDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* cb = qobject_cast<QComboBox*>(editor);
    if (!cb)
        return;
    model->setData(index, cb->currentData(), Qt::EditRole);
}

void ConflictColorDelegate::updateEditorGeometry(QWidget* editor,
                                                const QStyleOptionViewItem& option,
                                                const QModelIndex& index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

