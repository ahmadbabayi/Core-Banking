#include "accountnumbergenerator.h"

namespace
{

int calculateCheckDigit(
const QString& number
)
{
int check = 5;

for (const QChar character : number)
{
    const int digit =
        character.digitValue();

    if (digit < 0)
    {
        return -1;
    }

    int value = check;

    if (value == 0)
    {
        value = 10;
    }

    value *= 2;

    value %= 11;

    value += digit;

    value %= 10;

    check = value;
}

return (10 - check) % 10;

}

bool isDigits(
const QString& value
)
{
if (value.isEmpty())
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

QString AccountNumberGenerator::generateCheckDigit(
const QString& number
)
{
if (number.length() != 14)
{
return QString();
}

if (!isDigits(number))
{
    return QString();
}

const int checkDigit =
    calculateCheckDigit(number);

if (checkDigit < 0)
{
    return QString();
}

return QString::number(checkDigit);

}

QString AccountNumberGenerator::generateAccountNumber(
const QString& accountTypeCode,
const QString& serial,
const QString& currencyCode
)
{
if (accountTypeCode.length() != 2 ||
!isDigits(accountTypeCode))
{
return QString();
}

if (serial.length() != 10 ||
    !isDigits(serial))
{
    return QString();
}

if (currencyCode.length() != 2 ||
    !isDigits(currencyCode))
{
    return QString();
}

const QString baseNumber =
    accountTypeCode +
    serial +
    currencyCode;

const QString checkDigit =
    generateCheckDigit(baseNumber);

if (checkDigit.isEmpty())
{
    return QString();
}

return baseNumber + checkDigit;

}

bool AccountNumberGenerator::validate(
const QString& accountNumber
)
{
if (accountNumber.length() != 15)
{
return false;
}

if (!isDigits(accountNumber))
{
    return false;
}

const QString baseNumber =
    accountNumber.left(14);

const QString expectedCheckDigit =
    generateCheckDigit(baseNumber);

return expectedCheckDigit ==
       accountNumber.right(1);

}
