#ifndef BRANCHSERVICE_H
#define BRANCHSERVICE_H

#include "../domain/branch.h"
#include "../infrastructure/repository/ibranchrepository.h"
#include <QVector>

class BranchService
{
public:
    enum class CreateResult
    {
        Success,
        InvalidInput,
        Conflict,
        CityNotFound,
        InternalError
    };

    enum class FindResult
    {
        Found,
        NotFound,
        InternalError
    };

    enum class ListResult
    {
        Success,
        InternalError
    };

    explicit BranchService(
        IBranchRepository& branchRepository
    );

    CreateResult createBranch(
        const QString& branchCode,
        const QString& name,
        long long cityId,
        const QString& status,
        Branch& createdBranch
    );

    FindResult findBranchById(
        long long id,
        Branch& branch
    );

    ListResult findAllBranches(
        QVector<Branch>& branches
    );

private:
    bool isValidBranch(
        const QString& branchCode,
        const QString& name,
        long long cityId,
        const QString& status
    ) const;

    IBranchRepository& branchRepository;
};

#endif // BRANCHSERVICE_H
