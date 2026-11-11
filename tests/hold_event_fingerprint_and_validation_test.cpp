#include "../src/application/holdeventfingerprint.h"
#include "../src/application/holdservice.h"
#include "../src/web/holdcontroller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

void testTrim()
{
    using namespace HoldEventFingerprintV1;

    const QString surrounded =
        QChar(0x00A0) + QStringLiteral("value") + QChar(0x3000);
    check(trim(surrounded) == QStringLiteral("value"),
          "V1 trim removes Unicode whitespace");

    const QString internal =
        QStringLiteral("a") + QChar(0x2003) + QStringLiteral("b");
    check(trim(internal) == internal,
          "V1 trim keeps internal whitespace");

    const QString excluded =
        QChar(0x001C) + QStringLiteral("value") + QChar(0x001F);
    check(trim(excluded) == excluded,
          "V1 trim excludes U+001C..U+001F");

    check(containsNullCharacter(QStringLiteral("a") + QChar(0) +
                                QStringLiteral("b")),
          "NUL detection");
    check(!containsNullCharacter(QStringLiteral("plain")),
          "NUL absence");
}

void testCanonicalAmount()
{
    using namespace HoldEventFingerprintV1;

    QString canonical;
    const struct {
        const char* input;
        const char* expected;
    } cases[] = {
        { "0.10", "0.1" },
        { "1.23000000", "1.23" },
        { "10.0", "10" },
        { "10", "10" },
        { "0.000000010", "0.00000001" },
        { "0.01", "0.01" }
    };

    for (const auto& item : cases) {
        check(canonicalAmount(QString::fromLatin1(item.input), &canonical),
              "canonical amount accepts valid decimal");
        check(canonical == QString::fromLatin1(item.expected),
              "canonical amount output");
    }

    check(!canonicalAmount(QStringLiteral("1e-3"), &canonical),
          "canonical amount rejects exponent");
    check(!canonicalAmount(QStringLiteral("-0.1"), &canonical),
          "canonical amount rejects negative");
}

void testTimestamp()
{
    using namespace HoldEventFingerprintV1;

    QDateTime parsed;
    QString normalized;

    check(parseTimestamp(QStringLiteral("2026-11-03T16:23:06.123Z"),
                         &parsed, &normalized),
          "timestamp with Z parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "timestamp with Z normalizes");

    check(parseTimestamp(
              QStringLiteral("2026-11-03T19:53:06.123+03:30"),
              &parsed, &normalized),
          "timestamp with offset parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "timestamp with offset converts to UTC");

    check(parseTimestamp(
              QStringLiteral("2026-11-03T19:53:06.123+0330"),
              &parsed, &normalized),
          "timestamp with compact offset parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "timestamp with compact offset converts to UTC");

    check(parseTimestamp(
              QStringLiteral("2026-11-03T19:23:06.123+03"),
              &parsed, &normalized),
          "timestamp with hour-only offset parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "timestamp with hour-only offset converts to UTC");

    check(parseTimestamp(
              QStringLiteral("2026-11-03T12:53:06.123-03:30"),
              &parsed, &normalized),
          "timestamp with negative offset parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "timestamp with negative offset converts to UTC");

    check(parseTimestamp(
              QStringLiteral("2026-11-03T16:23:06.1239Z"),
              &parsed, &normalized),
          "timestamp with extra precision parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "timestamp truncates to milliseconds without rounding");

    check(parseTimestamp(QStringLiteral("2026-11-03T16:23:06Z"),
                         &parsed, &normalized),
          "timestamp without fraction parses");
    check(normalized == QStringLiteral("2026-11-03T16:23:06.000Z"),
          "timestamp without fraction normalizes milliseconds");

    check(!parseTimestamp(QStringLiteral("2026-11-03T16:23:06"),
                          &parsed, &normalized),
          "timestamp without timezone is rejected");
    check(!parseTimestamp(QStringLiteral("2026-13-03T16:23:06Z"),
                          &parsed, &normalized),
          "invalid timestamp is rejected");
    check(!parseTimestamp(QStringLiteral("not-a-timestamp"),
                          &parsed, &normalized),
          "garbage timestamp is rejected");
}

void testUtf8LengthsAndTrimmedFingerprint()
{
    using namespace HoldEventFingerprintV1;

    FingerprintInput input;
    input.holdId = 7;
    input.eventType = QStringLiteral(" FULL_RELEASE ");
    input.reasonCode = QString::fromUtf8("سلام");
    input.referenceType = QChar(0x00A0) + QStringLiteral("R") + QChar(0x00A0);

    QString payload;
    QString error;
    check(buildPayload(input, &payload, &error),
          "UTF-8 payload builds");
    check(payload.contains(QStringLiteral("eventType=12:FULL_RELEASE")),
          "fingerprint trims event type with V1 rules");
    check(payload.contains(QStringLiteral("reasonCode=8:سلام")),
          "fingerprint uses UTF-8 byte length");
    check(payload.contains(QStringLiteral("referenceType=1:R")),
          "fingerprint trims reference type with V1 rules");
}

void testMetadataClassification()
{
    using namespace HoldEventFingerprintV1;

    const struct {
        bool fingerprintNull;
        bool versionNull;
        bool providedNull;
        MetadataState expected;
    } cases[] = {
        { true, true, true, MetadataState::Legacy },
        { false, false, false, MetadataState::Complete },
        { false, true, true, MetadataState::Incomplete },
        { true, false, true, MetadataState::Incomplete },
        { true, true, false, MetadataState::Incomplete },
        { false, false, true, MetadataState::Incomplete },
        { false, true, false, MetadataState::Incomplete },
        { true, false, false, MetadataState::Incomplete }
    };

    for (const auto& item : cases) {
        check(classifyMetadata(
                  item.fingerprintNull,
                  item.versionNull,
                  item.providedNull) == item.expected,
              "metadata null combination classifies correctly");
    }
}

void testFingerprintAmountIdentity()
{
    using namespace HoldEventFingerprintV1;

    FingerprintInput partialTenth;
    partialTenth.holdId = 7;
    partialTenth.eventType = QStringLiteral("PARTIAL_RELEASE");
    partialTenth.amount = QStringLiteral("0.10");
    partialTenth.reasonCode = QStringLiteral("R");

    FingerprintInput partialOneTenth = partialTenth;
    partialOneTenth.amount = QStringLiteral("0.1");

    QString first;
    QString second;
    QString error;

    check(fingerprint(partialTenth, &first, &error),
          "0.10 fingerprint builds");
    check(fingerprint(partialOneTenth, &second, &error),
          "0.1 fingerprint builds");
    check(first == second,
          "0.10 and 0.1 produce the same fingerprint");

    FingerprintInput fullWithAmount;
    fullWithAmount.holdId = 7;
    fullWithAmount.eventType = QStringLiteral("FULL_RELEASE");
    fullWithAmount.amount = QStringLiteral("999.999");
    fullWithAmount.reasonCode = QStringLiteral("R");

    FingerprintInput fullWithoutAmount = fullWithAmount;
    fullWithoutAmount.amount.clear();

    QString payload;
    check(buildPayload(fullWithAmount, &payload, &error),
          "full release payload builds");
    check(payload.contains(QStringLiteral("amountProvided=0\n")) &&
              payload.contains(QStringLiteral("amount=0:\n")),
          "full release payload ignores the computed amount");

    check(fingerprint(fullWithAmount, &first, &error) &&
              fingerprint(fullWithoutAmount, &second, &error),
          "full release fingerprints build");
    check(first == second,
          "full release fingerprint ignores the computed amount");
}

void testHoldLegacyFingerprintCompatibility()
{
    using namespace HoldEventFingerprintV1;

    const QString input = QStringLiteral("2026-11-03T16:23:06.1239Z");

    QDateTime parsed;
    QString currentNormalized;
    QString legacyNormalized;

    check(parseTimestamp(input, &parsed, &currentNormalized),
          "new timestamp normalization succeeds");
    check(currentNormalized == QStringLiteral("2026-11-03T16:23:06.123Z"),
          "new timestamp normalization truncates");

    check(HoldFingerprint::normalizeLegacyTimestamp(
              input, &legacyNormalized),
          "legacy timestamp normalization succeeds");
    check(legacyNormalized == QStringLiteral("2026-11-03T16:23:06.124Z"),
          "legacy timestamp normalization preserves Qt rounding");

    const QString currentFingerprint = HoldFingerprint::compute(
        1,
        QStringLiteral("10"),
        QStringLiteral("R"),
        QString(),
        QString(),
        QString(),
        currentNormalized,
        QString());
    const QString legacyFingerprint = HoldFingerprint::compute(
        1,
        QStringLiteral("10"),
        QStringLiteral("R"),
        QString(),
        QString(),
        QString(),
        legacyNormalized,
        QString());

    check(currentFingerprint != legacyFingerprint,
          "rounded and truncated timestamps produce different fingerprints");
    check(HoldFingerprint::matchesStored(
              legacyFingerprint, currentFingerprint, legacyFingerprint),
          "legacy stored fingerprint matches the legacy candidate");
    check(HoldFingerprint::matchesStored(
              currentFingerprint, currentFingerprint, legacyFingerprint),
          "current stored fingerprint matches the current candidate");
    check(!HoldFingerprint::matchesStored(
              QString(64, QLatin1Char('a')),
              currentFingerprint,
              legacyFingerprint),
          "unrelated stored fingerprint does not match");
    check(!HoldFingerprint::matchesStored(
              legacyFingerprint, currentFingerprint, QString()),
          "empty legacy candidate does not match");
}

void testTestVectors()
{
    using namespace HoldEventFingerprintV1;

    FingerprintInput vectorA;
    vectorA.holdId = 7;
    vectorA.eventType = QStringLiteral("PARTIAL_RELEASE");
    vectorA.amount = QStringLiteral("0.01");
    vectorA.effectiveAtProvided = true;
    vectorA.effectiveAt = QStringLiteral("2026-11-03T16:23:06.000Z");
    vectorA.reasonCode = QStringLiteral("R1");

    QString payload;
    QString error;
    check(buildPayload(vectorA, &payload, &error),
          "vector A payload builds");
    check(payload ==
              QStringLiteral(
                  "HOLD_EVENT_FP\n"
                  "1\n"
                  "holdId=7\n"
                  "eventType=15:PARTIAL_RELEASE\n"
                  "amountProvided=1\n"
                  "amount=4:0.01\n"
                  "effectiveAtProvided=1\n"
                  "effectiveAt=24:2026-11-03T16:23:06.000Z\n"
                  "reasonCode=2:R1\n"
                  "referenceType=0:\n"
                  "referenceId=0:\n"),
          "vector A payload is canonical");

    QString fingerprint;
    check(HoldEventFingerprintV1::fingerprint(vectorA, &fingerprint, &error),
          "vector A fingerprint builds");
    check(fingerprint ==
              QStringLiteral(
                  "f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1"),
          "vector A hash matches");

    FingerprintInput vectorB;
    vectorB.holdId = 7;
    vectorB.eventType = QStringLiteral("PARTIAL_RELEASE");
    vectorB.amount = QStringLiteral("0.02");
    vectorB.effectiveAtProvided = false;
    vectorB.reasonCode = QStringLiteral("R2");

    check(HoldEventFingerprintV1::fingerprint(vectorB, &fingerprint, &error),
          "vector B fingerprint builds");
    check(fingerprint ==
              QStringLiteral(
                  "5e7271fc717f758194af0a0a66247da6a65e34d669359d084876397a3fa5ea8f"),
          "vector B hash matches");
}

void testClassification()
{
    using namespace HoldEventFingerprintV1;

    const QString stored =
        QStringLiteral(
            "f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1");

    check(classify(true, false, QString(), 0, true, QString()) ==
              Decision::LegacyMatch,
          "legacy exact retry classifies as match");
    check(classify(true, false, QString(), 0, false, QString()) ==
              Decision::LegacyConflict,
          "legacy different payload classifies as conflict");
    check(classify(false, true, stored, 1, false, stored) ==
              Decision::MetadataIncomplete,
          "incomplete metadata fails closed");
    check(classify(false, false, stored, 1, false, stored) ==
              Decision::VersionedMatch,
          "versioned exact retry classifies as match");
    check(classify(false, false, stored, 1, false, QString(64, QLatin1Char('a'))) ==
              Decision::VersionedConflict,
          "versioned different fingerprint classifies as conflict");
    check(classify(false, false, stored, 0, false, stored) ==
              Decision::UnsupportedVersion,
          "version zero fails closed");
    check(classify(false, false, stored, -1, false, stored) ==
              Decision::UnsupportedVersion,
          "negative version fails closed");
    check(classify(false, false, stored, 2, false, stored) ==
              Decision::UnsupportedVersion,
          "unknown version fails closed");
    check(classify(false, false, QStringLiteral("bad"), 1, false, stored) ==
              Decision::InvalidFingerprint,
          "invalid stored fingerprint fails closed");
    check(classify(false, false, stored.left(63), 1, false, stored) ==
              Decision::InvalidFingerprint,
          "short stored fingerprint fails closed");
    check(classify(false, false,
                  QString(64, QLatin1Char('A')), 1, false, stored) ==
              Decision::InvalidFingerprint,
          "uppercase stored fingerprint fails closed");
}

void testServiceNullCharacter()
{
    QSqlDatabase database;
    HoldService service(database);

    const HoldEventResult eventResult = service.createEvent(
        1,
        QStringLiteral("FULL_RELEASE"),
        QString(),
        QString(),
        QStringLiteral("R"),
        QString(),
        QString(),
        QStringLiteral("key") + QChar(0));

    check(eventResult.status == HoldEventResult::Status::Rejected,
          "service rejects NUL event input");
    check(eventResult.code == QStringLiteral("HOLD_INVALID_TEXT_INPUT"),
          "service returns NUL event code");

    const HoldResult holdResult = service.createHold(
        1,
        QStringLiteral("1"),
        QStringLiteral("R") + QChar(0),
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("key"));

    check(holdResult.status == HoldResult::Status::Rejected,
          "service rejects NUL hold input");
    check(holdResult.code == QStringLiteral("HOLD_INVALID_TEXT_INPUT"),
          "service returns NUL hold code");
}

void testControllerNullCharacter()
{
    QSqlDatabase database;
    HoldService service(database);
    HoldController controller(service);

    const QByteArray eventBody =
        "{\"eventType\":\"FULL_RELEASE\\u0000\","
        "\"reasonCode\":\"R\",\"idempotencyKey\":\"k\"}";
    const HttpResponse eventResponse =
        controller.createEvent(1, eventBody);
    check(eventResponse.status == QByteArray("400 Bad Request"),
          "controller rejects NUL event input");
    check(eventResponse.body.contains("INVALID_TEXT_INPUT"),
          "controller returns NUL event code");

    const QByteArray holdBody =
        "{\"accountId\":1,\"amount\":\"1\","
        "\"reasonCode\":\"R\\u0000\",\"idempotencyKey\":\"k\"}";
    const HttpResponse holdResponse =
        controller.createHold(holdBody);
    check(holdResponse.status == QByteArray("400 Bad Request"),
          "controller rejects NUL hold input");
    check(holdResponse.body.contains("INVALID_TEXT_INPUT"),
          "controller returns NUL hold code");
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    testTrim();
    testCanonicalAmount();
    testTimestamp();
    testUtf8LengthsAndTrimmedFingerprint();
    testMetadataClassification();
    testFingerprintAmountIdentity();
    testHoldLegacyFingerprintCompatibility();
    testTestVectors();
    testClassification();
    testServiceNullCharacter();
    testControllerNullCharacter();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All hold event fingerprint and validation tests passed\n";
    return 0;
}
