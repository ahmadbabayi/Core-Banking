#ifndef ACCOUNTREPOSITORY_H
#define ACCOUNTREPOSITORY_H

#include "iaccountrepository.h"

class AccountRepository : public IAccountRepository
{
public:
    bool save(const Account& account) override;

    Account* findById(qint64 id) override;
};

#endif // ACCOUNTREPOSITORY_H
