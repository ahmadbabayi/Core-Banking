#ifndef BRANCHCONTROLLER_H
#define BRANCHCONTROLLER_H

#include "../application/branchservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>

class BranchController
{
public:
    explicit BranchController(
        BranchService& branchService
    );

    HttpResponse createBranch(
        const QByteArray& body
    );

    HttpResponse updateBranch(
        long long branchId,
        const QByteArray& body
    );

    HttpResponse changeBranchStatus(
        long long branchId,
        const QByteArray& body
    );

    HttpResponse getBranchById(
        long long branchId
    );

    HttpResponse getAllBranches();

private:
    QJsonObject branchToJson(
        const Branch& branch
    ) const;

    BranchService& branchService;
};

#endif // BRANCHCONTROLLER_H
