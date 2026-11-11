#ifndef HOLDEVENTFINGERPRINT_H
#define HOLDEVENTFINGERPRINT_H

#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <QString>

namespace HoldEventFingerprintV1
{

constexpr int Version = 1;

inline bool isWhitespace(const QChar character)
{
    const ushort code = character.unicode();

    switch (code) {
    case 0x0009:
    case 0x000A:
    case 0x000B:
    case 0x000C:
    case 0x000D:
    case 0x0020:
    case 0x0085:
    case 0x00A0:
    case 0x1680:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
        return true;
    default:
        return code >= 0x2000 && code <= 0x200A;
    }
}

inline QString trim(const QString& value)
{
    int start = 0;
    int end = value.size();

    while (start < end && isWhitespace(value.at(start)))
        ++start;

    while (end > start && isWhitespace(value.at(end - 1)))
        --end;

    return value.mid(start, end - start);
}

inline bool containsNullCharacter(const QString& value)
{
    return value.contains(QChar(0x0000));
}

inline bool canonicalAmount(const QString& input, QString* canonical)
{
    if (!canonical)
        return false;

    const QString value = input.trimmed();
    static const QRegularExpression pattern(
        QStringLiteral("^(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?$"));

    if (!pattern.match(value).hasMatch())
        return false;

    QString result = value;

    if (result.contains(QLatin1Char('.'))) {
        while (result.endsWith(QLatin1Char('0')))
            result.chop(1);

        if (result.endsWith(QLatin1Char('.')))
            result.chop(1);
    }

    *canonical = result;
    return true;
}

inline bool parseTimestamp(const QString& input,
                           QDateTime* parsed,
                           QString* normalized)
{
    if (!parsed || !normalized)
        return false;

    const QString value = trim(input);

    static const QRegularExpression pattern(
        QStringLiteral(
            "^(\\d{4})-(\\d{2})-(\\d{2})[Tt](\\d{2}):(\\d{2})"
            "(?::(\\d{2})(?:[.,](\\d{1,9}))?)?"
            "(Z|z|[+-]\\d{2}(?::?\\d{2})?)$"));

    const QRegularExpressionMatch match = pattern.match(value);
    if (!match.hasMatch())
        return false;

    const QString seconds = match.captured(6).isEmpty()
        ? QStringLiteral("00")
        : match.captured(6);
    QString fraction = match.captured(7);
    while (fraction.size() < 3)
        fraction.append(QLatin1Char('0'));
    const QString milliseconds = fraction.left(3);

    QString offset = match.captured(8);
    if (offset == QStringLiteral("z")) {
        offset = QStringLiteral("Z");
    } else if (offset.size() == 3) {
        offset += QStringLiteral(":00");
    } else if (offset.size() == 5 &&
               !offset.contains(QLatin1Char(':'))) {
        offset.insert(3, QLatin1Char(':'));
    }

    const QString reconstructed =
        match.captured(1) + QLatin1Char('-') +
        match.captured(2) + QLatin1Char('-') +
        match.captured(3) + QLatin1Char('T') +
        match.captured(4) + QLatin1Char(':') +
        match.captured(5) + QLatin1Char(':') +
        seconds + QLatin1Char('.') +
        milliseconds + offset;

    const QDateTime timestamp =
        QDateTime::fromString(reconstructed, Qt::ISODateWithMs);

    if (!timestamp.isValid() || timestamp.timeSpec() == Qt::LocalTime)
        return false;

    *parsed = timestamp;
    *normalized = timestamp.toUTC().toString(
        QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"));
    return true;
}

inline bool isValidFingerprintFormat(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{64}$"));
    return pattern.match(value).hasMatch();
}

struct FingerprintInput
{
    qint64 holdId = 0;
    QString eventType;
    QString amount;
    bool effectiveAtProvided = false;
    QString effectiveAt;
    QString reasonCode;
    QString referenceType;
    QString referenceId;
};

inline bool buildPayload(
    const FingerprintInput& input,
    QString* payload,
    QString* errorCode)
{
    if (!payload)
        return false;

    const QString eventType = trim(input.eventType);
    const QString reasonCode = trim(input.reasonCode);
    const QString referenceType = trim(input.referenceType);
    const QString referenceId = trim(input.referenceId);

    if (eventType.isEmpty() || reasonCode.isEmpty()) {
        if (errorCode) {
            *errorCode = QStringLiteral(
                "ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID");
        }
        return false;
    }

    const bool amountProvided =
        eventType == QStringLiteral("PARTIAL_RELEASE");
    QString amountText;

    if (amountProvided) {
        if (input.amount.isEmpty() ||
            !canonicalAmount(input.amount, &amountText)) {
            if (errorCode) {
                *errorCode = QStringLiteral(
                    "ACCOUNT_HOLD_EVENT_INVALID_AMOUNT");
            }
            return false;
        }
    }

    QString effectiveText;

    if (input.effectiveAtProvided) {
        QDateTime parsed;
        QString normalized;

        if (input.effectiveAt.isEmpty() ||
            !parseTimestamp(input.effectiveAt, &parsed, &normalized)) {
            if (errorCode) {
                *errorCode = QStringLiteral(
                    "ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT");
            }
            return false;
        }

        effectiveText = normalized;
    }

    auto byteLength = [](const QString& value) {
        return value.toUtf8().size();
    };

    *payload =
        QStringLiteral("HOLD_EVENT_FP\n") +
        QStringLiteral("1\n") +
        QStringLiteral("holdId=") + QString::number(input.holdId) + QLatin1Char('\n') +
        QStringLiteral("eventType=") +
            QString::number(byteLength(eventType)) + QLatin1Char(':') +
            eventType + QLatin1Char('\n') +
        QStringLiteral("amountProvided=") +
            (amountProvided ? QStringLiteral("1") : QStringLiteral("0")) +
            QLatin1Char('\n') +
        QStringLiteral("amount=") +
            QString::number(byteLength(amountText)) + QLatin1Char(':') +
            amountText + QLatin1Char('\n') +
        QStringLiteral("effectiveAtProvided=") +
            (input.effectiveAtProvided
                 ? QStringLiteral("1")
                 : QStringLiteral("0")) +
            QLatin1Char('\n') +
        QStringLiteral("effectiveAt=") +
            QString::number(byteLength(effectiveText)) + QLatin1Char(':') +
            effectiveText + QLatin1Char('\n') +
        QStringLiteral("reasonCode=") +
            QString::number(byteLength(reasonCode)) + QLatin1Char(':') +
            reasonCode + QLatin1Char('\n') +
        QStringLiteral("referenceType=") +
            QString::number(byteLength(referenceType)) + QLatin1Char(':') +
            referenceType + QLatin1Char('\n') +
        QStringLiteral("referenceId=") +
            QString::number(byteLength(referenceId)) + QLatin1Char(':') +
            referenceId + QLatin1Char('\n');

    return true;
}

inline bool fingerprint(
    const FingerprintInput& input,
    QString* result,
    QString* errorCode)
{
    if (!result)
        return false;

    QString payload;

    if (!buildPayload(input, &payload, errorCode))
        return false;

    *result = QString::fromLatin1(
        QCryptographicHash::hash(
            payload.toUtf8(), QCryptographicHash::Sha256).toHex());
    return true;
}

enum class MetadataState
{
    Complete,
    Legacy,
    Incomplete
};

inline MetadataState classifyMetadata(
    bool fingerprintNull,
    bool versionNull,
    bool effectiveAtProvidedNull)
{
    if (fingerprintNull && versionNull && effectiveAtProvidedNull)
        return MetadataState::Legacy;

    if (!fingerprintNull && !versionNull && !effectiveAtProvidedNull)
        return MetadataState::Complete;

    return MetadataState::Incomplete;
}

enum class Decision
{
    LegacyMatch,
    LegacyConflict,
    VersionedMatch,
    VersionedConflict,
    MetadataIncomplete,
    UnsupportedVersion,
    InvalidFingerprint
};

inline Decision classify(
    bool allMetadataNull,
    bool metadataIncomplete,
    const QString& storedFingerprint,
    int storedVersion,
    bool fieldsMatch,
    const QString& requestFingerprint)
{
    if (allMetadataNull)
        return fieldsMatch ? Decision::LegacyMatch : Decision::LegacyConflict;

    if (metadataIncomplete)
        return Decision::MetadataIncomplete;

    if (!isValidFingerprintFormat(storedFingerprint))
        return Decision::InvalidFingerprint;

    if (storedVersion != Version)
        return Decision::UnsupportedVersion;

    if (storedFingerprint == requestFingerprint)
        return Decision::VersionedMatch;

    return Decision::VersionedConflict;
}

} // namespace HoldEventFingerprintV1

#endif // HOLDEVENTFINGERPRINT_H
