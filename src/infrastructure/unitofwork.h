#ifndef UNITOFWORK_H
#define UNITOFWORK_H

#include <QSqlDatabase>
#include <QString>

class UnitOfWork
{
public:
    explicit UnitOfWork(const QSqlDatabase& database);
    ~UnitOfWork();

    UnitOfWork(const UnitOfWork&) = delete;
    UnitOfWork& operator=(const UnitOfWork&) = delete;

    bool begin(QString* error = nullptr);
    bool commit(QString* error = nullptr);
    bool rollback(QString* error = nullptr);

    bool isActive() const;
    bool isCommitOutcomeUnknown() const;

    QSqlDatabase& database();

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

    void setError(QString* error, const QString& message) const;

    QSqlDatabase m_database;
    State m_state = State::NotStarted;
    bool m_closeConnectionOnDestruct = false;
};

#endif // UNITOFWORK_H
