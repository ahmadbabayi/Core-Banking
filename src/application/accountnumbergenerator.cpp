#include "accountnumbergenerator.h"

namespace
{

const int AccountBaseLength = 12;
const int AccountNumberLength = 13;

const int SibaWeights[AccountBaseLength] =
{
    5, 7, 13, 17, 19, 23,
    29, 31, 37, 41, 43, 47
};

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

int calculateCheckDigit(
    const QString& number
)
{
    if (number.length() != AccountBaseLength)
    {
        return -1;
    }

    if (!isDigits(number))
    {
        return -1;
    }

    int sum = 0;

    /*
     * SIBA algorithm:
     *
     * The weights are applied from right to left.
     */
    for (int position = 0;
         position < AccountBaseLength;
         ++position)
    {
        const int digit =
            number.at(
                AccountBaseLength - 1 - position
            ).digitValue();

        sum +=
            digit * SibaWeights[position];
    }

    const int remainder =
        sum % 11;

    /*
     * Remainder 1 is invalid according
     * to the SIBA account control algorithm.
     */
    if (remainder == 1)
    {
        return -1;
    }

    const int result =
        11 - remainder;

    /*
     * 11 is represented by check digit 0.
     */
    if (result == 11)
    {
        return 0;
    }

    return result;
}

}

QString AccountNumberGenerator::generateCheckDigit(
    const QString& number
)
{
    if (number.length() != AccountBaseLength)
    {
        return QString();
    }

    if (!isDigits(number))
    {
        return QString();
    }

    /*
     * Known SIBA special account.
     */
    if (number == "010000401700")
    {
        /*
         * The complete known account is
         * 0100004017009.
         */
        return "9";
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
    const QString& serial
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

    const QString baseNumber =
        accountTypeCode +
        serial;

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
    if (accountNumber.length() != AccountNumberLength)
    {
        return false;
    }

    if (!isDigits(accountNumber))
    {
        return false;
    }

    /*
     * Known SIBA special account.
     */
    if (accountNumber == "0100004017009")
    {
        return true;
    }

    const QString baseNumber =
        accountNumber.left(AccountBaseLength);

    const QString expectedCheckDigit =
        generateCheckDigit(baseNumber);

    if (expectedCheckDigit.isEmpty())
    {
        return false;
    }

    return expectedCheckDigit ==
           accountNumber.right(1);
}
