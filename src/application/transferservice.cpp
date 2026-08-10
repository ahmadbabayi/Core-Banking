#include "transferservice.h"

TransferService::TransferService(
    IAccountRepository& accountRepository,
    ITransactionRepository& transactionRepository,
    ITransferRepository& transferRepository
)
    : accountRepository(accountRepository),
      transactionRepository(transactionRepository),
      transferRepository(transferRepository),
      nextTransactionId(1000),
      nextTransferId(1)
{
}

bool TransferService::transfer(
    qint64 sourceAccountId,
    qint64 destinationAccountId,
    qint64 amount,
    const QString& description)
{
    // 1. مبلغ انتقال باید مثبت باشد
    if (amount <= 0)
        return false;

    // 2. حساب مبدأ و مقصد نباید یکی باشند
    if (sourceAccountId == destinationAccountId)
        return false;

    // 3. پیدا کردن حساب مبدأ
    Account* sourceAccount =
        accountRepository.findById(sourceAccountId);

    if (sourceAccount == nullptr)
        return false;

    // 4. پیدا کردن حساب مقصد
    Account* destinationAccount =
        accountRepository.findById(destinationAccountId);

    if (destinationAccount == nullptr)
        return false;

    // 5. ایجاد Transfer با وضعیت Pending
    Transfer transfer(
        nextTransferId++,
        sourceAccountId,
        destinationAccountId,
        amount,
        description
    );

    // 6. برداشت از حساب مبدأ
    if (!sourceAccount->withdraw(amount))
    {
        transfer.fail();
        return false;
    }

    // 7. واریز به حساب مقصد
    destinationAccount->deposit(amount);

    // 8. ذخیره حساب مبدأ
    if (!accountRepository.save(*sourceAccount))
    {
        transfer.fail();
        return false;
    }

    // 9. ذخیره حساب مقصد
    if (!accountRepository.save(*destinationAccount))
    {
        transfer.fail();
        return false;
    }

    // 10. ایجاد Transaction برای حساب مبدأ
    Transaction withdrawalTransaction(
        nextTransactionId++,
        sourceAccountId,
        Transaction::Type::Withdrawal,
        amount,
        description
    );

    // 11. ذخیره Transaction مبدأ
    if (!transactionRepository.save(withdrawalTransaction))
    {
        transfer.fail();
        return false;
    }

    // 12. ایجاد Transaction برای حساب مقصد
    Transaction depositTransaction(
        nextTransactionId++,
        destinationAccountId,
        Transaction::Type::Deposit,
        amount,
        description
    );

    // 13. ذخیره Transaction مقصد
    if (!transactionRepository.save(depositTransaction))
    {
        transfer.fail();
        return false;
    }

    // 14. انتقال با موفقیت انجام شد
    transfer.complete();

    // 15. ذخیره Transfer
    if (!transferRepository.save(transfer))
    {
        return false;
    }

    return true;
}
