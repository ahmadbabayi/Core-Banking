#ifndef ACCOUNTNUMBERGENERATOR_H
#define ACCOUNTNUMBERGENERATOR_H

#include <QString>

class AccountNumberGenerator
{
public:

static QString generateCheckDigit(
    const QString& number
);

static QString generateAccountNumber(
    const QString& accountTypeCode,
    const QString& serial,
    const QString& currencyCode
);

static bool validate(
    const QString& accountNumber
);

};

#endif // ACCOUNTNUMBERGENERATOR_H
