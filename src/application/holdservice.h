#ifndef HOLDSERVICE_H
#define HOLDSERVICE_H

#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace HoldFingerprint
{

inline QString compute(
    qint64 accountId,
    const QString& amount,
    const QString& reasonCode,
    const QString& reasonDescription,
    const QString& referenceType,
    const QString& referenceId,
    const QString& effectiveAt,
    const QString& expiresAt)
{
    const QByteArray payload =
        QStringLiteral("HOLD\n%1\n%2\n%3\n%4\n%5\n%6\n%7\n%8")
            .arg(accountId)
            .arg(amount)
            .arg(reasonCode)
            .arg(reasonDescription)
            .arg(referenceType)
            .arg(referenceId)
            .arg(effectiveAt)
            .arg(expiresAt)
            .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(
            payload, QCryptographicHash::Sha256).toHex());
}

inline bool normalizeLegacyTimestamp(
    const QString& input,
    QString* normalized)
{
    if (!normalized)
        return false;

    const QString value = input.trimmed();

    if (value.isEmpty())
        return false;

    QDateTime timestamp = QDateTime::fromString(value, Qt::ISODateWithMs);

    if (!timestamp.isValid())
        timestamp = QDateTime::fromString(value, Qt::ISODate);

    if (!timestamp.isValid())
        return false;

    *normalized = timestamp.toUTC().toString(Qt::ISODateWithMs);
    return true;
}

inline bool matchesStored(
    const QString& stored,
    const QString& current,
    const QString& legacy)
{
    return stored == current ||
        (!legacy.isEmpty() && stored == legacy);
}

} // namespace HoldFingerprint

struct HoldStatusData
{
    qint64 holdId = 0;
    qint64 accountId = 0;
    qint64 currencyId = 0;

    QString originalAmount;
    QString releasedAmount;
    QString remainingAmount;
    QString currentlyBlockedAmount;

    QString reasonCode;
    QString reasonDescription;
    QString referenceType;
    QString referenceId;

    QString status;
    QDateTime createdAt;
    QDateTime effectiveAt;
    QDateTime expiresAt;

    int releaseEventCount = 0;
    QString lastReleaseEventType;
};

struct HoldEventData
{
    qint64 eventId = 0;
    qint64 holdId = 0;
    QString eventType;
    QString amount;
    QDateTime effectiveAt;
    QDateTime createdAt;
    QString referenceType;
    QString referenceId;
    QString reasonCode;
};

struct HoldResult
{
    enum class Status {
        Completed,
        Rejected,
        Failed
    };

    Status status = Status::Failed;
    qint64 holdId = 0;
    bool replayed = false;
    QString code;
    QString message;
    HoldStatusData snapshot;
    bool hasSnapshot = false;
};

struct HoldEventResult
{
    enum class Status {
        Completed,
        Rejected,
        Failed
    };

    Status status = Status::Failed;
    qint64 eventId = 0;
    qint64 holdId = 0;
    QString eventType;
    QString amount;
    QDateTime effectiveAt;
    QString reasonCode;
    bool replayed = false;
    QString code;
    QString message;
    HoldStatusData snapshot;
    bool hasSnapshot = false;
};

struct HoldQueryResult
{
    enum class Status {
        Found,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    HoldStatusData data;
    QString code;
    QString message;
};

struct HoldListResult
{
    enum class Status {
        Completed,
        Failed
    };

    Status status = Status::Failed;
    QVector<HoldStatusData> holds;
    QString code;
    QString message;
};

struct HoldEventsResult
{
    enum class Status {
        Completed,
        HoldNotFound,
        Failed
    };

    Status status = Status::Failed;
    QVector<HoldEventData> events;
    QString code;
    QString message;
};

// Creates account holds and hold release events directly against the
// redesigned schema. Hold booking never changes ledger_balance or
// available_balance; those remain derived from active holds.
class HoldService
{
public:
    explicit HoldService(const QSqlDatabase& database);

    HoldResult createHold(
        qint64 accountId,
        const QString& amount,
        const QString& reasonCode,
        const QString& reasonDescription,
        const QString& referenceType,
        const QString& referenceId,
        const QString& effectiveAt,
        const QString& expiresAt,
        const QString& idempotencyKey);

    HoldEventResult createEvent(
        qint64 holdId,
        const QString& eventType,
        const QString& amount,
        const QString& effectiveAt,
        const QString& reasonCode,
        const QString& referenceType,
        const QString& referenceId,
        const QString& idempotencyKey);

    HoldQueryResult getHoldById(qint64 holdId);
    HoldListResult getHoldsByAccount(qint64 accountId);
    HoldEventsResult getHoldEvents(qint64 holdId);

private:
    QSqlDatabase m_database;
};

#endif // HOLDSERVICE_H
