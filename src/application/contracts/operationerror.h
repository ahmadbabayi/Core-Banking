#ifndef OPERATIONERROR_H
#define OPERATIONERROR_H

#include <QString>

enum class PersistenceErrorCode
{
    None,
    NotFound,
    Conflict,
    ConstraintViolation,
    InvalidState,
    ConnectionUnavailable,
    QueryFailed,
    Unknown
};

struct OperationError
{
    PersistenceErrorCode code = PersistenceErrorCode::Unknown;
    QString message;

    bool isOk() const
    {
        return code == PersistenceErrorCode::None;
    }
};

#endif // OPERATIONERROR_H
