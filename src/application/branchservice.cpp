#include "branchservice.h"

#include <QRegularExpression>

BranchService::BranchService(
    IBranchRepository& branchRepository
)
    : branchRepository(branchRepository)
{
}

bool BranchService::isValidBranch(
    const QString& branchCode,
    const QString& name,
    long long cityId,
    const QString& status
) const
{
    static const QRegularExpression codePattern(
        "^[0-9]{7}$"
    );

    if (!codePattern.match(branchCode).hasMatch())
    {
        return false;
    }

    if (name.isEmpty() || name.size() > 150)
    {
        return false;
    }

    if (cityId <= 0)
    {
        return false;
    }

    if (status != "ACTIVE" &&
        status != "INACTIVE" &&
        status != "CLOSED")
    {
        return false;
    }

    return true;
}

BranchService::CreateResult
BranchService::createBranch(
    const QString& branchCode,
    const QString& name,
    long long cityId,
    const QString& status,
    Branch& createdBranch
)
{
    const QString normalizedCode = branchCode.trimmed();
    const QString normalizedName = name.trimmed();
    const QString normalizedStatus = status.trimmed();

    if (!isValidBranch(
            normalizedCode,
            normalizedName,
            cityId,
            normalizedStatus))
    {
        return CreateResult::InvalidInput;
    }

    Branch branch(
        0,
        normalizedCode,
        normalizedName,
        cityId,
        normalizedStatus
    );

    const IBranchRepository::SaveResult result =
        branchRepository.save(branch);

    switch (result)
    {
    case IBranchRepository::SaveResult::Success:
        createdBranch = branch;
        return CreateResult::Success;

    case IBranchRepository::SaveResult::Conflict:
        return CreateResult::Conflict;

    case IBranchRepository::SaveResult::CityNotFound:
        return CreateResult::CityNotFound;

    case IBranchRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }

    return CreateResult::InternalError;
}

BranchService::FindResult
BranchService::findBranchById(
    long long id,
    Branch& branch
)
{
    if (id <= 0)
    {
        return FindResult::NotFound;
    }

    const IBranchRepository::FindResult result =
        branchRepository.findById(id, branch);

    switch (result)
    {
    case IBranchRepository::FindResult::Found:
        return FindResult::Found;

    case IBranchRepository::FindResult::NotFound:
        return FindResult::NotFound;

    case IBranchRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }

    return FindResult::InternalError;
}

BranchService::ListResult
BranchService::findAllBranches(
    QVector<Branch>& branches
)
{
    const IBranchRepository::ListResult result =
        branchRepository.findAll(branches);

    switch (result)
    {
    case IBranchRepository::ListResult::Success:
        return ListResult::Success;

    case IBranchRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }

    return ListResult::InternalError;
}
