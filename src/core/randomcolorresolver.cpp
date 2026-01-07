/**
 * @file randomcolorresolver.cpp
 * @brief 随机颜色解析器实现（0 -> 具体颜色编号 + 冲突约束求解）
 */

#include "randomcolorresolver.h"

#include <QSet>

static bool isLedModeValidUpper(const QString& m)
{
    const QString u = m.trimmed().toUpper();
    return (u == "ALL" || u == "SEQ" || u == "RAND");
}

QVector<int> RandomColorResolver::collectAvailableColorIndices(const QVector<ColorItem>& colorTable)
{
    QVector<int> out;
    out.reserve(colorTable.size());
    for (const auto& c : colorTable)
    {
        if (c.index > 0)
            out.push_back(c.index);
    }
    // 去重（防止用户写入异常重复编号）
    QSet<int> uniq(out.begin(), out.end());
    out = QVector<int>(uniq.begin(), uniq.end());
    std::sort(out.begin(), out.end());
    return out;
}

QVector<int> RandomColorResolver::alignLedColors(const QVector<int>& src, int ledCount)
{
    QVector<int> out;
    out.reserve(ledCount);

    for (int i = 0; i < ledCount; ++i)
    {
        if (i < src.size())
            out.push_back(src[i]);
        else
            out.push_back(0); // 不足补 0（随机）
    }

    if (out.size() > ledCount)
        out.resize(ledCount);

    return out;
}

bool RandomColorResolver::validateLedMode(const QString& mode, QString& errMsg)
{
    if (!isLedModeValidUpper(mode))
    {
        errMsg = QStringLiteral("LED 模式非法：%1（应为 ALL/SEQ/RAND 或可被标准化为这些值）").arg(mode);
        return false;
    }
    return true;
}

bool RandomColorResolver::validateFixedColorIndices(const QVector<int>& fixedColors,
                                                    const QVector<int>& availableColorIndices,
                                                    QString& errMsg)
{
    QSet<int> avail(availableColorIndices.begin(), availableColorIndices.end());

    for (int c : fixedColors)
    {
        if (c <= 0) continue;
        if (!avail.contains(c))
        {
            errMsg = QStringLiteral("固定颜色编号不存在于颜色表：%1").arg(c);
            return false;
        }
    }
    return true;
}

QVector<QVector<int>> RandomColorResolver::buildGroupsForColors(const QVector<int>& availableColorIndices,
                                                                const QVector<ConflictTriple>& conflicts)
{
    // groupsForColorLookup[k]：availableColorIndices[k] 属于哪些冲突组（组编号=conflicts 的索引）
    QVector<QVector<int>> groupsForColorLookup;
    groupsForColorLookup.resize(availableColorIndices.size());

    // 建一个 colorIdx -> lookupIndex 的映射（用线性找也可以，但这里先简单）
    auto findLookup = [&](int colorIdx)->int {
        for (int i = 0; i < availableColorIndices.size(); ++i)
        {
            if (availableColorIndices[i] == colorIdx)
                return i;
        }
        return -1;
    };

    for (int gi = 0; gi < conflicts.size(); ++gi)
    {
        const auto& g = conflicts[gi];
        const int a = g.c1;
        const int b = g.c2;
        const int c = g.c3;

        // 0 表示未设置（忽略）
        const int idxA = (a > 0) ? findLookup(a) : -1;
        const int idxB = (b > 0) ? findLookup(b) : -1;
        const int idxC = (c > 0) ? findLookup(c) : -1;

        if (idxA >= 0) groupsForColorLookup[idxA].push_back(gi);
        if (idxB >= 0) groupsForColorLookup[idxB].push_back(gi);
        if (idxC >= 0) groupsForColorLookup[idxC].push_back(gi);
    }

    return groupsForColorLookup;
}

bool RandomColorResolver::canPickColor(int idx,
                                       const QVector<int>& groupIndicesOfColor,
                                       const QVector<int>& repColorByGroup)
{
    // 如果该颜色属于某些组：
    // - 若组未被占用（rep=0）：允许
    // - 若组已被占用（rep!=idx）：不允许
    for (int g : groupIndicesOfColor)
    {
        if (g < 0 || g >= repColorByGroup.size())
            continue;

        const int rep = repColorByGroup[g];
        if (rep == 0) continue;
        if (rep != idx) return false;
    }
    return true;
}

QVector<int> RandomColorResolver::applyPickColor(int idx,
                                                 const QVector<int>& groupIndicesOfColor,
                                                 const QVector<int>& repColorByGroup)
{
    QVector<int> next = repColorByGroup;
    for (int g : groupIndicesOfColor)
    {
        if (g < 0 || g >= next.size())
            continue;

        if (next[g] == 0)
            next[g] = idx; // 第一次占用该组
        // 若 next[g] 已经是 idx，保持即可
    }
    return next;
}

bool RandomColorResolver::checkConflictSatisfied(const QVector<int>& finalColors,
                                                 const QVector<ConflictTriple>& conflicts,
                                                 QString& errMsg)
{
    errMsg.clear();

    // 收集集合（忽略 0）
    QSet<int> s;
    for (int c : finalColors)
    {
        if (c > 0) s.insert(c);
    }

    // 对每个三元组：如果同时出现任意两种不同颜色 -> 违规
    for (int i = 0; i < conflicts.size(); ++i)
    {
        const auto& g = conflicts[i];
        QVector<int> present;
        if (g.c1 > 0 && s.contains(g.c1)) present.push_back(g.c1);
        if (g.c2 > 0 && s.contains(g.c2)) present.push_back(g.c2);
        if (g.c3 > 0 && s.contains(g.c3)) present.push_back(g.c3);

        // present.size >= 2 => 至少出现两种不同颜色（因为集合去重）
        if (present.size() >= 2)
        {
            errMsg = QStringLiteral("冲突违规：冲突组 #%1 含 %2/%3/%4，其中同时出现了 %5")
                         .arg(i + 1)
                         .arg(g.c1).arg(g.c2).arg(g.c3)
                         .arg(QString("%1,%2").arg(present[0]).arg(present[1]));
            return false;
        }
    }

    return true;
}

int RandomColorResolver::indexOfColor(const QVector<int>& availableColorIndices, int colorIdx)
{
    for (int i = 0; i < availableColorIndices.size(); ++i)
    {
        if (availableColorIndices[i] == colorIdx)
            return i;
    }
    return -1;
}

bool RandomColorResolver::solveOneLedAction(const QVector<int>& alignedColors,
                                            const QVector<int>& availableColorIndices,
                                            const QVector<ConflictTriple>& conflicts,
                                            QVector<int>& filledColors,
                                            QString& errMsg)
{
    errMsg.clear();

    // 1) 固定颜色合法性（>0 的颜色必须存在于颜色表）
    if (!validateFixedColorIndices(alignedColors, availableColorIndices, errMsg))
        return false;

    // 2) 固定颜色本身是否已违反冲突（例如 fixed 同时含 1 和 2）
    if (!checkConflictSatisfied(alignedColors, conflicts, errMsg))
        return false;

    // 3) 若没有 0，直接成功
    QVector<int> work = alignedColors;
    QVector<int> zeros;
    for (int i = 0; i < work.size(); ++i)
    {
        if (work[i] == 0)
            zeros.push_back(i);
    }
    if (zeros.isEmpty())
    {
        filledColors = work;
        return true;
    }

    if (availableColorIndices.isEmpty())
    {
        errMsg = QStringLiteral("需要随机颜色，但颜色表为空");
        return false;
    }

    // 4) 构建 groupsForColorLookup
    QVector<QVector<int>> groupsForColorLookup = buildGroupsForColors(availableColorIndices, conflicts);

    // 5) 初始化 repColorByGroup：扫描固定颜色占用哪些冲突组
    QVector<int> repColorByGroup;
    repColorByGroup.resize(conflicts.size());
    for (int gi = 0; gi < repColorByGroup.size(); ++gi) repColorByGroup[gi] = 0;

    for (int c : work)
    {
        if (c <= 0) continue;

        // c 属于哪些组？
        const int k = indexOfColor(availableColorIndices, c);
        if (k < 0) continue; // 理论上不会发生（已校验存在）
        const QVector<int>& groups = groupsForColorLookup[k];

        // 占用这些组：若组已有 rep 且 != c => 本应被 checkConflictSatisfied 拦截，
        // 这里仍做保护
        for (int g : groups)
        {
            if (g < 0 || g >= repColorByGroup.size())
                continue;

            if (repColorByGroup[g] == 0)
                repColorByGroup[g] = c;
            else if (repColorByGroup[g] != c)
            {
                errMsg = QStringLiteral("固定颜色冲突不可解：冲突组 #%1 同时出现 %2 和 %3")
                             .arg(g + 1)
                             .arg(repColorByGroup[g])
                             .arg(c);
                return false;
            }
        }
    }

    // 6) 回溯填充 0
    if (!backtrackFill(work, zeros, 0,
                       availableColorIndices, conflicts,
                       repColorByGroup, groupsForColorLookup,
                       errMsg))
    {
        // errMsg 由 backtrackFill 填充
        return false;
    }

    filledColors = work;
    return true;
}

bool RandomColorResolver::backtrackFill(QVector<int>& workColors,
                                        const QVector<int>& zeroPositions,
                                        int posIdx,
                                        const QVector<int>& availableColorIndices,
                                        const QVector<ConflictTriple>& conflicts,
                                        QVector<int>& repColorByGroup,
                                        const QVector<QVector<int>>& groupsForColorLookup,
                                        QString& errMsg)
{
    Q_UNUSED(conflicts);

    if (posIdx >= zeroPositions.size())
    {
        // 全部填完，最终再做一次冲突校验（保险）
        QString tmp;
        if (!checkConflictSatisfied(workColors, conflicts, tmp))
        {
            errMsg = tmp;
            return false;
        }
        return true;
    }

    const int ledPos = zeroPositions[posIdx];

    // 为了更“随机”，我们可以对 availableColorIndices 做一个轮转或打乱。
    // 但为了可复现/便于调试，这里先按固定顺序尝试；后续可加入随机种子。
    for (int colorIdx : availableColorIndices)
    {
        // 当前 colorIdx 属于哪些冲突组？
        const int k = indexOfColor(availableColorIndices, colorIdx);
        if (k < 0) continue;

        const QVector<int>& groups = groupsForColorLookup[k];

        // 是否满足冲突占用约束？
        if (!canPickColor(colorIdx, groups, repColorByGroup))
            continue;

        // 试探：写入并更新 repColorByGroup
        const int old = workColors[ledPos];
        workColors[ledPos] = colorIdx;

        QVector<int> nextRep = applyPickColor(colorIdx, groups, repColorByGroup);

        // 深入下一层
        QVector<int> savedRep = repColorByGroup;
        repColorByGroup = nextRep;

        if (backtrackFill(workColors, zeroPositions, posIdx + 1,
                          availableColorIndices, conflicts,
                          repColorByGroup, groupsForColorLookup,
                          errMsg))
        {
            return true;
        }

        // 回溯
        repColorByGroup = savedRep;
        workColors[ledPos] = old;
    }

    // 所有颜色都尝试失败：无解
    errMsg = QStringLiteral("随机颜色不可解：在位置 LED%1 处无法选择任何颜色以满足冲突约束")
                 .arg(ledPos + 1);
    return false;
}

bool RandomColorResolver::precheckSolvable(const QVector<ActionItem>& actions,
                                          const QVector<ColorItem>& colorTable,
                                          const QVector<ConflictTriple>& conflicts,
                                          int ledCount,
                                          QString& errMsg)
{
    errMsg.clear();

    if (ledCount <= 0)
    {
        errMsg = QStringLiteral("LED数非法：%1").arg(ledCount);
        return false;
    }

    const QVector<int> avail = collectAvailableColorIndices(colorTable);

    // 逐个 L 动作检测
    for (int i = 0; i < actions.size(); ++i)
    {
        const auto& a = actions[i];
        if (a.type != ActionType::L)
            continue;

        // mode 校验
        QString mErr;
        if (!validateLedMode(a.ledMode, mErr))
        {
            errMsg = QStringLiteral("第 %1 行：%2").arg(i + 1).arg(mErr);
            return false;
        }

        QVector<int> aligned = alignLedColors(a.ledColors, ledCount);

        QVector<int> filled;
        QString sErr;
        if (!solveOneLedAction(aligned, avail, conflicts, filled, sErr))
        {
            errMsg = QStringLiteral("第 %1 行 L 动作无解/非法：%2").arg(i + 1).arg(sErr);
            return false;
        }
    }

    return true;
}

bool RandomColorResolver::resolveAll(const QVector<ActionItem>& actions,
                                    const QVector<ColorItem>& colorTable,
                                    const QVector<ConflictTriple>& conflicts,
                                    int ledCount,
                                    QVector<ActionItem>& outResolved,
                                    QString& errMsg)
{
    errMsg.clear();
    outResolved.clear();

    if (ledCount <= 0)
    {
        errMsg = QStringLiteral("LED数非法：%1").arg(ledCount);
        return false;
    }

    const QVector<int> avail = collectAvailableColorIndices(colorTable);

    outResolved = actions;

    for (int i = 0; i < outResolved.size(); ++i)
    {
        auto& a = outResolved[i];
        if (a.type != ActionType::L)
            continue;

        // mode 校验
        QString mErr;
        if (!validateLedMode(a.ledMode, mErr))
        {
            errMsg = QStringLiteral("第 %1 行：%2").arg(i + 1).arg(mErr);
            return false;
        }

        // 对齐 LED 数
        QVector<int> aligned = alignLedColors(a.ledColors, ledCount);

        // 求解填充 0
        QVector<int> filled;
        QString sErr;
        if (!solveOneLedAction(aligned, avail, conflicts, filled, sErr))
        {
            errMsg = QStringLiteral("第 %1 行 L 动作无解/非法：%2").arg(i + 1).arg(sErr);
            return false;
        }

        a.ledColors = filled;
    }

    return true;
}
