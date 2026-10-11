#ifndef IBRANCHREPOSITORY_H
#define IBRANCHREPOSITORY_H

#include "../../domain/branch.h"

#include <QString>
#include <QVector>

class IBranchRepository
{
public:
    enum class SaveResult
    {
        Success,
        Conflict,
        CityNotFound,
        DatabaseError
    };

    enum class FindResult
    {
        Found,
        NotFound,
        DatabaseError
    };

    enum class ListResult
    {
        Success,
        DatabaseError
    };

    enum class UpdateResult
    {
        Success,
        NotFound,
        BranchCodeConflict,
        CityNotFound,
        DatabaseError
    };

    enum class StatusResult
    {
        Success,
        NotFound,
        DatabaseError
    };

    virtual ~IBranchRepository() = default;

    virtual SaveResult save(Branch& branch) = 0;

    virtual FindResult findById(
        long long id,
        Branch& branch
    ) = 0;

    virtual ListResult findAll(
        QVector<Branch>& branches
    ) = 0;

    virtual UpdateResult update(
        Branch& branch
    ) = 0;

    virtual StatusResult updateStatus(
        long long id,
        const QString& status,
        Branch& branch
    ) = 0;
};

#endif // IBRANCHREPOSITORY_H
