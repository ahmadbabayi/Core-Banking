# گزارش تکمیل سرویس Withdrawal — 2026-11-16

نوع مرحله: پیاده‌سازی محدود Withdrawal (Application، API، Persistence لازم)، Build، آزمون هدفمند، آزمون واقعی API، تطبیق read-only دیتابیس و مستندسازی.

محدوده: فقط Withdrawal. Deposit، Transfer و Hold از نظر قرارداد و قواعد مالی تغییر نکردند. هیچ Migration/DDL یا تغییر Schema انجام نشد.

## ۱. SHA مبنا و وضعیت Git

- شاخه: `main`
- HEAD مبنا: `d5a31497e3b76806083f72d10ccc56464c109f6b`
- `origin/main` در زمان بررسی: همان `d5a3149`
- تغییرات این مرحله uncommitted هستند؛ هیچ `git add`/`commit`/`push` اجرا نشد.
- **تفکیک نسخه باینری:** شواهد API با `transactionId=12..18` (بخش ۸) مربوط به باینری **پیش از اصلاح minimum** هستند. پس از اصلاح minimum، Build دوباره اجرا شد، سرویس قدیمی متوقف و باینری جدید اجرا شد (PID `58285`، پورت `8080`). شاهد API پس از اصلاح، `transactionId=19` است؛ سایر شواهد Snapshot/Rule در بخش ۱۰ با harness ایزوله PostgreSQL و QSQLITE گرفته شده‌اند.

## ۲. قرارداد نهایی API

### مسیر

`POST /api/v1/withdrawals`

در صورت تنظیم‌نبودن Controller:

`503 Service Unavailable` با کد `WITHDRAWAL_SERVICE_UNAVAILABLE`.

### درخواست

```json
{
  "accountId": 3,
  "amount": "0.05",
  "description": "optional",
  "idempotencyKey": "unique-key"
}
```

- `accountId`: عدد صحیح مثبت.
- `amount`: رشته یا JSON number. مسیر دقیق پیشنهادی، رشته است. برای JSON number، اگر ۸ رقم اعشار نتواند همان مقدار را بازنمایی کند، درخواست پیش از هر نوشتن با `422 INVALID_AMOUNT` رد می‌شود.
- `description`: اختیاری، حداکثر ۵۰۰ کاراکتر.
- `idempotencyKey`: اجباری، ۱ تا ۱۰۰ کاراکتر پس از trim.

### پاسخ موفق

`201 Created`

```json
{"data":{"transactionId":13,"transactionNumber":"WDR-...","status":"COMPLETED","code":"WITHDRAWAL_COMPLETED","message":"Withdrawal completed"},"success":true}
```

Replay همان کلید و payload نیز طبق قرارداد موجود Deposit با `201` و همان `transactionId`/`transactionNumber` پاسخ می‌دهد.

### نگاشت HTTP و کدهای خطای استخراج‌شده از کد نهایی

- `400 INVALID_JSON` — body معتبر JSON object نیست.
- `400 INVALID_WITHDRAWAL_REQUEST` — `accountId`/`amount`/`idempotencyKey` مفقود یا نوع نامعتبر.
- `400 INVALID_ACCOUNT_ID` — شناسه حساب مثبت صحیح نیست.
- `400 INVALID_DESCRIPTION` — description غیر رشته‌ای است.
- `422 INVALID_AMOUNT` — مبلغ صفر/منفی/بیش از ۸ رقم اعشار یا JSON number غیرقابل بازنمایی دقیق در ۸ رقم.
- `400 MISSING_IDEMPOTENCY_KEY` — کلید خالی است.
- `422` برای رد کسب‌وکاری (`WithdrawalResult::Status::Rejected`).
- `500` برای خطای فنی (`WithdrawalResult::Status::Failed`).

کدهای قابل بازگشت از سرویس/Controller (استخراج از `withdrawalservice.cpp`، `withdrawalcontroller.cpp` و `transactionservice.cpp`):

`DATABASE_UNAVAILABLE`، `SETTLEMENT_LEDGER_NOT_CONFIGURED`، `TRANSACTION_START_FAILED`، `IDEMPOTENCY_LOCK_FAILED`، `IDEMPOTENCY_LOOKUP_FAILED`، `IDEMPOTENCY_KEY_REUSED`، `IDEMPOTENCY_IN_PROGRESS`، `ACCOUNT_LOOKUP_FAILED`، `ACCOUNT_NOT_FOUND`، `ACCOUNT_LOCK_FAILED`، `ACCOUNT_BALANCE_LOCK_FAILED`، `HOLD_LOCK_FAILED`، `ACCOUNT_LOCK_OR_READ_FAILED`، `ACCOUNT_BALANCE_NOT_INITIALIZED`، `ACCOUNT_NOT_ACTIVE`، `PRODUCT_NOT_ACTIVE`، `CUSTOMER_LEDGER_NOT_POSTABLE`، `CUSTOMER_LEDGER_CURRENCY_MISMATCH`، `AVAILABLE_FUNDS_CHECK_FAILED`، `INSUFFICIENT_AVAILABLE_FUNDS`، `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED`، `SETTLEMENT_LEDGER_LOOKUP_FAILED`، `LEDGER_BALANCE_INIT_FAILED`، `LEDGER_BALANCE_LOCK_FAILED`، `TRANSACTION_ENTRY_CREATE_FAILED`، `JOURNAL_CREATE_FAILED`، `JOURNAL_ENTRY_CREATE_FAILED`، `JOURNAL_BALANCE_CHECK_FAILED`، `JOURNAL_NOT_BALANCED`، `ACCOUNT_BALANCE_UPDATE_FAILED`، `LEDGER_BALANCE_PREPARE_FAILED`، `LEDGER_BALANCE_UPDATE_FAILED`، `JOURNAL_POST_FAILED`، `TRANSACTION_COMPLETE_FAILED`، `DATABASE_COMMIT_FAILED`، `DATABASE_ROLLBACK_FAILED`، `REJECTION_PERSIST_FAILED`، `TRANSACTION_CREATE_FAILED`، `INVALID_ACCOUNT_ID`، `INVALID_AMOUNT`، `INVALID_IDEMPOTENCY_KEY`، `INVALID_DESCRIPTION`.

## ۳. Schema

هیچ تغییر Schema انجام نشد. موارد بررسی‌شده:

- `transaction.transaction_type` شامل `WITHDRAWAL` است (`ck_transaction_type`).
- `transaction_entry.entry_type` شامل `SOURCE` است.
- `journal` و `journal_entry` و قیود debit/credit موجودند.
- `account_balance` با `ledger_balance >= 0` (`ck_account_balance_ledger`).
- `ledger_account_balance` موجود است.
- `uq_transaction_api_idempotency` موجود است.
- `product_balance_rule`، `account_balance_rule_override` و `account_hold_status` موجودند.

## ۴. فایل‌های تغییرکرده

### ایجادشده

- `src/application/withdrawalservice.h`
- `src/application/withdrawalservice.cpp`
- `src/web/withdrawalcontroller.h`
- `src/web/withdrawalcontroller.cpp`
- `tests/withdrawal_orchestration_test.cpp`
- `docs/opencode-reports/WITHDRAWAL_COMPLETION_RESULT.md`

### تغییر‌یافته

- `src/application/contracts/ibalancerepository.h`
- `src/application/contracts/ijournalrepository.h`
- `src/application/contracts/itransactionrepository.h`
- `src/infrastructure/repository/balancerepository.h`
- `src/infrastructure/repository/balancerepository.cpp`
- `src/infrastructure/repository/journalrepository.h`
- `src/infrastructure/repository/journalrepository.cpp`
- `src/infrastructure/repository/transactionrepository.h`
- `src/infrastructure/repository/transactionrepository.cpp`
- `src/web/httprouter.h`
- `src/web/httprouter.cpp`
- `src/main.cpp`
- `CMakeLists.txt`
- `tests/deposit_orchestration_test.cpp` (پیاده‌سازی متدهای جدید interface در fakeها)
- `tests/balance_repository_unitofwork_test.cpp` (آزمون هدفمند Withdrawal funds/decrement و مرز minimum)
- `docs/PROJECT_CONTEXT.md` (ثبت سیاست قطعی حداقل مانده و وضعیت Withdrawal)
- `docs/PROJECT_HISTORY.md` (ثبت بخش 18.27 و یادداشت حل‌شدن تصمیم در 18.26)

فهرست بالا با `git status` واقعی این مرحله هماهنگ است (۱۷ فایل modified و ۶ فایل untracked).

## ۵. قواعد مالی اعمال‌شده

- ترتیب قفل: Account → Balance → Hold → Snapshot، مطابق Deposit/Transfer/Hold.
- `deposit_funds = ledger_balance - active_holds - minimum_balance`.
- `overdraft_capacity = ledger_balance - active_holds + effective_overdraft_limit - minimum_balance`.
- اگر مبلغ در `deposit_funds` باشد، برداشت انجام می‌شود.
- اگر فقط در `overdraft_capacity` باشد، با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود (مانند Transfer؛ چرخه اضافه‌برداشت پیاده نشده است).
- در غیر این صورت `INSUFFICIENT_AVAILABLE_FUNDS`.
- `available_balance` فقط Cache است و مبنای مجوز برداشت نیست؛ مقایسه NUMERIC در SQL انجام می‌شود.
- Journal برداشت: سطر ۱ بدهکار سرفصل مشتری (LIABILITY)، سطر ۲ بستانکار سرفصل تسویه (ASSET).
- `transaction_entry` با نوع `SOURCE` و مبلغ برداشت.
- Transaction، TransactionEntry، Journal/Entry، account_balance و ledger_account_balance در همان UoW/تراکنش مشترک و بدون Commit مستقل ثبت می‌شوند.
- `available_balance` پس از برداشت با فرمول فعلی `new_ledger - active_holds + effective_overdraft_limit` بازمحاسبه می‌شود (minimum در Cache وارد نمی‌شود؛ همان رفتار Deposit/Transfer).

## ۶. بازبینی تغییر مشترک `loadLockedAccountSnapshot`

تغییر مشترک: افزودن دو ستون `b.ledger_balance` و `minimum_balance` مؤثر به Snapshot، برای استفاده Withdrawal. Deposit از این دو ستون استفاده نمی‌کند و ایندکس ستون‌های قبلی (0..11) تغییر نکرده است.

سیاست حداقل مانده در Snapshot بر اساس تصمیم قطعی کاربر اعمال شد:

```text
effective_minimum = GREATEST(
    COALESCE(product_minimum, 0),
    COALESCE(account_override_minimum, 0)
)
```

- حداقل محصول کف است و Override حساب فقط می‌تواند آن را افزایش دهد.
- `LEAST` استفاده نمی‌شود.
- نبود Override حداقل محصول را حذف نمی‌کند.
- نبود Rule محصول پایه صفر دارد؛ Override مؤثر مثبت همچنان اعمال می‌شود.
- Override صفر یا کمتر از حداقل محصول، حداقل را کاهش نمی‌دهد.
- بازه اعتبار و `CURRENT_TIMESTAMP` بدون تغییر حفظ شدند.
- مصرف‌کننده مشترک: `DepositService` ستون `minimumBalance` را نمی‌خواند؛ `TransferService` SQL جدا و دست‌نخورده دارد؛ `HoldService` حداقل را استفاده نمی‌کند. تنها مصرف‌کننده `minimumBalance`، `WithdrawalService` است و آن را دقیقاً یک بار در `checkWithdrawalFunds` لحاظ می‌کند.
- تفاوت باقی‌مانده: `TransferService` هنوز از `COALESCE(override, product, 0)` استفاده می‌کند. این مورد به‌عنوان **مورد بازبینی Transfer** ثبت شد و در این مرحله تغییر نکرد.

پاسخ به موارد بازبینی:

- **JOIN و تعداد ردیف:** JOINهای موجود `account` → `account_balance` → `product` → `ledger_account` همان‌ها هستند و 1:1اند. Ruleها با Scalar Subquery خوانده می‌شوند، نه JOIN؛ بنابراین ردیف تکثیر نمی‌شود و حساب بدون Rule حذف نمی‌شود.
- **قفل:** `FOR UPDATE OF a, b` بدون تغییر است و همان UoW آن را تا Commit/Rollback نگه می‌دارد.
- **تقدم و اعتبار زمانی:** Overdraft = `product.overdraft_enabled AND COALESCE(override.overdraft_enabled, TRUE)` و سقف = `LEAST(product_limit, COALESCE(override_limit, product_limit))`؛ Minimum = `GREATEST(COALESCE(product_minimum,0), COALESCE(override_minimum,0))`. بازه‌ها با `CURRENT_TIMESTAMP` و بازه `[from, to)` اعمال می‌شوند.
- **نبود/نامعتبری Rule:** نبود Rule با `COALESCE(..., 0)` و `ELSE 0::NUMERIC` مدیریت می‌شود؛ حساب حذف نمی‌شود. Ruleهای نامعتبر توسط Constraintهای Schema ممکن نیستند.
- **حفظ رفتار Deposit:** Deposit فقط `activeHolds` و `effectiveOverdraftLimit` را مصرف می‌کند و این دو ستون تغییر نکرده‌اند. آزمون‌های Deposit و یک Deposit واقعی پس از این تغییر موفق بودند.

### شواهد تست مربوط به Snapshot

- آزمون واقعی Deposit پس از تغییر: `transactionId=12` با مبلغ `0.20` روی حساب `3` که هیچ Rule مؤثری ندارد → `201` و Journal `9` با وضعیت `POSTED`. این نشان می‌دهد حساب بدون Rule حذف نمی‌شود و مسیر Deposit حفظ شده است.
- آزمون واقعی Withdrawal: `transactionId=13` روی حساب `3` (بدون Rule) و `transactionId=14` روی حساب `1` با Hold فعال `0.52` → هر دو موفق؛ مسیر minimum=0/overdraft=0 و Hold در Snapshot درست عمل کرد.
- آزمون هدفمند `tests/balance_repository_unitofwork_test.cpp` (QSQLITE): طبقه‌بندی `WithinDepositFunds`/`WithinOverdraftCapacity`/`Insufficient` و اثر minimum و کاهش `ledger_balance`/بازمحاسبه `available_balance` بررسی شد.
- **پوشش‌نشده:** Snapshot SQL به‌صورت Unit روی PostgreSQL آزموده نشد؛ حساب دارای Rule واقعی، Override، مرزهای `effective_from/effective_to`، Rule منقضی و درج هم‌زمان Rule آزموده نشدند.

## ۷. Build و تست‌های هدفمند

### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه: موفق؛ `[100%] Built target CoreBanking`.

### آزمون‌های جدید/تغییریافته

```bash
g++ -std=c++17 -fPIC -I. tests/withdrawal_orchestration_test.cpp \
  src/application/withdrawalservice.cpp src/application/transactionservice.cpp \
  src/domain/transaction.cpp src/web/withdrawalcontroller.cpp \
  src/web/apiresponse.cpp $(pkg-config --cflags --libs Qt5Core) \
  -o /tmp/opencode/withdrawal_orchestration_test
/tmp/opencode/withdrawal_orchestration_test
```

نتیجه: `All withdrawal orchestration tests passed`

پوشش: ترتیب کامل فراخوانی‌ها، مبلغ ارسالی به Balance/Entry، Replay بدون دست‌زدن به حساب/مانده، `INSUFFICIENT_AVAILABLE_FUNDS` با Commit رد، `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED`، شکست Journal post و Rollback پس از نوشتن مانده، `DATABASE_UNAVAILABLE`، و اعتبارسنجی مبلغ/کلید در Controller.

```bash
g++ -std=c++17 -fPIC -I. tests/balance_repository_unitofwork_test.cpp \
  src/infrastructure/unitofwork.cpp \
  src/infrastructure/repository/balancerepository.cpp \
  src/infrastructure/repository/journalrepository.cpp \
  src/infrastructure/repository/accountstaterepository.cpp \
  src/infrastructure/repository/holdrepository.cpp \
  src/infrastructure/repository/transactionrepository.cpp \
  src/domain/transaction.cpp $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/opencode/balance_repository_unitofwork_test
/tmp/opencode/balance_repository_unitofwork_test
```

نتیجه: `All balance repository and unit-of-work tests passed` (شامل آزمون جدید Withdrawal و آزمون مرز `5` / `5.00000001` با `ledger=10`, `holds=2`, `minimum=3`).

### harness ایزوله PostgreSQL برای سیاست minimum

پس از اصلاح Snapshot، Build دوباره اجرا شد و Query read-only زیر روی `corebanking` اجرا شد:

```sql
SET statement_timeout = '5s';
SET default_transaction_read_only = on;

WITH cases(name, product_min, account_min, expected) AS (
    VALUES
        ('product100_no_override',   100::numeric, NULL::numeric, 100::numeric),
        ('product100_override20',    100::numeric, 20::numeric,   100::numeric),
        ('product100_override150',   100::numeric, 150::numeric,  150::numeric),
        ('product100_override0',     100::numeric, 0::numeric,    100::numeric),
        ('no_product_override20',    NULL::numeric, 20::numeric,   20::numeric),
        ('no_rules',                 NULL::numeric, NULL::numeric,  0::numeric),
        ('product100_expired_or_ineffective_override', 100::numeric, NULL::numeric, 100::numeric)
)
SELECT name,
       GREATEST(COALESCE(product_min, 0), COALESCE(account_min, 0)) AS effective_minimum,
       expected,
       GREATEST(COALESCE(product_min, 0), COALESCE(account_min, 0)) = expected AS pass
FROM cases ORDER BY name;
```

نتیجه: هر ۷ حالت `pass = t`. این Query عبارت SQL واقعی سیاست را روی PostgreSQL و با داده فقط در `VALUES/CTE` بررسی می‌کند؛ Subqueryهای جدول Rule، فیلتر زمانی و wiring کامل Snapshot تولیدی را پوشش نمی‌دهد. هیچ نوشتنی در جداول انجام نشد.

### آزمون‌های Regression

- `tests/deposit_orchestration_test.cpp` → `All deposit orchestration tests passed`
- `tests/journal_repository_unitofwork_test.cpp` → `All journal repository and unit-of-work tests passed`
- `tests/account_hold_persistence_test.cpp` → `All account and hold persistence tests passed`

هشدارهای `database not open` در این تست‌ها مربوط به سناریوهای عمدی اتصال بسته‌اند. این تست‌ها fake/QSQLITE هستند و جایگزین PostgreSQL، هم‌زمانی واقعی یا Fault Injection نیستند.

## ۸. آزمون واقعی API، کلیدها و شناسه‌ها

حساب‌ها:

- `1` (`0100000000017`): ACTIVE، Hold فعال `0.52`، بدون Rule مؤثر.
- `2` (`0100000000022`): CLOSED.
- `3` (`0100000000038`): ACTIVE، بدون Hold و بدون Rule مؤثر؛ برای آزمون Withdrawal استفاده شد.

تأمین وجه از API Deposit:

```bash
curl -i -X POST http://localhost:8080/api/v1/deposits \
  -H 'Content-Type: application/json' \
  --data '{"accountId":3,"amount":"0.20","description":"Setup funds for withdrawal tests","idempotencyKey":"withdrawal-setup-deposit-20261116-a1"}'
```

نتیجه: `201 Created`, `transactionId=12`, `journalId=9` (تأیید read-only).

برداشت موفق:

```bash
curl -i -X POST http://localhost:8080/api/v1/withdrawals \
  -H 'Content-Type: application/json' \
  --data '{"accountId":3,"amount":"0.05","description":"Withdrawal completion test S1","idempotencyKey":"withdrawal-completion-20261116-s1"}'
```

نتیجه: `201 Created`, `transactionId=13`, `journalId=10`.

| کلید | حساب | نتیجه | Transaction | Journal | مبلغ |
|---|---|---|---|---|---|
| `withdrawal-setup-deposit-20261116-a1` | 3 | `201 DEPOSIT_COMPLETED` | 12 | 9 | `0.20` |
| `withdrawal-completion-20261116-s1` | 3 | `201 WITHDRAWAL_COMPLETED` | 13 | 10 | `0.05` |
| `withdrawal-completion-20261116-s2` | 1 | `201 WITHDRAWAL_COMPLETED` | 14 | 11 | `0.10` |
| `withdrawal-completion-20261116-numeric-2dp` | 3 | `201 WITHDRAWAL_COMPLETED` | 18 | 12 | `0.01` |
| `withdrawal-completion-20261116-hold-limit` | 1 | `422 INSUFFICIENT_AVAILABLE_FUNDS` | 15 (REJECTED) | — | `1.0` |
| `withdrawal-completion-20261116-insufficient` | 3 | `422 INSUFFICIENT_AVAILABLE_FUNDS` | 16 (REJECTED) | — | `1.0` |
| `withdrawal-completion-20261116-closed` | 2 | `422 ACCOUNT_NOT_ACTIVE` | 17 (REJECTED) | — | `0.01` |
| `withdrawal-completion-20261116-missing` | 999999 | `422 ACCOUNT_NOT_FOUND` | — | — | `0.01` |

نتیجه Replay/Conflict:

- Replay `withdrawal-completion-20261116-s1`: `201`، همان `transactionId=13`، بدون رکورد/Journal جدید؛ مانده حساب `3` در آن مقطع `0.15` ماند.
- Replay `withdrawal-completion-20261116-s2`: `201`، همان `transactionId=14`، بدون تغییر مانده حساب `1` (`1.41691357`).
- Conflict همان کلید با مبلغ متفاوت: `422 IDEMPOTENCY_KEY_REUSED` و بدون اثر مالی.
- Reuse کلید Deposit در Withdrawal: `422 IDEMPOTENCY_KEY_REUSED`؛ تعداد تراکنش با کلید `withdrawal-setup-deposit-20261116-a1` همان `1` ماند و مانده حساب `3` همان `0.14`.
- مبلغ `0`، `-0.01`، `"0.123456789"` و JSON number `0.123456789` → `422 INVALID_AMOUNT` و بدون رکورد.
- JSON number معتبر `0.01` → `201` و ذخیره دقیق `0.01` (`transactionId=18`).

شاهد قبل/بعد برای Replay/Conflict: در زمان Replay/Conflict، `transactionId` جدیدی ایجاد نشد و مانده‌های ثبت‌شده (حساب `3` = `0.15` و حساب `1` = `1.41691357`) بدون تغییر ماندند؛ شمارش Transaction/Journal نیز افزایش نیافت.

### Regression پس از اصلاح minimum (باینری جدید)

پس از Build مجدد و restart سرویس (PID `58285`)، یک برداشت واقعی روی حساب `3` با مبلغ `0.01` و کلید `withdrawal-minpolicy-20261116-s1` اجرا شد:

- پاسخ: `201 Created`, `transactionId=19`, `transactionNumber=WDR-1794843794629-400446EB`
- Journal: `13`, وضعیت `POSTED`
- مانده حساب `3`: `0.14` → `0.13` (`ledger` و `available`)
- Journal: Debit `0.01` روی سرفصل `1` (LIABILITY) و Credit `0.01` روی سرفصل `4` (ASSET)
- Ledger: سرفصل `1` → `debit_total=0.17`, `credit_total=1.71691357`, `balance=-1.54691357`؛ سرفصل `4` → `debit_total=1.71691357`, `credit_total=0.17`, `balance=1.54691357`

این Regression ثابت می‌کند Snapshot تولیدی جدید روی مسیر no-rule اجرا می‌شود؛ اما سیاست minimum با ردیف Rule واقعی همچنان آزمون نشده است.

## ۹. تطبیق read-only دیتابیس

تأیید شناسه‌ها و Journalها (Query read-only، `statement_timeout=5s`، `default_transaction_read_only=on`):

| Transaction | نوع | وضعیت | مبلغ | Journal | وضعیت Journal |
|---|---|---|---|---|---|
| 12 | DEPOSIT | COMPLETED | 0.20 | 9 | POSTED |
| 13 | WITHDRAWAL | COMPLETED | 0.05 | 10 | POSTED |
| 14 | WITHDRAWAL | COMPLETED | 0.10 | 11 | POSTED |
| 18 | WITHDRAWAL | COMPLETED | 0.01 | 12 | POSTED |

- `transaction_entry`: برای txهای 13، 14، 18 و 19 یک سطر `SOURCE` با `account_id` و `amount` درست؛ ارز `1`.
- هر Journal دقیقاً ۲ سطر و `debit == credit` دارد.
- سطر ۱: `ledger_account_id=1` (LIABILITY) با Debit روی حساب مشتری.
- سطر ۲: `ledger_account_id=4` (ASSET) با Credit و `account_id=NULL`.
- برای txهای `REJECTED` (3، 6، 11، 15، 16، 17) تعداد Journal و TransactionEntry صفر است.

مانده‌های قبل/بعد:

| حساب | قبل از Withdrawal | بعد از tx13/14/18 | پس از Regression tx19 (باینری جدید) |
|---|---|---|---|
| 1 | `ledger=1.51691357`, `available=0.99691357`, Hold=`0.52` | `ledger=1.41691357`, `available=0.89691357`, Hold=`0.52` | بدون تغییر |
| 2 | `0` | `0` (CLOSED) | بدون تغییر |
| 3 | `0` سپس با Deposit=`0.20` | `0.14`, `available=0.14` | `0.13`, `available=0.13` |

- حساب `1`: `1.41691357 - 0.52000000 = 0.89691357`.
- حساب `3`: `0.20 - 0.05 - 0.01 = 0.14`؛ سپس `0.14 - 0.01 = 0.13`.

تطبیق Ledger:

- سرفصل `1` (LIABILITY): `debit_total=0.17` (مجموع برداشت‌ها)، `credit_total=1.71691357` (شامل Depositها)، `balance=-1.54691357`.
- سرفصل `4` (ASSET): `debit_total=1.71691357`، `credit_total=0.17`، `balance=1.54691357`.
- `debit_total - credit_total` برای هر دو سرفصل با `balance` برابر است.

## ۱۰. سیاست حداقل مانده — تصمیم قطعی و اعمال‌شده

تصمیم مالی کاربر قطعی است و دیگر «باز» نیست:

- حداقل مانده محصول، کف الزام‌آور برداشت است.
- قاعده اختصاصی حساب فقط می‌تواند حداقل را افزایش دهد، نه کاهش.
- کاهش حداقل از طریق معافیت مستقل فعلاً خارج از دامنه است.
- `effective_minimum = MAX(product_minimum, account_minimum)`.
- `LEAST` برای این سیاست استفاده نمی‌شود.

پیاده‌سازی در `loadLockedAccountSnapshot` با `GREATEST(COALESCE(product,0), COALESCE(override,0))` انجام شد. minimum دقیقاً یک بار در `checkWithdrawalFunds` کسر می‌شود. تعریف `available_balance` و سیاست Overdraft تغییر نکرد.

### هفت حالت minimum — harness ایزوله PostgreSQL

یک Query read-only روی `corebanking` با `default_transaction_read_only=on` و `statement_timeout=5s` اجرا شد. داده‌ها فقط داخل `VALUES/CTE` بودند؛ هیچ نوشتنی در جدول Rule یا جای دیگر انجام نشد. این تست **عبارت SQL واقعی سیاست** (`GREATEST/COALESCE` و NULL semantics) را روی PostgreSQL بررسی می‌کند، اما Subqueryهای جدول‌های Rule، فیلترهای زمانی و wiring کامل Snapshot تولیدی را پوشش نمی‌دهد.

| حالت | product_min | account_min | effective | نتیجه |
|---|---|---|---|---|
| محصول 100، بدون Override | 100 | NULL | 100 | pass |
| محصول 100، Override 20 | 100 | 20 | 100 | pass |
| محصول 100، Override 150 | 100 | 150 | 150 | pass |
| محصول 100، Override 0 | 100 | 0 | 100 | pass |
| بدون Rule محصول، Override 20 | NULL | 20 | 20 | pass |
| بدون هیچ Rule | NULL | NULL | 0 | pass |
| محصول 100، Override نامؤثر/منقضی (NULL مؤثر) | 100 | NULL | 100 | pass |

هر ۷ حالت در PostgreSQL نتیجه `pass` دادند. این harness جایگزین آزمون Snapshot تولیدی با ردیف‌های Rule واقعی نیست.

### مرز برداشت

- در `tests/balance_repository_unitofwork_test.cpp` (QSQLITE) بررسی شد:
  - `ledger=10`, `holds=2`, `minimum=3` → مقدار `5` پذیرفته می‌شود (`WithinDepositFunds`).
  - مقدار `5.00000001` (یک واحد کوچک‌ترین دقت) رد می‌شود (`Insufficient`).
  - این آزمون نشان می‌دهد Hold و minimum هر کدام یک بار کسر می‌شوند (اگر دوبار کسر می‌شد، مقدار `5` رد می‌شد).
- QSQLITE جایگزین PostgreSQL نیست؛ SQL واقعی `checkWithdrawalFunds` روی آن اجرا شد، اما Snapshot تولیدی و Rule واقعی پوشش داده نشدند.

### مورد بازبینی Transfer

`TransferService` هنوز از `COALESCE(override.minimum_balance, product.minimum_balance, 0)` بدون `GREATEST` استفاده می‌کند. طبق تصمیم جدید، این با سیاست حداقل مانده سازگار نیست، اما در این مرحله **تغییر نکرد** و به‌عنوان **مورد بازبینی مرحله Transfer** ثبت شد.

## ۱۱. Overdraft و موارد آزمون‌نشده

- Overdraft واقعی **آزمون نشد و موفق اعلام نمی‌شود**. جدول‌های Rule خالی‌اند و API نوشتن ندارند. مسیر نیازمند Overdraft عمداً با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود و در تست fake این رد بررسی شد، نه چرخه واقعی Overdraft.
- سیاست حداقل مانده با **ردیف Rule واقعی** در دیتابیس آزمون نشد؛ فقط عبارت SQL روی PostgreSQL با VALUES/CTE و مسیر no-rule از طریق API بررسی شد.
- Override، مرزهای زمانی Rule، Rule منقضی با ردیف واقعی و درج هم‌زمان Rule آزمون نشدند.
- سناریوی محصول غیرفعال، سرفصل غیرقابل‌ثبت/عدم‌تطابق ارز و محصول/سرفصل نامعتبر با fixture واقعی آزمون نشد.
- هم‌زمانی واقعی آزمون نشد؛ سرور تک‌ریسمانی و Connection مشترک است. درخواست‌های سریالی به‌عنوان race معرفی نمی‌شوند.
- Fault Injection واقعی، restart و `CommitOutcomeUnknown` آزمون نشدند؛ رفتار F1/F2 و fail-closed از کد و آزمون‌های قبلی حفظ شده است.
- Snapshot SQL به‌صورت Unit روی PostgreSQL آزمون نشد؛ مسیر واقعی API فقط برای حساب بدون Rule و حساب دارای Hold اجرا شد.

## ۱۲. محدودیت‌های باقی‌مانده و جمع‌بندی

- Withdrawal در دامنه مسیرهای آزموده‌شده (موفق، Replay، Conflict، موجودی ناکافی، Hold، حساب CLOSED/ناموجود، مبلغ نامعتبر و Rollback fake) کار می‌کند و تطبیق مالی read-only آن انجام شد.
- سیاست حداقل مانده قطعی و اعمال‌شده است: `MAX(product, account override)`؛ Override فقط افزایش‌دهنده است. مورد بازبینی باقی‌مانده، هم‌ترازی `TransferService` با همین سیاست است که در این مرحله انجام نشد.
- این مرحله **آمادگی جامع انتشار نیست**: Overdraft واقعی، Rule واقعی PostgreSQL، هم‌زمانی و Fault Injection آزمون نشده‌اند.
- API و قواعد Deposit، Transfer و Hold تغییر نکردند. تغییر مشترک افزودن دو ستون به Snapshot و متدهای جدید Withdrawal در قراردادها/Repositoryها بود؛ فقط Snapshot ستون minimum را برای Withdrawal با سیاست جدید محاسبه می‌کند.
- هیچ Credential/Token/Password یا connection string حساس در این گزارش درج نشده است.

## ۱۳. بررسی diff و پیشنهاد Commit (اجرا نشده)

- `git diff --check` بدون خطا.
- تغییرات scope محدود به Withdrawal و متدهای مشترک لازم است؛ هیچ فایل نامرتبطی تغییر نکرد.

پیشنهاد Commitها:

```bash
git add src/application/withdrawalservice.h src/application/withdrawalservice.cpp \
  src/web/withdrawalcontroller.h src/web/withdrawalcontroller.cpp \
  src/application/contracts/ibalancerepository.h \
  src/application/contracts/ijournalrepository.h \
  src/application/contracts/itransactionrepository.h \
  src/infrastructure/repository/balancerepository.h \
  src/infrastructure/repository/balancerepository.cpp \
  src/infrastructure/repository/journalrepository.h \
  src/infrastructure/repository/journalrepository.cpp \
  src/infrastructure/repository/transactionrepository.h \
  src/infrastructure/repository/transactionrepository.cpp \
  src/web/httprouter.h src/web/httprouter.cpp src/main.cpp CMakeLists.txt
git commit -m "feat(withdrawal): add atomic withdrawal API"

git add tests/withdrawal_orchestration_test.cpp \
  tests/deposit_orchestration_test.cpp \
  tests/balance_repository_unitofwork_test.cpp
git commit -m "test(withdrawal): cover orchestration and balance rules"

git add docs/PROJECT_CONTEXT.md docs/PROJECT_HISTORY.md \
  docs/opencode-reports/WITHDRAWAL_COMPLETION_RESULT.md
git commit -m "docs: record withdrawal completion and limitations"
```

نکته: اگر ترجیح می‌دهید کد و تست در یک Commit باشند، می‌توانید دو Commit اول را ادغام کنید. هیچ‌کدام از دستورهای بالا اجرا نشده‌اند.

## ۱۴. دستور کپی گزارش

```bash
xsel --clipboard --input < /home/ahmad/projects/CoreBanking/docs/opencode-reports/WITHDRAWAL_COMPLETION_RESULT.md
```
