# گزارش بسته نهایی استخراج persistence از Deposit

تاریخ: 2026-11-13
وضعیت: پیاده‌سازی، Build و آزمون‌های هدفمند بدون PostgreSQL انجام شد. این گزارش تکمیل Domain rules، رفع race یا آمادگی عملیاتی بانکی را ادعا نمی‌کند.

## ۱. خلاصه اجرا

- سه SQL باقی‌مانده از `DepositService` خارج شدند:
  - Account currency lookup
  - Account row lock
  - Hold row lock
- این عملیات به قراردادهای Application و پیاده‌سازی‌های Infrastructure متناسب با مسئولیتشان منتقل شدند:
  - `IAccountStateRepository` / `AccountStateRepository`
  - `IHoldRepository` / `HoldRepository`
- `accounts()` و `holds()` به `ITransactionUnitOfWork` اضافه شدند و از همان UoW، همان Connection و همان تراکنش استفاده می‌کنند.
- `DepositService` دیگر هیچ SQL اجرایی، `QSqlDatabase`، `QSqlQuery`، `QSqlError` یا include مستقیم Infrastructure ندارد.
- برای حفظ `DATABASE_UNAVAILABLE` و ترتیب کنترل قبلی، `ITransactionUnitOfWorkFactory::isConnectionAvailable()` اضافه شد.
- ترتیب کامل مراحل Deposit و قفل‌ها، fingerprint/canonicalization/replay/conflict/retry، F1/F2، `CommitOutcomeUnknown`، عدم rollback پس از commit نامعلوم و fail-closed حفظ شدند.
- تغییرات 1A.1، 1B.1 و 1C.1 overwrite یا revert نشدند.
- Git فقط read-only بررسی شد؛ stage/commit/push انجام نشد.

## ۲. فایل‌های تغییر‌یافته

### ایجادشده

- `src/application/contracts/iaccountstaterepository.h`
- `src/application/contracts/iholdrepository.h`
- `src/infrastructure/repository/accountstaterepository.h`
- `src/infrastructure/repository/accountstaterepository.cpp`
- `src/infrastructure/repository/holdrepository.h`
- `src/infrastructure/repository/holdrepository.cpp`
- `tests/deposit_orchestration_test.cpp`
- `tests/account_hold_persistence_test.cpp`

### تغییر‌یافته

- `src/application/contracts/itransactionunitofwork.h`
- `src/infrastructure/unitofwork.h`
- `src/infrastructure/unitofwork.cpp`
- `src/application/depositservice.h`
- `src/application/depositservice.cpp`
- `src/main.cpp` برای wiring جدید Composition Root
- `CMakeLists.txt` برای قراردادها و Repositoryهای جدید
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`

## ۳. تغییرات ساختاری

### Account

`IAccountStateRepository` دو عملیات واقعی Deposit را منتشر می‌کند:

1. `findCurrencyById(...)` با نتیجه `Found`/`NotFound`/`Failed`
2. `lockAccountRow(...)` با نتیجه `Locked`/`Failed`

`AccountStateRepository` همان SQL و bind قبلی را اجرا می‌کند. وضعیت «ردیف پیدا نشد» از خطای persistence جدا باقی مانده است.

### Hold

`IHoldRepository` فقط `lockAccountHolds(...)` را منتشر می‌کند. `HoldRepository` همان SQL قبلی را با همان bind و ترتیب اجرا می‌کند. HoldService و مسیرهای نوشتن Hold تغییر نکردند.

### UnitOfWork

- `accounts()` و `holds()` به قرارداد `ITransactionUnitOfWork` اضافه شدند.
- `UnitOfWork` هر دو Repository را با همان `QSqlDatabase` و همان scope می‌سازد.
- `begin/commit/rollback` فقط تحت مدیریت UoW باقی ماندند.
- `ITransactionUnitOfWorkFactory::isConnectionAvailable()` اضافه شد تا Deposit بدون وابستگی Qt SQL همچنان بتواند `DATABASE_UNAVAILABLE` را در همان ترتیب قبلی برگرداند.

### Deposit

- constructor فقط factory و کد سرفصل تسویه را می‌گیرد.
- `QSqlDatabase` و member آن حذف شد.
- تمام SQL و includeهای Qt SQL حذف شدند.
- فقط قراردادهای Application و Qt Core باقی ماندند.
- ترتیب Account lock → Balance lock → Hold lock و سپس Snapshot/Journal/Balance تغییر نکرد.

## ۴. حفظ SQL، bind و ترتیب

سه SQL منتقل‌شده با نسخه قبل مقایسه شدند:

| بخش | SQL | bind | طول |
|---|---|---|---|
| Account currency lookup | یکسان | `:id` | ۴۶ |
| Account row lock | یکسان | `:account_id` | ۵۶ |
| Hold row lock | یکسان | `:account_id` | ۸۱ |

ترتیب حساس فراخوانی‌ها در آزمون orchestration بررسی شد:

`uow.begin` → Transaction lock/lookup → Account currency → Transaction create → Account lock → Balance lock → Hold lock → Balance snapshot → Journal/Ledger → Balance update → Ledger update → Journal post → Transaction complete → `uow.commit`

## ۵. تغییر پیام‌های خطا

کدهای API و شاخه‌های کنترل حفظ شدند. متن خام جایگزین‌شده:

| کد API | متن قبلی | متن فعلی |
|---|---|---|
| `ACCOUNT_LOOKUP_FAILED` | متن خام `QSqlError::text()` | `Account currency lookup failed` |
| `ACCOUNT_LOCK_FAILED` | متن خام `QSqlError::text()` | `Account row lock failed` |
| `HOLD_LOCK_FAILED` | متن خام `QSqlError::text()` | `Account hold rows could not be locked` |

مسیر `ACCOUNT_NOT_FOUND` و تشخیص NotFound از Failed بدون تغییر حفظ شد.

## ۶. Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق.

```text
-- Configuring done
-- Generating done
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/repository/accountstaterepository.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/repository/holdrepository.cpp.o
[  6%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[  8%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  9%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[ 10%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/depositcontroller.cpp.o
[ 12%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

## ۷. آزمون orchestration Deposit بدون Qt SQL

این آزمون فقط با Qt5Core کامپایل شد و هیچ فایل Infrastructure یا Qt5Sql را لینک نکرد.

```bash
g++ -std=c++17 -fPIC -I. tests/deposit_orchestration_test.cpp \
  src/application/depositservice.cpp \
  src/application/transactionservice.cpp \
  src/domain/transaction.cpp \
  $(pkg-config --cflags --libs Qt5Core) \
  -o /tmp/deposit_orchestration_test

/tmp/deposit_orchestration_test
```

خروجی واقعی:

```text
All deposit orchestration tests passed
```

### پوشش

- موفقیت کامل و ترتیب حساس فراخوانی‌ها از `uow.begin` تا `uow.commit`
- استفاده از `transactions()`، `accounts()`، `balances()`، `holds()` و `journals()` همان UoW
- Replay تراکنش Completed و عدم ورود به Account/Balance/Journal
- Account not found و rollback بدون ایجاد Transaction
- خطای persistence امن در Account currency lookup
- شکست Hold lock و توقف پیش از Journal
- شکست Journal post پس از نوشتن Balance و rollback پیش از commit
- `DATABASE_UNAVAILABLE` و توقف پیش از ساخت UoW

## ۸. آزمون persistence Account/Hold

```bash
g++ -std=c++17 -fPIC -I. tests/account_hold_persistence_test.cpp \
  src/infrastructure/repository/accountstaterepository.cpp \
  src/infrastructure/repository/holdrepository.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/account_hold_persistence_test

/tmp/account_hold_persistence_test
```

خروجی واقعی:

```text
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
All account and hold persistence tests passed
```

### پوشش

- خواندن موفق `currency_id` از `account` با `QSQLITE`
- تشخیص `NotFound` برای Account ناموجود، جدا از خطای persistence
- خطای امن برای currency lookup، account row lock و hold lock روی Connection بسته
- عدم نشت متن خام `not open`

### محدودیت صریح

- موفقیت `FOR UPDATE` روی `QSQLITE` آزموده نشد.
- آزمون PostgreSQL واقعی اجرا نشد؛ `QSQLITE` اثبات سازگاری PostgreSQL نیست.
- HTTP، Integration/Concurrency واقعی و Fault Injection واقعی اجرا نشد.

## ۹. بررسی نهایی وابستگی‌های Deposit

- جست‌وجوی `QSqlDatabase`، `QSqlQuery`، `QSqlError`، `lastError` و `infrastructure/repository` در `depositservice.h`/`.cpp`: بدون نتیجه.
- includeهای Deposit فقط شامل قراردادهای Application، `transactionservice.h` و Qt Core هستند.
- `DepositService` هیچ تصمیم مالی جدیدی دریافت نکرد؛ فقط محل اجرای SQL تغییر کرد.
- وابستگی Qt Core (`QDateTime`، `QCryptographicHash`، `QRegularExpression`، `QUuid`، `QDebug`) خارج از دامنه این مرحله باقی ماند.
- `main.cpp` فقط factory و کد سرفصل تسویه را به Deposit می‌دهد.

## ۱۰. داده‌ها

- هیچ داده‌ای در دیتابیس `corebanking` ایجاد یا تغییر نکرد.
- هیچ SQL واقعی روی PostgreSQL اجرا نشد.
- هیچ شناسه کسب‌وکاری جدیدی تولید نشد.
- آزمون‌ها فقط از fake و دیتابیس حافظه‌ای `QSQLITE` استفاده کردند.

## ۱۱. موارد باز که حل نشده‌اند

- هم‌زمانی قواعد مؤثر Balance، همه نویسندگان آن‌ها و شکاف نبود Override باز است.
- زمان مرجع پس از قفل‌ها و انتخاب `CURRENT_TIMESTAMP`/`CURRENT_DATE` تعیین تکلیف نشده است.
- محاسبات دقیق monetary و قواعد Domain تکمیل نشده‌اند.
- آزمون PostgreSQL واقعی، رفتار commit نامعلوم در QPSQL، Integration/Concurrency و Fault Injection واقعی اجرا نشد.
- محدودیت Connection مشترک و fail-closed فعلی باقی است.
- HoldService همچنان نویسنده مستقل Hold است.
- این مرحله تکمیل 1B/1C، رفع race یا آمادگی عملیاتی بانکی نیست.

## ۱۲. پیشنهاد Commit

پیام پیشنهادی (اجرا نشده است):

```text
refactor(deposit): extract remaining account and hold persistence
```
