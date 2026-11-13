#include "unitofwork.h"

#include "repository/accountstaterepository.h"
#include "repository/balancerepository.h"
#include "repository/holdrepository.h"
#include "repository/journalrepository.h"
#include "repository/transactionrepository.h"

#include <QDebug>

UnitOfWork::UnitOfWork(const QSqlDatabase& database)
    : m_database(database),
      m_transactionRepository(
          std::make_unique<TransactionRepository>(m_database)),
      m_balanceRepository(
          std::make_unique<BalanceRepository>(m_database)),
      m_journalRepository(
          std::make_unique<JournalRepository>(m_database)),
      m_accountStateRepository(
          std::make_unique<AccountStateRepository>(m_database)),
      m_holdRepository(
          std::make_unique<HoldRepository>(m_database))
{
}

UnitOfWork::~UnitOfWork()
{
    if (m_state == State::Active)
    {
        if (m_database.rollback())
        {
            m_state = State::RolledBack;
        }
        else
        {
            m_state = State::RollbackFailed;
            m_closeConnectionOnDestruct = true;
            qWarning()
                << "UnitOfWork: rollback failed during destruction;"
                << "closing shared database connection";
        }
    }

    if (m_closeConnectionOnDestruct ||
        m_state == State::CommitOutcomeUnknown ||
        m_state == State::RollbackFailed)
    {
        // Destructor runs after the per-request query scopes in Deposit have
        // ended, so local QSqlQuery objects are already destroyed here. This
        // is a scope contract, not a general claim that closing with live
        // queries is always safe.
        m_database.close();
    }
}

BeginOutcome UnitOfWork::begin(OperationError* error)
{
    if (m_state != State::NotStarted)
    {
        setError(
            error,
            PersistenceErrorCode::InvalidState,
            QStringLiteral("Unit of work has already been started"));
        return BeginOutcome::AlreadyStarted;
    }

    if (!m_database.isValid() || !m_database.isOpen())
    {
        setError(
            error,
            PersistenceErrorCode::ConnectionUnavailable,
            QStringLiteral("Database connection is not open"));
        return BeginOutcome::ConnectionUnavailable;
    }

    if (!m_database.transaction())
    {
        // Preserve a safe error before any later cleanup. The connection was
        // open, so a failed BEGIN leaves it in an unsafe state; close it at the
        // safe destructor boundary.
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Database transaction could not be started"));
        m_closeConnectionOnDestruct = true;
        return BeginOutcome::Failed;
    }

    m_state = State::Active;
    return BeginOutcome::Started;
}

CommitOutcome UnitOfWork::commit(OperationError* error)
{
    if (m_state != State::Active)
    {
        setError(
            error,
            PersistenceErrorCode::InvalidState,
            QStringLiteral("Unit of work is not active"));
        return CommitOutcome::NotActive;
    }

    if (!m_database.commit())
    {
        // A failed commit does not prove that the server rolled back. Keep the
        // unknown outcome explicit and do not attempt rollback afterwards.
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Database commit outcome is unknown"));
        m_state = State::CommitOutcomeUnknown;
        m_closeConnectionOnDestruct = true;
        return CommitOutcome::OutcomeUnknown;
    }

    m_state = State::Committed;
    return CommitOutcome::Committed;
}

RollbackOutcome UnitOfWork::rollback(OperationError* error)
{
    if (m_state == State::CommitOutcomeUnknown)
    {
        setError(
            error,
            PersistenceErrorCode::InvalidState,
            QStringLiteral("Commit outcome is unknown; rollback was not attempted"));
        m_closeConnectionOnDestruct = true;
        return RollbackOutcome::UnsafeState;
    }

    if (m_state == State::RollbackFailed)
    {
        setError(
            error,
            PersistenceErrorCode::InvalidState,
            QStringLiteral("Rollback already failed; retry was not attempted"));
        m_closeConnectionOnDestruct = true;
        return RollbackOutcome::UnsafeState;
    }

    if (m_state != State::Active)
    {
        setError(
            error,
            PersistenceErrorCode::InvalidState,
            QStringLiteral("Unit of work is not active"));
        return RollbackOutcome::NotActive;
    }

    if (!m_database.rollback())
    {
        // Preserve a safe error before the shared connection is closed.
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Database transaction could not be rolled back"));
        m_state = State::RollbackFailed;
        m_closeConnectionOnDestruct = true;
        return RollbackOutcome::Failed;
    }

    m_state = State::RolledBack;
    return RollbackOutcome::RolledBack;
}

ITransactionRepository& UnitOfWork::transactions()
{
    return *m_transactionRepository;
}

IBalanceRepository& UnitOfWork::balances()
{
    return *m_balanceRepository;
}

IJournalRepository& UnitOfWork::journals()
{
    return *m_journalRepository;
}

IAccountStateRepository& UnitOfWork::accounts()
{
    return *m_accountStateRepository;
}

IHoldRepository& UnitOfWork::holds()
{
    return *m_holdRepository;
}

bool UnitOfWork::isActive() const
{
    return m_state == State::Active;
}

bool UnitOfWork::isCommitOutcomeUnknown() const
{
    return m_state == State::CommitOutcomeUnknown;
}

void UnitOfWork::setError(
    OperationError* error,
    PersistenceErrorCode code,
    const QString& message) const
{
    if (error != nullptr)
    {
        error->code = code;
        error->message = message;
    }
}

UnitOfWorkFactory::UnitOfWorkFactory(const QSqlDatabase& database)
    : m_database(database)
{
}

bool UnitOfWorkFactory::isConnectionAvailable() const
{
    return m_database.isValid() && m_database.isOpen();
}

std::unique_ptr<ITransactionUnitOfWork> UnitOfWorkFactory::create()
{
    return std::make_unique<UnitOfWork>(m_database);
}
