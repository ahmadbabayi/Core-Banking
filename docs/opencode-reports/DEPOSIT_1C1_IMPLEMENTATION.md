# گزارش پیاده‌سازی زیرمرحله 1C.1 — استخراج ساختاری Journal/Ledger از Deposit

تاریخ: 2026-11-13
وضعیت: پیاده‌سازی، Build و آزمون هدفمند بدون PostgreSQL انجام شد. این گزارش تکمیل کل 1B/1C یا رفع race را ادعا نمی‌کند.

## ۱. خلاصه اجرا

- SQLهای Journal/Ledger از `DepositService` خارج شدند.
- قرارداد `IJournalRepository` در Application و پیاده‌سازی آن در Infrastructure اضافه/به‌روزرسانی شد.
- `journals()` به قرارداد `ITransactionUnitOfWork` و پیاده‌سازی `UnitOfWork` اضافه شد.
- Repository جدید به همان `QSqlDatabase` و همان UoW/تراکنش متصل است؛ Connection یا transaction مستقل ساخته نشد.
- SQL، bindها، ترتیب عملیات و قفل‌ها، `CURRENT_DATE`/`CURRENT_TIMESTAMP`، ترتیب debit/credit و precision NUMERIC حفظ شدند.
- تصمیم‌ها و اعتبارسنجی‌های Use Case در Deposit باقی ماندند.
- Balance، Transaction/Idempotency، fingerprint، replay، conflict و retry تغییر نکردند.
- F1/F2، `CommitOutcomeUnknown`، عدم rollback بعد از commit نامعلوم و fail-closed حفظ شدند.
- تغییرات 1A.1 و 1B.1 overwrite یا revert نشدند.
- Git فقط read-only بررسی شد؛ stage/commit/push انجام نشد.

## ۲. فایل‌های تغییر‌یافته

### ایجادشده

- `src/application/contracts/ijournalrepository.h`
- `tests/journal_repository_unitofwork_test.cpp`

### تغییر‌یافته

- `src/infrastructure/repository/journalrepository.h`
- `src/infrastructure/repository/journalrepository.cpp`
- `src/application/contracts/itransactionunitofwork.h`
- `src/infrastructure/unitofwork.h`
- `src/infrastructure/unitofwork.cpp`
- `src/application/depositservice.h`
- `src/application/depositservice.cpp`
- `CMakeLists.txt` برای افزودن قرارداد جدید و حذف مسیر قرارداد قدیمی
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`

### حذف‌شده

- `src/infrastructure/repository/ijournalrepository.h` — قرارداد به Application منتقل شد.

`JournalRepository` موجود Infrastructure برای پیاده‌سازی قرارداد جدید استفاده شد. متدهای CRUD عمومی قبلی آن هیچ مصرف‌کننده فعالی نداشتند و با عملیات واقعی Deposit جایگزین شدند. این جایگزینی در مسیر فعال پروژه رفتار قابل مشاهده‌ای تغییر نداد.

## ۳. تغییرات ساختاری

### قرارداد Application

`IJournalRepository` فقط عملیات واقعی Deposit را منتشر می‌کند:

1. lookup سرفصل تسویه با `FOR SHARE`
2. مقداردهی اولیه `ledger_account_balance`
3. قفل ردیف‌های `ledger_account_balance`
4. ایجاد Journal پیش‌نویس
5. درج دو سطر Journal واریز
6. خواندن تعداد و توازن سطرهای Journal
7. به‌روزرسانی تجمعی Ledger
8. posting مشروط Journal

قرارداد هیچ `QSqlDatabase`، `QSqlQuery`، SQL یا متن خام خطای دیتابیس را منتشر نمی‌کند و از `OperationError` امن استفاده می‌کند.

### پیاده‌سازی Infrastructure

`JournalRepository` همان SQLهای قبلی Deposit را اجرا می‌کند. متن SQL، bindها و ترتیب آن‌ها تغییر نکردند.

### اتصال به UnitOfWork

- `journals()` به `ITransactionUnitOfWork` اضافه شد.
- `UnitOfWork` یک `JournalRepository` با همان `QSqlDatabase` خود می‌سازد.
- `DepositService` از `unitOfWork->journals()` استفاده می‌کند؛ نه Connection جدید و نه transaction مستقل.
- `begin/commit/rollback` فقط تحت مدیریت UoW باقی ماندند.

### ترتیب حفظ‌شده

Transaction create → Account/Balance/Hold locks → Snapshot → Settlement lookup → Ledger init/lock → TransactionEntry → Journal create/entries/check → Balance update → Ledger update → Journal post → Transaction complete.

## ۴. SQLهای باقی‌مانده در Deposit

بعد از این مرحله فقط این SQLها در Deposit باقی‌ماندند:

- `SELECT currency_id FROM account WHERE id = :id`
- `SELECT id FROM account WHERE id = :account_id FOR UPDATE`
- `SELECT id FROM account_hold WHERE account_id = :account_id ORDER BY id FOR UPDATE`

به همین دلیل `QSqlDatabase` انتقالی و includeهای Qt SQL در Deposit حذف نشدند. این SQLها عمداً پنهان نشدند و به Repository نامرتبط منتقل نشدند.

## ۵. حفظ SQL و bindها

مقایسه خودکار SQL و ترتیب bindها با نسخه پیش از تغییر انجام شد. نتیجه برای هر هشت بخش `sql=True` و `bind_order=True` بود:

| بخش | طول SQL | bindها |
|---|---|---|
| Settlement ledger lookup | ۱۶۹ | `:code`, `:currency_id` |
| Ledger balance init | ۳۶۱ | `:customer_ledger`, `:settlement_ledger` |
| Ledger balance lock | ۱۹۰ | `:customer_ledger`, `:settlement_ledger` |
| Journal create | ۱۷۴ | `:transaction_id`, `:description` |
| Journal entries | ۳۵۲ | `:journal_id`, `:settlement_ledger`, `:customer_ledger`, `:currency_id`, `:debit_amount`, `:credit_amount`, `:account_id`, `:cash_description`, `:customer_description` |
| Journal balance read | ۱۴۵ | `:journal_id` |
| Ledger update | ۹۷۷ | `:settlement_ledger`, `:customer_ledger`, `:cash_debit`, `:customer_credit` |
| Journal post | ۴۰۲ | `:journal_id` |

فایل مقایسه دائمی ایجاد نشد؛ این بررسی بخشی از شواهد همین مرحله است.

## ۶. تغییر پیام‌های خطا

کدهای API و شاخه‌های کنترل حفظ شدند. متن خام جایگزین‌شده:

| کد API | متن قبلی | متن فعلی |
|---|---|---|
| `SETTLEMENT_LEDGER_LOOKUP_FAILED` | متن خام `QSqlError::text()` | `Settlement ledger lookup failed` |
| `LEDGER_BALANCE_INIT_FAILED` | متن خام `QSqlError::text()` | `Ledger balance initialization failed` |
| `LEDGER_BALANCE_LOCK_FAILED` | متن خام `QSqlError::text()` | `Ledger balance rows could not be locked` |
| `LEDGER_BALANCE_LOCK_FAILED` (تعداد ناکافی) | `Could not lock all ledger balance rows` | بدون تغییر |
| `JOURNAL_CREATE_FAILED` | متن خام `QSqlError::text()` | `Journal could not be created` |
| `JOURNAL_ENTRY_CREATE_FAILED` | متن خام `QSqlError::text()` | `Journal entries could not be created` |
| `JOURNAL_BALANCE_CHECK_FAILED` | متن خام `QSqlError::text()` | `Journal balance check failed` |
| `JOURNAL_NOT_BALANCED` | `Journal debit and credit totals do not match` | بدون تغییر |
| `LEDGER_BALANCE_PREPARE_FAILED` | متن خام `QSqlError::text()` | `Ledger balance update could not be prepared` |
| `LEDGER_BALANCE_UPDATE_FAILED` | متن خام `QSqlError::text()` | `Ledger balance update failed` |
| `JOURNAL_POST_FAILED` (خطای اجرا) | متن خام `QSqlError::text()` | `Journal could not be posted` |
| `JOURNAL_POST_FAILED` (رد شدن) | متن خام یا fallback | `Journal is unbalanced or could not be posted` |

هیچ جزئیات حساس جدیدی لاگ نشد.

## ۷. Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق.

```text
-- Configuring done
-- Generating done
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/repository/journalrepository.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[  6%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  8%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  9%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/depositcontroller.cpp.o
[ 10%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

CMake به‌دلیل تغییر فهرست فایل‌ها یک بار خودکار Reconfigure شد. دستور `cmake -S` جداگانه اجرا نشد.

## ۸. آزمون هدفمند

فایل آزمون در مسیر استاندارد `tests/journal_repository_unitofwork_test.cpp` نگهداری می‌شود.

دستور:

```bash
g++ -std=c++17 -fPIC -I. tests/journal_repository_unitofwork_test.cpp \
  src/infrastructure/unitofwork.cpp \
  src/infrastructure/repository/journalrepository.cpp \
  src/infrastructure/repository/balancerepository.cpp \
  src/infrastructure/repository/transactionrepository.cpp \
  src/domain/transaction.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/journal_repository_unitofwork_test

/tmp/journal_repository_unitofwork_test
```

خروجی واقعی:

```text
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
All journal repository and unit-of-work tests passed
```

هشدارهای `database not open` مربوط به آزمون عمدی اتصال بسته برای بررسی پیام امن هستند؛ assertionها موفق بودند.

### پوشش آزمون

- `UnitOfWorkFactory`/`UnitOfWork` واقعی با دیتابیس حافظه‌ای `QSQLITE`.
- `journals()` از همان scope و Connection است و از `transactions()`/`balances()` جداست.
- ایجاد Journal پیش‌نویس، درج دو سطر، خواندن تعداد/توازن و rollback بدون باقی‌ماندن ردیف.
- ایجاد و commit دومین Journal و دو سطر آن.
- اتصال بسته و بررسی پیام‌های امن برای lookup، init، lock، create، entries، balance read، ledger update و post.

### محدودیت صریح آزمون

- موفقیت `postIfBalanced` روی `QSQLITE` اجرا نشد، چون SQL آن از alias در `UPDATE` استفاده می‌کند که PostgreSQL-specific است.
- موفقیت `ensureLedgerBalanceRows` و `applyLedgerDeltas` روی `QSQLITE` اجرا نشد، چون SQL آن‌ها از `unnest(ARRAY[...])`، CTE و `ON CONFLICT` استفاده می‌کند.
- برای این سه عملیات فقط مسیر خطای امن با اتصال بسته آزموده شد.
- آزمون PostgreSQL واقعی اجرا نشد. `QSQLITE` اثبات سازگاری PostgreSQL نیست.
- HTTP، Integration/Concurrency واقعی و Fault Injection واقعی اجرا نشد.

## ۹. داده‌ها

- هیچ داده‌ای در دیتابیس `corebanking` ایجاد یا تغییر نکرد.
- هیچ SQL واقعی روی PostgreSQL اجرا نشد.
- هیچ شناسه کسب‌وکاری جدیدی تولید نشد.
- آزمون فقط از دیتابیس حافظه‌ای `QSQLITE` استفاده کرد.

## ۱۰. بدهی‌ها و محدودیت‌های باقی‌مانده

- Account currency lookup، Account row lock و Hold row lock همچنان در `DepositService` و روی `QSqlDatabase` انتقالی هستند.
- Journal invariants جدید، بازطراحی قواعد Journal و رفع race قواعد مؤثر Balance انجام نشد.
- زمان مرجع، isolation، advisory locks و روش محاسبات پولی تغییر نکردند.
- Transfer، HoldService و مسیرهای نوشتن Hold تغییر نکردند.
- `available_balance` همچنان Cache کهنه‌شونده است.
- آزمون PostgreSQL واقعی، HTTP، Integration/Concurrency واقعی و Fault Injection واقعی اجرا نشد.
- این مرحله تکمیل کل 1B/1C، رفع race یا آمادگی عملیاتی بانکی نیست.

## ۱۱. پیشنهاد Commit

پیام پیشنهادی (اجرا نشده است):

```text
refactor(journal): extract Deposit journal and ledger persistence
```
