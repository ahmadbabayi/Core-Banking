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

    enum class UpdateResult
    {
        Success,
        InvalidInput,
        Conflict,
        NotFound,
        CityNotFound,
        InternalError
    };

    enum class ChangeStatusResult
    {
        Success,
        InvalidStatus,
        NotFound,
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

    UpdateResult updateBranch(
        long long id,
        const QString& branchCode,
        const QString& name,
        long long cityId,
        Branch& updatedBranch
    );

    ChangeStatusResult changeBranchStatus(
        long long id,
        const QString& status,
        Branch& updatedBranch
    );

private:
    bool isValidBranch(
        const QString& branchCode,
        const QString& name,
        long long cityId,
        const QString& status
    ) const;

    bool isValidStatus(
        const QString& status
    ) const;

    IBranchRepository& branchRepository;
};

#endif // BRANCHSERVICE_H
