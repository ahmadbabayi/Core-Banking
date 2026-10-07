#include "accountservice.h"

#include "../infrastructure/repository/iaccountrepository.h"

#include <QSqlError>

AccountService::AccountService(
    IAccountRepository& accountRepository,
    const QSqlDatabase& database
)
    : accountRepository(accountRepository),
      db(database)
{
}

bool AccountService::activate(qint64 accountId)
{
    if (accountId <= 0)
    {
        return false;
    }

    if (!db.transaction())
    {
        return false;
    }

    Account* account = accountRepository.findByIdForUpdate(accountId);

    if (account == nullptr)
    {
        db.rollback();
        return false;
    }

    if (!account->activate())
    {
        delete account;
        db.rollback();
        return false;
    }

    const bool saved = accountRepository.save(*account);

    delete account;

    if (!saved)
    {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool AccountService::makeDormant(qint64 accountId)
{
    if (accountId <= 0)
    {
        return false;
    }

    if (!db.transaction())
    {
        return false;
    }

    Account* account = accountRepository.findByIdForUpdate(accountId);

    if (account == nullptr)
    {
        db.rollback();
        return false;
    }

    if (!account->makeDormant())
    {
        delete account;
        db.rollback();
        return false;
    }

    const bool saved = accountRepository.save(*account);

    delete account;

    if (!saved)
    {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool AccountService::block(qint64 accountId)
{
    if (accountId <= 0)
    {
        return false;
    }

    if (!db.transaction())
    {
        return false;
    }

    Account* account = accountRepository.findByIdForUpdate(accountId);

    if (account == nullptr)
    {
        db.rollback();
        return false;
    }

    if (!account->block())
    {
        delete account;
        db.rollback();
        return false;
    }

    const bool saved = accountRepository.save(*account);

    delete account;

    if (!saved)
    {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool AccountService::close(qint64 accountId)
{
    if (accountId <= 0)
    {
        return false;
    }

    if (!db.transaction())
    {
        return false;
    }

    Account* account = accountRepository.findByIdForUpdate(accountId);

    if (account == nullptr)
    {
        db.rollback();
        return false;
    }

    if (!account->close(QDateTime::currentDateTime()))
    {
        delete account;
        db.rollback();
        return false;
    }

    const bool saved = accountRepository.save(*account);

    delete account;

    if (!saved)
    {
        db.rollback();
        return false;
    }

    return db.commit();
}
