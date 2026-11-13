#ifndef UNITOFWORK_H
#define UNITOFWORK_H

#include "../application/contracts/itransactionunitofwork.h"

#include <QSqlDatabase>
#include <QString>

#include <memory>

class TransactionRepository;
class BalanceRepository;
class JournalRepository;
class AccountStateRepository;
class HoldRepository;

class UnitOfWork : public ITransactionUnitOfWork
{
public:
    explicit UnitOfWork(const QSqlDatabase& database);
    ~UnitOfWork() override;

    UnitOfWork(const UnitOfWork&) = delete;
    UnitOfWork& operator=(const UnitOfWork&) = delete;

    BeginOutcome begin(OperationError* error) override;
    CommitOutcome commit(OperationError* error) override;
    RollbackOutcome rollback(OperationError* error) override;

    ITransactionRepository& transactions() override;
    IBalanceRepository& balances() override;
    IJournalRepository& journals() override;
    IAccountStateRepository& accounts() override;
    IHoldRepository& holds() override;

    bool isActive() const;
    bool isCommitOutcomeUnknown() const;

private:
    enum class State
    {
        NotStarted,
        Active,
        Committed,
        RolledBack,
        RollbackFailed,
        CommitOutcomeUnknown
    };

    void setError(
        OperationError* error,
        PersistenceErrorCode code,
        const QString& message) const;

    QSqlDatabase m_database;
    std::unique_ptr<TransactionRepository> m_transactionRepository;
    std::unique_ptr<BalanceRepository> m_balanceRepository;
    std::unique_ptr<JournalRepository> m_journalRepository;
    std::unique_ptr<AccountStateRepository> m_accountStateRepository;
    std::unique_ptr<HoldRepository> m_holdRepository;
    State m_state = State::NotStarted;
    bool m_closeConnectionOnDestruct = false;
};

class UnitOfWorkFactory : public ITransactionUnitOfWorkFactory
{
public:
    explicit UnitOfWorkFactory(const QSqlDatabase& database);

    bool isConnectionAvailable() const override;
    std::unique_ptr<ITransactionUnitOfWork> create() override;

private:
    QSqlDatabase m_database;
};

#endif // UNITOFWORK_H
