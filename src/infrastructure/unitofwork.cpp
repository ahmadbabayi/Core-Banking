#include "unitofwork.h"

#include <QDebug>
#include <QSqlError>

UnitOfWork::UnitOfWork(const QSqlDatabase& database)
    : m_database(database)
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

bool UnitOfWork::begin(QString* error)
{
    if (m_state != State::NotStarted)
    {
        setError(error, QStringLiteral("Unit of work has already been started"));
        return false;
    }

    if (!m_database.isValid() || !m_database.isOpen())
    {
        setError(error, QStringLiteral("Database connection is not open"));
        return false;
    }

    if (!m_database.transaction())
    {
        // Preserve the original error before any later cleanup can overwrite
        // the driver error. The connection was open, so a failed BEGIN leaves
        // it in an unsafe state; close it at the safe destructor boundary.
        setError(error, m_database.lastError().text());
        m_closeConnectionOnDestruct = true;
        return false;
    }

    m_state = State::Active;
    return true;
}

bool UnitOfWork::commit(QString* error)
{
    if (m_state != State::Active)
    {
        setError(error, QStringLiteral("Unit of work is not active"));
        return false;
    }

    if (!m_database.commit())
    {
        // A failed commit does not prove that the server rolled back. Keep the
        // unknown outcome explicit and do not attempt rollback afterwards.
        setError(error, m_database.lastError().text());
        m_state = State::CommitOutcomeUnknown;
        m_closeConnectionOnDestruct = true;
        return false;
    }

    m_state = State::Committed;
    return true;
}

bool UnitOfWork::rollback(QString* error)
{
    if (m_state == State::CommitOutcomeUnknown)
    {
        setError(error,
                 QStringLiteral("Commit outcome is unknown; rollback was not attempted"));
        m_closeConnectionOnDestruct = true;
        return false;
    }

    if (m_state == State::RollbackFailed)
    {
        setError(error,
                 QStringLiteral("Rollback already failed; retry was not attempted"));
        m_closeConnectionOnDestruct = true;
        return false;
    }

    if (m_state != State::Active)
    {
        setError(error, QStringLiteral("Unit of work is not active"));
        return false;
    }

    if (!m_database.rollback())
    {
        // Preserve the original error before the shared connection is closed.
        setError(error, m_database.lastError().text());
        m_state = State::RollbackFailed;
        m_closeConnectionOnDestruct = true;
        return false;
    }

    m_state = State::RolledBack;
    return true;
}

bool UnitOfWork::isActive() const
{
    return m_state == State::Active;
}

bool UnitOfWork::isCommitOutcomeUnknown() const
{
    return m_state == State::CommitOutcomeUnknown;
}

QSqlDatabase& UnitOfWork::database()
{
    return m_database;
}

void UnitOfWork::setError(QString* error, const QString& message) const
{
    if (error != nullptr)
        *error = message;
}
