#ifndef BRANCHREPOSITORY_H
#define BRANCHREPOSITORY_H

#include "ibranchrepository.h"

#include <QSqlDatabase>

class BranchRepository : public IBranchRepository
{
public:
    explicit BranchRepository(
        const QSqlDatabase& database
    );

    SaveResult save(Branch& branch) override;

    FindResult findById(
        long long id,
        Branch& branch
    ) override;

    ListResult findAll(
        QVector<Branch>& branches
    ) override;

    UpdateResult update(
        Branch& branch
    ) override;

    StatusResult updateStatus(
        long long id,
        const QString& status,
        Branch& branch
    ) override;

private:
    QSqlDatabase db;
};

#endif // BRANCHREPOSITORY_H
