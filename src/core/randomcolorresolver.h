#pragma once
/**
 * @file randomcolorresolver.h
 * @brief 随机颜色解析器：把 L 动作中的颜色 0（随机）替换为具体颜色编号，并进行冲突约束校验
 *
 * ✅ 需求对齐：
 * 1) Excel 里颜色列允许填 0 表示随机
 * 2) 随机颜色由上位机根据【颜色表 + 冲突表 + LED数】生成
 * 3) 冲突表：每行固定 3 个颜色；必须避免同一冲突组内出现任意两个“不同颜色”
 *    - 若不可避免（无解），点击【开始】后立马检测并弹窗警告（上位机不进入运行态）
 * 4) LED 个数以设置页为准：resolver 会把 L 动作的 ledColors 对齐到 ledCount（截断/补齐0）
 *
 * 🔎 约束解释（按你的最终要求）：
 * - 对每一个 L 动作，收集其最终颜色集合（忽略 0）
 * - 对冲突组 (a,b,c)：若集合中同时出现 {a,b} 或 {a,c} 或 {b,c} 中任意两种不同颜色 -> 违规
 * - 同一颜色重复出现不算“两种不同颜色”，允许（例如 1,1,1 合法）
 *
 * 🧠 算法说明：
 * - 为了避免“随机选错导致误判无解”，这里提供：
 *   - precheckSolvable(): 对每个 L 动作用回溯搜索保证判断是否有解（不会因为运气差误判）
 *   - resolveAll(): 在保证可解的前提下，生成一份 resolved plan（0 -> 具体颜色）
 */

#include <QString>
#include <QVector>

#include "models.h"
#include "../config/appsettings.h"  // ColorItem, ConflictTriple

class RandomColorResolver
{
public:
    /**
     * @brief 点击“开始”时的预检：检查是否可解（不修改输入动作）
     * @param actions   ExcelImporter 解析出来的动作列表（可能包含 0）
     * @param colorTable 用户配置的颜色表（可为空；若需要随机则必须非空）
     * @param conflicts 用户配置的冲突表（三元组/行）
     * @param ledCount  设置页 LED 数（最终生效 LED 数）
     * @param errMsg    失败原因（用于弹窗）
     * @return true=全部 L 动作可解且参数合法；false=存在无解/非法
     */
    static bool precheckSolvable(const QVector<ActionItem>& actions,
                                 const QVector<ColorItem>& colorTable,
                                 const QVector<ConflictTriple>& conflicts,
                                 int ledCount,
                                 QString& errMsg);

    /**
     * @brief 生成 resolved plan：把所有 L 动作的 0（随机）替换为具体颜色编号
     * @param actions   输入动作列表（包含 0）
     * @param colorTable 颜色表
     * @param conflicts 冲突表
     * @param ledCount  LED 数（最终生效）
     * @param outResolved 输出：已替换 0 的新动作列表（与输入等长，顺序一致）
     * @param errMsg    失败原因
     * @return true=成功生成；false=无解/非法
     */
    static bool resolveAll(const QVector<ActionItem>& actions,
                           const QVector<ColorItem>& colorTable,
                           const QVector<ConflictTriple>& conflicts,
                           int ledCount,
                           QVector<ActionItem>& outResolved,
                           QString& errMsg);

private:
    // ----------------------------
    // 内部工具：颜色表 -> 可用编号集合
    // ----------------------------
    static QVector<int> collectAvailableColorIndices(const QVector<ColorItem>& colorTable);

    // ----------------------------
    // 内部工具：对齐 ledColors 长度到 ledCount
    // - 若不足：补 0（视为随机）
    // - 若超出：截断
    // ----------------------------
    static QVector<int> alignLedColors(const QVector<int>& src, int ledCount);

    // ----------------------------
    // 内部工具：校验 L mode 合法性（必须是 ALL/SEQ/RAND）
    // ----------------------------
    static bool validateLedMode(const QString& mode, QString& errMsg);

    // ----------------------------
    // 内部工具：校验固定颜色编号是否存在于颜色表
    // - fixedColors 中 >0 的编号必须在 colorTable 里存在
    // ----------------------------
    static bool validateFixedColorIndices(const QVector<int>& fixedColors,
                                          const QVector<int>& availableColorIndices,
                                          QString& errMsg);

    // ----------------------------
    // 冲突组处理：
    // - groupsForColor[i] = 颜色 i 属于哪些冲突组三元组（用 group index 表示）
    // - repColor[group]  = 当前该冲突组“已选择”的代表色（0=尚未占用）
    //   若 repColor[g] = 2，则同组内只能再选 2（允许重复），不能选 1/3
    // ----------------------------
    static QVector<QVector<int>> buildGroupsForColors(const QVector<int>& availableColorIndices,
                                                      const QVector<ConflictTriple>& conflicts);

    // ----------------------------
    // 判断一个颜色 idx 是否能在当前 repColor 状态下被选中
    // ----------------------------
    static bool canPickColor(int idx,
                             const QVector<int>& groupIndicesOfColor,
                             const QVector<int>& repColorByGroup);

    // ----------------------------
    // 选中一个颜色 idx，更新 repColor（返回更新后的副本）
    // ----------------------------
    static QVector<int> applyPickColor(int idx,
                                       const QVector<int>& groupIndicesOfColor,
                                       const QVector<int>& repColorByGroup);

    // ----------------------------
    // 检查最终颜色集合是否违反冲突规则（用于固定色直接违规时）
    // ----------------------------
    static bool checkConflictSatisfied(const QVector<int>& finalColors,
                                       const QVector<ConflictTriple>& conflicts,
                                       QString& errMsg);

    // ----------------------------
    // 对单个 L 动作做求解（回溯）：
    // 输入：alignedColors（长度=ledCount，包含 0）
    // 输出：filledColors（长度=ledCount，0 被替换为可用颜色编号）
    // ----------------------------
    static bool solveOneLedAction(const QVector<int>& alignedColors,
                                  const QVector<int>& availableColorIndices,
                                  const QVector<ConflictTriple>& conflicts,
                                  QVector<int>& filledColors,
                                  QString& errMsg);

    // 回溯递归
    static bool backtrackFill(QVector<int>& workColors,
                              const QVector<int>& zeroPositions,
                              int posIdx,
                              const QVector<int>& availableColorIndices,
                              const QVector<ConflictTriple>& conflicts,
                              QVector<int>& repColorByGroup,
                              const QVector<QVector<int>>& groupsForColorLookup,
                              QString& errMsg);

    // ----------------------------
    // 把可用颜色编号映射到 lookup 索引（避免直接用 color idx 当数组下标）
    // - groupsForColorLookup[k] 对应 availableColorIndices[k] 这个颜色的所属组
    // - 若要查 idx 的组：先在 availableColorIndices 中找 idx 的位置 k
    // ----------------------------
    static int indexOfColor(const QVector<int>& availableColorIndices, int colorIdx);
};
