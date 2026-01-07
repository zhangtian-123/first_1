#pragma once
/**
 * @file conflictcolordelegate.h
 * @brief Delegate providing a combobox editor for conflict table cells.
 */

#include <QStyledItemDelegate>

class ColorTableModel;

class ConflictColorDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ConflictColorDelegate(ColorTableModel* colorsModel, QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

private:
    ColorTableModel* m_colorsModel = nullptr;
};

