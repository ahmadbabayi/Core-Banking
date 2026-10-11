#include "branchservice.h"

#include <QRegularExpression>

BranchService::BranchService(
    IBranchRepository& branchRepository
)
    : branchRepository(branchRepository)
{
}

bool BranchService::isValidStatus(
    const QString& status
) const
{
    return status == "ACTIVE" ||
           status == "INACTIVE" ||
           status == "CLOSED";
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

    if (!isValidStatus(status))
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

BranchService::UpdateResult
BranchService::updateBranch(
    long long id,
    const QString& branchCode,
    const QString& name,
    long long cityId,
    Branch& updatedBranch
)
{
    if (id <= 0)
    {
        return UpdateResult::NotFound;
    }

    const QString normalizedCode = branchCode.trimmed();
    const QString normalizedName = name.trimmed();

    static const QRegularExpression codePattern(
        "^[0-9]{7}$"
    );

    if (!codePattern.match(normalizedCode).hasMatch() ||
        normalizedName.isEmpty() ||
        normalizedName.size() > 150 ||
        cityId <= 0)
    {
        return UpdateResult::InvalidInput;
    }

    Branch existingBranch;

    const IBranchRepository::FindResult findResult =
        branchRepository.findById(id, existingBranch);

    switch (findResult)
    {
    case IBranchRepository::FindResult::NotFound:
        return UpdateResult::NotFound;

    case IBranchRepository::FindResult::DatabaseError:
        return UpdateResult::InternalError;

    case IBranchRepository::FindResult::Found:
        break;
    }

    existingBranch.setBranchCode(normalizedCode);
    existingBranch.setName(normalizedName);
    existingBranch.setCityId(cityId);

    const IBranchRepository::UpdateResult updateResult =
        branchRepository.update(existingBranch);

    switch (updateResult)
    {
    case IBranchRepository::UpdateResult::Success:
        updatedBranch = existingBranch;
        return UpdateResult::Success;

    case IBranchRepository::UpdateResult::NotFound:
        return UpdateResult::NotFound;

    case IBranchRepository::UpdateResult::BranchCodeConflict:
        return UpdateResult::Conflict;

    case IBranchRepository::UpdateResult::CityNotFound:
        return UpdateResult::CityNotFound;

    case IBranchRepository::UpdateResult::DatabaseError:
        return UpdateResult::InternalError;
    }

    return UpdateResult::InternalError;
}

BranchService::ChangeStatusResult
BranchService::changeBranchStatus(
    long long id,
    const QString& status,
    Branch& updatedBranch
)
{
    if (id <= 0)
    {
        return ChangeStatusResult::NotFound;
    }

    const QString normalizedStatus = status.trimmed();

    if (!isValidStatus(normalizedStatus))
    {
        return ChangeStatusResult::InvalidStatus;
    }

    const IBranchRepository::StatusResult result =
        branchRepository.updateStatus(
            id,
            normalizedStatus,
            updatedBranch
        );

    switch (result)
    {
    case IBranchRepository::StatusResult::Success:
        return ChangeStatusResult::Success;

    case IBranchRepository::StatusResult::NotFound:
        return ChangeStatusResult::NotFound;

    case IBranchRepository::StatusResult::DatabaseError:
        return ChangeStatusResult::InternalError;
    }

    return ChangeStatusResult::InternalError;
}
