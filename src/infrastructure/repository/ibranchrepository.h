#ifndef IBRANCHREPOSITORY_H
#define IBRANCHREPOSITORY_H

#include "../../domain/branch.h"
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

    virtual ~IBranchRepository() = default;

    virtual SaveResult save(Branch& branch) = 0;

    virtual FindResult findById(
        long long id,
        Branch& branch
    ) = 0;

    virtual ListResult findAll(
        QVector<Branch>& branches
    ) = 0;
};

#endif // IBRANCHREPOSITORY_H
