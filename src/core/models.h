#pragma once
/**
 * @file models.h
 * @brief Core data models shared across the app.
 */

#include <QString>
#include <QVector>

/**
 * @brief Supported action types.
 */
enum class ActionType
{
    Unknown = 0,
    D,  ///< Delay
    L,  ///< LED
    B,  ///< Beep
    V   ///< Voice
};

/**
 * @brief Single action parsed from Excel (or resolved plan).
 */
struct ActionItem
{
    // Common
    QString flowName;                 ///< Segment name (used for grouping rows)
    ActionType type = ActionType::Unknown;
    QString rawParamText;             ///< Original text for logging/debug

    // Delay
    int delayMs = 0;

    // Beep
    int beepFreqHz = 0;
    int beepDurMs  = 0;

    // Voice
    int voiceMs = 0;
    QString voiceText;
    int voiceSet = 1;               ///< 1 or 2 (VOICESET1/VOICESET2)

    // LED
    QString ledMode;                  ///< ALL / SEQ / RAND
    QVector<int> ledColors;           ///< Size = LED count; 0 means random

    // Runtime marker (optional)
    bool markedForRerun = false;
};

/**
 * @brief Segment = contiguous actions sharing the same flowName.
 */
struct Segment
{
    QString name;
    int startIndex = -1; ///< inclusive
    int endIndex   = -1; ///< inclusive
};

inline QString actionTypeToString(ActionType t)
{
    switch (t)
    {
    case ActionType::D: return QStringLiteral("D");
    case ActionType::L: return QStringLiteral("L");
    case ActionType::B: return QStringLiteral("B");
    case ActionType::V: return QStringLiteral("V");
    default: return QStringLiteral("?");
    }
}

inline ActionType actionTypeFromString(const QString& s)
{
    const QString x = s.trimmed().toUpper();
    if (x == QStringLiteral("D") || x == QStringLiteral("DELAY")) return ActionType::D;
    if (x == QStringLiteral("L") || x == QStringLiteral("LED"))   return ActionType::L;
    if (x == QStringLiteral("B") || x == QStringLiteral("BEEP"))  return ActionType::B;
    if (x == QStringLiteral("V") || x == QStringLiteral("VOICE")) return ActionType::V;
    return ActionType::Unknown;
}

/**
 * @brief Standardise LED mode text; accepts Chinese aliases.
 * Returns trimmed text if unrecognised so caller can report an error.
 */
inline QString normalizeLedMode(const QString& mode)
{
    const QString trimmed = mode.trimmed();
    const QString upper = trimmed.toUpper();

    if (upper == QStringLiteral("ALL")
        || trimmed == QStringLiteral("全部")
        || trimmed == QStringLiteral("全亮")
        || trimmed == QStringLiteral("同时")
        || trimmed == QStringLiteral("同时点亮"))
    {
        return QStringLiteral("ALL");
    }

    if (upper == QStringLiteral("SEQ")
        || trimmed == QStringLiteral("顺序")
        || trimmed == QStringLiteral("顺序点亮")
        || trimmed == QStringLiteral("依次"))
    {
        return QStringLiteral("SEQ");
    }

    if (upper == QStringLiteral("RAND")
        || trimmed == QStringLiteral("随机")
        || trimmed == QStringLiteral("随机点亮"))
    {
        return QStringLiteral("RAND");
    }

    return trimmed;
}
