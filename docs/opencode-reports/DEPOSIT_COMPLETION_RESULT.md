# گزارش تکمیل سرویس Deposit — 2026-11-16

نوع مرحله: تکمیل عملی سرویس Deposit (بررسی Schema، اصلاح کد، Build، آزمون واقعی API، تطبیق دیتابیس، مستندسازی).
محدوده: فقط Deposit. Withdrawal، Reverse، Hold و Transfer تغییر نکردند.

## ۱. SHA مبنا و وضعیت اولیه

- شاخه: `main`
- HEAD مبنا: `fd23100c06f5862234f14019ac4b4eac26b0ff86` (بدون commit جدید در این مرحله)
- `origin/main` در زمان بررسی: همان `fd23100`.
- working tree در شروع مرحله: تغییرات مستنداتی مرحله قبل (طراحی Balance) به‌صورت uncommitted موجود بود و حفظ شد:
  - `docs/PROJECT_CONTEXT.md` (modified)
  - `docs/PROJECT_HISTORY.md` (modified)
  - `docs/opencode-reports/BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md` (untracked)
- سرویس از قبل اجرا نبود؛ پس از Build از همان باینری موجود Build Directory اجرا شد.

## ۲. وضعیت Schema

ساختار دیتابیس برای Deposit کافی و سازگار بود؛ **هیچ Migration، DDL یا تغییر Schema انجام نشد.** موارد بررسی‌شده با نقش read-only `corebanking_reader`:

- `account_balance` با `ledger_balance` و `available_balance` و قید `ck_account_balance_ledger` (`ledger_balance >= 0`). قید `ck_account_balance_available` طبق Migration 016 حذف شده است (تأیید شد).
- `ledger_account_balance` با `debit_total`, `credit_total`, `balance` و قیود نامنفی debit/credit.
- `journal` و `journal_entry` با قیود debit/credit، `ck_journal_entry_debit_or_credit` و `uq_journal_entry_sequence`.
- `transaction` با `idempotency_key`, `request_fingerprint`, `result_code`, `result_message` و CHECKهای وضعیت/نوع/کانال/مبلغ.
- ایندکس یکتای جزئی `uq_transaction_api_idempotency` روی `(channel, idempotency_key)` برای `channel='API'` موجود است.
- VIEW `account_hold_status` موجود است و برای محاسبه Hold فعال استفاده می‌شود.
- `product_balance_rule` و `account_balance_rule_override` در Schema موجودند؛ در دیتابیس فعلی هر دو جدول **صفر ردیف** دارند (بدون قاعده Overdraft/حداقل مؤثر).

## ۳. نقص واقعی یافته‌شده و اصلاح

### نقص

در `DepositController`، وقتی `amount` به‌صورت JSON number ارسال می‌شد، با `QString::number(value, 'f', 8)` به رشته تبدیل می‌شد. این کار مبالغ با بیش از ۸ رقم اعشار را بی‌صدا گرد می‌کرد و مبلغی متفاوت از درخواست در دیتابیس ثبت می‌شد.

شاهد واقعی پیش از اصلاح (با باینری قدیمی):

- درخواست: `amount: 0.123456789` (عدد JSON)، حساب `1`
- پاسخ: `201 Created`، `transactionId=8`
- مقدار ذخیره‌شده در `transaction.amount`: `0.12345679` (گردشده)

این با قاعده مصوب «مبلغ حداکثر ۸ رقم اعشار» و با اصل «محاسبات پولی با اعشار دقیق» ناسازگار بود.

### اصلاح (کوچک‌ترین تغییر در دامنه Deposit)

در `src/web/depositcontroller.cpp`، برای ورودی عددی، رشته ۸ رقمی ساخته می‌شود و فقط وقتی پذیرفته می‌شود که با `toDouble()` دقیقاً به همان عدد اولیه برگردد؛ در غیر این صورت همان کد موجود `INVALID_AMOUNT` با HTTP `422` برگردانده می‌شود.

- ورودی رشته‌ای بدون تغییر پردازش می‌شود.
- ورودی عددی معتبر (نمونه: `0.12345678`) همچنان پذیرفته می‌شود.
- ورودی عددی با بیش از ۸ رقم اعشار (نمونه: `0.123456789`) دیگر گرد نمی‌شود و رد می‌شود.
- کد خطا، پیام و نگاشت HTTP جدیدی اضافه نشد.

توجه: رکورد `transactionId=8` مربوط به Probe پیش از اصلاح است و در دیتابیس توسعه باقی می‌ماند؛ این رکورد در پایان گزارش فهرست شده است.

## ۴. فایل‌های کد و تست تغییرکرده

- `src/web/depositcontroller.cpp` — اصلاح ورودی عددی مبلغ.
- `tests/deposit_orchestration_test.cpp` — افزودن دو آزمون Controller:
  - رد مبلغ عددی با اعشار بیش از ۸ رقم پیش از ساخت UnitOfWork.
  - پذیرش مبلغ عددی معتبر و پاس‌شدن مبلغ بدون گردکردن.

هیچ تغییر در `DepositService`، قراردادهای Application، Repositoryها، `TransferService`، `HoldService`، Schema یا `main.cpp` انجام نشد.

## ۵. Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/depositcontroller.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق؛ بدون هشدار کامپایلر در خروجی.

## ۶. آزمون‌های هدفمند (بدون PostgreSQL)

دستور و نتیجه واقعی:

```bash
g++ -std=c++17 -fPIC -I. tests/deposit_orchestration_test.cpp \
  src/application/depositservice.cpp \
  src/application/transactionservice.cpp \
  src/domain/transaction.cpp \
  src/web/depositcontroller.cpp \
  src/web/apiresponse.cpp \
  $(pkg-config --cflags --libs Qt5Core) \
  -o /tmp/opencode/deposit_orchestration_test

/tmp/opencode/deposit_orchestration_test
```

نتیجه:

```text
All deposit orchestration tests passed
```

پوشش این اجرا شامل مسیرهای قبلی (ترتیب فراخوانی، Replay، rollbackها، `DATABASE_UNAVAILABLE`) به‌همراه دو آزمون جدید مبلغ عددی است.

آزمون‌های موجود دیگر پس از این تغییر هم اجرا و تأیید شدند (بدون تغییر در آن فایل‌ها):

- `tests/balance_repository_unitofwork_test.cpp` → `All balance repository and unit-of-work tests passed`
  (دستور به‌روزشده شامل `journalrepository.cpp`, `accountstaterepository.cpp`, `holdrepository.cpp`).
- `tests/journal_repository_unitofwork_test.cpp` → `All journal repository and unit-of-work tests passed`
- `tests/account_hold_persistence_test.cpp` → `All account and hold persistence tests passed`
- هشدارهای `QSqlQuery::prepare: database not open` در این تست‌ها مربوط به سناریوهای عمدی اتصال بسته هستند.

محدودیت: این آزمون‌ها `QSQLITE`/fake هستند و جایگزین PostgreSQL واقعی، هم‌زمانی واقعی یا Fault Injection نیستند.

## ۷. حساب‌های استفاده‌شده و ایجادشده

- حساب `1` (`0100000000017`): ACTIVE، ارز `1`، محصول `1`، سرفصل `1`. حساب اصلی آزمون واریز.
- حساب `2` (`0100000000022`): از قبل `CLOSED`. برای سناریوی خطای `ACCOUNT_NOT_ACTIVE` استفاده شد.
- حساب `3` (`0100000000038`): در همین مرحله با `POST /api/v1/accounts` ایجاد شد (محصول `1`، شعبه `4`، مالک `49`). ACTIVE با مانده صفر و **بدون تراکنش**؛ برای آزمون‌های آینده Withdrawal/Transfer رزرو شده است.
- حساب‌های قبلی تغییر یا حذف نشدند.

## ۸. آزمون واقعی API (نمونه‌های curl)

سرویس از باینری Build فعلی روی `http://localhost:8080` اجرا شد. تمام ترتیب‌های مالی از API انجام شد.

واریز موفق:

```bash
curl -i -X POST http://localhost:8080/api/v1/deposits \
  -H 'Content-Type: application/json' \
  --data '{"accountId":1,"amount":"0.05","description":"Deposit completion after numeric fix","idempotencyKey":"deposit-completion-20261116-fixed-s1"}'
```

نتیجه: `201 Created`

```json
{"data":{"code":"DEPOSIT_COMPLETED","message":"Deposit completed","status":"COMPLETED","transactionId":9,"transactionNumber":"DEP-1794833865789-3690204E"},"success":true}
```

ورودی عددی با اعشار بیش از ۸ رقم پس از اصلاح:

```bash
curl -i -X POST http://localhost:8080/api/v1/deposits \
  -H 'Content-Type: application/json' \
  --data '{"accountId":1,"amount":0.123456789,"idempotencyKey":"deposit-completion-20261116-numeric-precision-fixed"}'
```

نتیجه: `422 Unprocessable Entity`

```json
{"error":{"code":"INVALID_AMOUNT","message":"Amount must be positive and have at most 8 decimal places"},"success":false}
```

ورودی عددی معتبر ۸ رقمی پس از اصلاح:

```bash
curl -i -X POST http://localhost:8080/api/v1/deposits \
  -H 'Content-Type: application/json' \
  --data '{"accountId":1,"amount":0.12345678,"idempotencyKey":"deposit-completion-20261116-fixed-numeric-8dp"}'
```

نتیجه: `201 Created`, `transactionId=10`, مقدار ذخیره‌شده `0.12345678`.

نمونه خطاهای دیگر: مبلغ `"0"` → `422 INVALID_AMOUNT`؛ حساب `999999` → `422 ACCOUNT_NOT_FOUND`؛ حساب `2` (CLOSED) → `422 ACCOUNT_NOT_ACTIVE`؛ نبود `idempotencyKey` → `400 INVALID_DEPOSIT_REQUEST`؛ JSON نامعتبر → `400 INVALID_JSON`.

## ۹. کلیدهای آزمون و شناسه‌های مالی

| کلید Idempotency | نتیجه | Transaction | Journal | مبلغ |
|---|---|---|---|---|
| `deposit-completion-20261116-s1` | `201` سپس Replay `201` | `5` | `4` | `0.1` |
| `deposit-completion-20261116-closed-acct` | `422 ACCOUNT_NOT_ACTIVE` | `6` (REJECTED) | — | `0.01` |
| `deposit-completion-20261116-numeric` | `201` (عدد JSON) | `7` | `5` | `0.1` |
| `deposit-completion-20261116-numeric-precision` | `201` پیش از اصلاح (Probe) | `8` | `6` | `0.12345679` (گردشده) |
| `deposit-completion-20261116-numeric-precision-fixed` | `422 INVALID_AMOUNT` پس از اصلاح | — | — | — |
| `deposit-completion-20261116-fixed-s1` | `201` سپس Replay `201`، Conflict `422` | `9` | `7` | `0.05` |
| `deposit-completion-20261116-fixed-numeric-8dp` | `201` پس از اصلاح | `10` | `8` | `0.12345678` |
| `deposit-completion-20261116-fixed-closed-acct` | `422 ACCOUNT_NOT_ACTIVE` پس از اصلاح | `11` (REJECTED) | — | `0.01` |
| `deposit-completion-20261116-fixed-missing-acct` | `422 ACCOUNT_NOT_FOUND` | — | — | — |

## ۱۰. مانده‌ها قبل و بعد

مانده حساب `1` پیش از این مرحله: `ledger_balance=1.02`، `available_balance=0.50`، Hold فعال `0.52`.

مانده حساب `1` پس از آزمون‌ها:

- `ledger_balance=1.51691357`
- `available_balance=0.99691357`
- Hold فعال: `0.52000000` (بدون تغییر)

تطبیق: `1.51691357 - 0.52000000 = 0.99691357`. واریزهای موفق این مرحله: `0.1 + 0.1 + 0.12345679 + 0.05 + 0.12345678 = 0.51691357`. بنابراین `1.02 + 0.51691357 = 1.51691357`.

مانده حساب‌های `2` و `3` بدون تغییر: `ledger_balance=0`، `available_balance=0`.

توجه: `available_balance` در این پروژه Cache است و به‌عنوان وجه تضمین‌شده قابل برداشت معرفی نمی‌شود.

## ۱۱. تطبیق ثبت حسابداری

برای تراکنش‌های موفق `5`, `7`, `8`, `9`, `10`:

- هر Journal در وضعیت `POSTED` و دارای دقیقاً ۲ سطر است.
- جمع Debit = جمع Credit (برابر مبلغ واریز).
- سطر اول: سرفصل `4` (`10100101`, ASSET) به‌صورت Debit، `account_id=NULL`، ارز `1`.
- سطر دوم: سرفصل `1` (`20100101`, LIABILITY) به‌صورت Credit روی `account_id` حساب، ارز `1`.
- `transaction_entry`: تنها یک سطر `DESTINATION` با مبلغ و ارز درست.
- `ledger_account_balance`: سرفصل `4` → `debit_total=1.51691357`، `balance=1.51691357`؛ سرفصل `1` → `credit_total=1.51691357`، `balance=-1.51691357`.
- برای تراکنش‌های `REJECTED` (`6`, `11`) هیچ Journal، Journal Entry یا تغییر مانده‌ای ثبت نشده است.
- هیچ کلید Idempotency تکراری در `channel='API'` وجود ندارد (`duplicate_keys=0`).
- اثر نیمه‌کاره یا ثبت تکراری مشاهده نشد.

## ۱۲. نتیجه Replay، Conflict و خطاها

- Replay کلید `deposit-completion-20261116-fixed-s1` با همان payload: `201`، همان `transactionId=9`، بدون Transaction/Journal جدید و بدون تغییر مانده.
- همان کلید با مبلغ متفاوت: `422 IDEMPOTENCY_KEY_REUSED`، بدون اثر مالی.
- Replay تراکنش ردشده (`ACCOUNT_NOT_ACTIVE`): همان `422 ACCOUNT_NOT_ACTIVE` و بدون رکورد جدید.
- Conflict تراکنش ردشده با مضمون متفاوت: `422 IDEMPOTENCY_KEY_REUSED`.
- حساب ناموجود: `422 ACCOUNT_NOT_FOUND` و بدون رکورد.
- مبلغ نامعتبر (`"0"`, `"-0.01"`, `"0.123456789"`): `422 INVALID_AMOUNT` و بدون رکورد.
- ضمناً `CommitOutcomeUnknown`/`RollbackFailed`/fail-closed در کد دست‌نخورده باقی ماند (F1/F2)؛ این مسیرها در این مرحله اجرا نشدند.

## ۱۳. موارد اجرانشده و محدودیت‌ها

- **Schema:** هیچ Migration/DDL انجام نشد؛ نیازی تشخیص داده نشد.
- **قواعد محصول/Override:** جدول‌های `product_balance_rule` و `account_balance_rule_override` خالی‌اند و API مدیریت/نوشتن ندارند؛ بنابراین مسیر Overdraft مؤثر و Override با داده واقعی آزمون نشد. ساخت fixture خارج از دامنه Deposit است.
- **وضعیت محصول ناهمگام/نامعتبر:** برای ساخت این خطا باید محصول مشترک غیرفعال می‌شد که بدون بررسی اثر آن انجام نشد؛ محدودیت ثبت می‌شود.
- **سرفصل نامعتبر/غیرقابل‌ثبت/عدم‌تطابق ارز:** fixture ایمن موجود نبود؛ محدودیت ثبت می‌شود.
- **هم‌زمانی واقعی:** سرور و Connection مشترک تک‌ریسمانی است؛ آزمون هم‌زمانی واقعی انجام نشد.
- **Fault Injection / restart / Commit نامعلوم:** اجرا نشد؛ رفتار فقط از کد و آزمون‌های Harness قبلی تأیید شده است.
- **آزمون PostgreSQL واقعی از طریق Unit Test:** انجام نشد؛ `QSQLITE` جایگزین PostgreSQL نیست. تطبیق واقعی داده با API و SQL read-only انجام شد.
- **DepositState/product effective_to:** مرز تاریخی آزمون نشد.
- **رکورد باقی‌مانده پیش از اصلاح:** `transactionId=8` با مبلغ گردشده `0.12345679` در دیتابیس توسعه باقی است و حذف/اصلاح نشد.

## ۱۴. جمع‌بندی

- Deposit در دامنه مصوب خود (واریز موفق، ثبت متوازن، به‌روزرسانی مانده، Idempotency/Replay/Conflict، خطاهای ورودی و وضعیت حساب) **تأیید و آزمون واقعی شد**.
- یک نقص واقعی مالی (گردکردن بی‌صدای مبلغ عددی) کشف و با کوچک‌ترین تغییر Deposit-scoped اصلاح و آزمون شد.
- تفاوت رفتاری واحد: مبلغ عددی با اعشار بیش از ۸ رقم اکنون `422 INVALID_AMOUNT` است، نه واریز گردشده. این تغییر با قاعده مصوب ۸ رقم اعشار سازگار است؛ در صورت نیاز به قرارداد متفاوت، تصمیم جداگانه لازم است.
- هیچ Commit اجرا نشد؛ پیشنهادها در ادامه آمده است.
