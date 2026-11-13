#ifndef ITRANSACTIONUNITOFWORK_H
#define ITRANSACTIONUNITOFWORK_H

#include "iaccountstaterepository.h"
#include "ibalancerepository.h"
#include "iholdrepository.h"
#include "ijournalrepository.h"
#include "itransactionrepository.h"
#include "operationerror.h"

#include <memory>

enum class BeginOutcome
{
    Started,
    AlreadyStarted,
    ConnectionUnavailable,
    Failed
};

enum class CommitOutcome
{
    Committed,
    OutcomeUnknown,
    NotActive
};

enum class RollbackOutcome
{
    RolledBack,
    Failed,
    NotActive,
    UnsafeState
};

class ITransactionUnitOfWork
{
public:
    virtual ~ITransactionUnitOfWork() = default;

    virtual BeginOutcome begin(OperationError* error) = 0;
    virtual CommitOutcome commit(OperationError* error) = 0;
    virtual RollbackOutcome rollback(OperationError* error) = 0;

    virtual ITransactionRepository& transactions() = 0;
    virtual IBalanceRepository& balances() = 0;
    virtual IJournalRepository& journals() = 0;
    virtual IAccountStateRepository& accounts() = 0;
    virtual IHoldRepository& holds() = 0;
};

class ITransactionUnitOfWorkFactory
{
public:
    virtual ~ITransactionUnitOfWorkFactory() = default;

    virtual bool isConnectionAvailable() const = 0;
    virtual std::unique_ptr<ITransactionUnitOfWork> create() = 0;
};

#endif // ITRANSACTIONUNITOFWORK_H
