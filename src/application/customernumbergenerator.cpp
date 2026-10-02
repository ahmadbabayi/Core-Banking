#include "customernumbergenerator.h"

#include <QRandomGenerator>

namespace
{
constexpr quint64 MaximumCustomerNumber = 9999999999ULL;

bool isTenDigits(const QString& value)
{
    if (value.length() != 10)
    {
        return false;
    }

    for (const QChar character : value)
    {
        if (!character.isDigit())
        {
            return false;
        }
    }

    return true;
}
}

QString CustomerNumberGenerator::generate()
{
    const quint64 number =
        QRandomGenerator::global()->generate64()
        % MaximumCustomerNumber;

    const quint64 normalizedNumber =
        number + 1;

    const QString customerNumber =
        QString("%1")
            .arg(
                normalizedNumber,
                10,
                10,
                QChar('0')
            );

    if (!isTenDigits(customerNumber))
    {
        return QString();
    }

    return customerNumber;
}
