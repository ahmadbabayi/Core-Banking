# پذیرش محدود زیرمرحله 1A — با محدودیت‌های ثبت‌شده

- تاریخ ثبت: 2026-11-12
- دامنه: پذیرش مستنداتی و محدود 1A؛ نه پیاده‌سازی جدید، نه شروع 1B و نه طراحی/اجرای Balance Slice.
- این سند فقط جمع‌بندی شواهد موجود است. هیچ آزمون، Build، HTTP، SQL، Fault Injection یا restart در این نوبت اجرا نشد.

## 1. منابع بررسی‌شده

- `AGENTS.md` (قواعد پروژه و محدودیت‌های اجرا)
- `docs/PROJECT_CONTEXT.md` — بخش 18.5 (وضعیت و پذیرش محدود 1A)
- `docs/PROJECT_HISTORY.md` — بخش 18.17 (اجرای 1A در 2026-11-11)
- گزارش‌های موقت خارج از مخزن در `/tmp`:
  - `/tmp/core-banking-1a-review.md`
  - `/tmp/core-banking-1a-review-code.txt`
  - `/tmp/core-banking-1a-failure-handling-plan.md`
  - `/tmp/core-banking-1a-f1-f2-implementation.md`
  - `/tmp/core-banking-1a-f1-f2-harness-report.md`
- وضعیت Git و diff فقط به‌صورت read-only بررسی شدند؛ هیچ stage/commit/branch انجام نشد.

نکته درباره دسترس‌پذیری منابع:

- پیش از این سند، گزارش دائمی مخصوص 1A در `docs/opencode-reports/` وجود نداشت. گزارش‌های بازبینی، طرح F1/F2، پیاده‌سازی و Harness صرفاً در `/tmp` نگهداری شده‌اند و بخشی از مخزن نیستند.
- خروجی خام کامل آزمون‌های HTTP تاریخ 2026-11-11 در مخزن موجود نیست؛ فقط خلاصه نتایج در `PROJECT_HISTORY.md` بخش 18.17 ثبت شده است.
- نتیجه Harness نیز از خروجی اجرای همان نوبت در `/tmp/core-banking-f1-f2-test/run.log` گرفته شده و در این سند خلاصه می‌شود.

## 2. دامنه پذیرفته‌شده

1A در دامنه محدود زیر پذیرفته می‌شود:

- `UnitOfWork` و مالکیت `begin/commit/rollback` برای مسیر Deposit؛ `TransactionService` و `TransactionRepository` هیچ Commit/Rollback مستقل ندارند.
- `TransactionService` و قرارداد `ITransactionRepository` در محدوده 1A: Lookup و Find با نتیجه Typed، تبدیل Fail-Closed وضعیت/نوع/کانال و Update با Expected Status.
- حفظ قرارداد API فعلی Deposit، الگوریتم Fingerprint، Canonicalization، دامنه Idempotency و رفتار Retry کلیدهای قدیمی.
- عدم تغییر SQL مالی و ترتیب عملیات Balance/Journal در Deposit.
- اصلاح محدود F1/F2 با گزینه 1:
  - وضعیت `RollbackFailed` مستقل از `CommitOutcomeUnknown`.
  - `RolledBack` فقط پس از موفقیت واقعی rollback.
  - پس از `CommitOutcomeUnknown` هیچ rollback خودکاری انجام نمی‌شود.
  - پس از شکست rollback هیچ retry خودکار انجام نمی‌شود.
  - در `CommitOutcomeUnknown`، `RollbackFailed` یا شکست BEGIN روی اتصال باز، اتصال مشترک در مرز امن بسته می‌شود و بدون بازگشایی خودکار بسته می‌ماند.
  - هر سه فراخوانی rollback در Deposit نتیجه را بررسی می‌کنند و شکست واقعی rollback با `DATABASE_ROLLBACK_FAILED` (Failed → HTTP 500 از نگاشت موجود) گزارش می‌شود.
  - `DATABASE_COMMIT_FAILED` در همه مسیرهای شکست commit حفظ شده است.

## 3. شواهد

### 3.1 آزمون‌های API قبلی (شاهد تاریخی، اجرا نشده در این نوبت)

طبق `docs/PROJECT_HISTORY.md` بخش 18.17 (تاریخ 2026-11-11):

- Deposit موفق روی حساب `1` با مبلغ `0.01` و کلید `stage1a-dep-success-20261111-a1` → `201`، `transactionId=2`، مبلغ/شناسه معتبر.
- Retry دقیق همان درخواست → همان `transactionId` و همان `transactionNumber`.
- همان کلید با مبلغ متفاوت → `422 IDEMPOTENCY_KEY_REUSED`.
- Retry کلید پیش از بازنویسی `deposit-main-20261026-a74c91` با Payload اصلی → همان `transactionId=1`.
- Deposit به حساب `CLOSED` → `422 ACCOUNT_NOT_ACTIVE` و Retry همان درخواست ردشده → `422 ACCOUNT_NOT_ACTIVE`.
- مبلغ `"0"` → `422 INVALID_AMOUNT`؛ درخواست بدون `idempotencyKey` → `400 INVALID_DEPOSIT_REQUEST`.
- دو درخواست پشت‌سرهم/هم‌زمان‌شده با کلید یکسان → هر دو همان `transactionId=4`؛ فقط یک ردیف Transaction ایجاد شد.
- کنترل read-only: Transaction `3` با `REJECTED` و بدون TransactionEntry/Journal؛ Transactionهای `2` و `4` با Journal `POSTED` و Debit/Credit برابر؛ `ledger_balance` حساب `1` برابر `1.02` و Active Holds برابر `0.52`.

این موارد شواهد تاریخی‌اند و در نوبت پذیرش دوباره اجرا نشده‌اند. آزمون هم‌زمانی واقعی نیز به‌دلیل سرور تک‌ریسمانی و Connection مشترک اثبات نشده است.

### 3.2 بررسی کد و Diff

- بازبینی متمرکز 1A در `/tmp/core-banking-1a-review.md` و متن کامل کد/ diff در `/tmp/core-banking-1a-review-code.txt` ثبت شده‌اند.
- بررسی کد نشان داد:
  - `TransactionService` و `TransactionRepository` هیچ `commit/rollback/transaction()` مستقل ندارند.
  - Lookup پیش از اعتبارسنجی شرایط متغیر حساب است.
  - تفکیک خطای Query از نبود رکورد در `findTransaction` انجام می‌شود.
  - Fingerprint و Advisory Lock قبلی حفظ شده‌اند.
  - SQL مالی و ترتیب Balance/Journal در diff تغییر نکرده‌اند.
- محدودیت: این بررسی، کدخوانی است و به‌تنهایی اثبات رفتار runtime نیست.

### 3.3 Build موفق پس از اصلاح F1/F2

در نوبت پیاده‌سازی اصلاح F1/F2، این دستور اجرا شد و موفق بود:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

در نوبت پذیرش، Build تکرار نشد. موفقیت Build فقط صحت کامپایل را نشان می‌دهد، نه رفتار شکست.

### 3.4 Harness هدفمند F1/F2

Harness مستقل فقط زیر `/tmp/core-banking-f1-f2-test/` ساخته و اجرا شد؛ `unitofwork.cpp` و `unitofwork.h` واقعی پروژه مستقیماً کامپایل شدند و هیچ کپی/شبیه‌سازی از UnitOfWork استفاده نشد.

- Qt: `5.15.8`
- کامپایلر: `g++ (Debian 12.2.0-14+deb12u1) 12.2.0`
- درایورها: `QSQLITE QPSQL QPSQL7`
- روش: `FakeDriver` از نوع `QSqlDriver` با شمارنده Begin/Commit/Rollback/Close، پرچم‌های شکست قابل‌کنترل و `QSqlError` مصنوعی غیرحساس. هر سناریو driver name و connection name یکتا داشت. cleanup خود Harness از close ناشی از UnitOfWork جدا بود و snapshot شمارنده‌ها پیش از cleanup گرفته شد.
- نتیجه کلی: `TOTAL FAILURES: 0` و `EXIT_CODE=0`.

شمارنده‌های واقعی:

| سناریو | نتیجه | begin | commit | rollback | close | openBefore | openAfter |
|---|---|---|---|---|---|---|---|
| A: commit ناموفق | PASS | 1 | 1 | 0 | 1 | true | false |
| B: rollback صریح ناموفق | PASS | 1 | 0 | 1 | 1 | true | false |
| C: rollback ناموفق در Destructor | PASS | 1 | 0 | 1 | 1 | true | false |
| D: BEGIN ناموفق | PASS | 1 | 0 | 0 | 1 | true | false |
| E1: commit موفق | PASS | 1 | 1 | 0 | 0 | true | true |
| E2: rollback صریح موفق | PASS | 1 | 0 | 1 | 0 | true | true |
| E3: rollback خودکار موفق در Destructor | PASS | 1 | 0 | 1 | 0 | true | true |

نکات تأییدشده:

- در A، rollback بعد از `CommitOutcomeUnknown` از API رد شد و به درایور نرسید (`rollback=0`)، و اتصال در خروج از scope بسته شد.
- در B، rollback دوم رد شد و شمارنده افزایش نیافت؛ Destructor دوباره rollback نکرد.
- در C، دقیقاً یک rollback ناموفق و سپس close رخ داد.
- در D، هیچ commit/rollback رخ نداد و close انجام شد.
- در E1/E2/E3، اتصال باز ماند و هیچ close اضافه‌ای رخ نداد.

### 3.5 آزمون QSQLITE برای shared-close

- `QSQLITE` در زمان اجرا موجود بود و آزمون واقعی اجرا شد.
- یک اتصال `:memory:` با دو کپی `QSqlDatabase` ساخته شد.
- Query روی کپی دوم پیش از close موفق بود.
- `close()` روی کپی اول باعث شد `isOpen()` کپی دوم false شود.
- Query روی کپی دوم پس از close شکست خورد.
- نتیجه: PASS.

این آزمون رفتار واقعی plugin QSQLITE در Qt را می‌سنجد، اما رفتار QPSQL/PostgreSQL را اثبات نمی‌کند.

### 3.6 یادداشت State خصوصی

ثبت State از کدخوانی و رفتار متناظر از Harness تأیید شده است؛ دسترسی مستقیم به private انجام نشده است. به‌ویژه `RollbackFailed` مستقیماً خوانده نشد و از رفتار عمومی (`isActive() == false`، رد شدن rollback دوم و عدم افزایش شمارنده درایور) استنباط شد.

## 4. محدودیت‌ها و ریسک‌های باز

- **شکست واقعی QPSQL/PostgreSQL:** رفتار COMMIT/ROLLBACK در قطع واقعی ارتباط یا خاتمه backend و نحوه گزارش Qt/QPSQL آزموده نشده است. Harness با FakeDriver کار می‌کند.
- **Retry پس از Commit نامعلوم:** بازیابی نتیجه پایدار از دیتابیس واقعی پس از restart/recovery آزموده نشده است.
- **توقف دسترسی تا restart:** پس از `CommitOutcomeUnknown`، `RollbackFailed` یا شکست BEGIN روی اتصال باز، اتصال مشترک بسته می‌ماند و بدون بازگشایی خودکار بسته می‌ماند. در نتیجه دسترسی دیتابیس سرویس تا restart متوقف می‌شود. این یک مصالحه fail-closed موقت است، نه معماری نهایی.
- **restart و Commit قبلی:** restart نتیجه Commit قبلی را تعیین نمی‌کند. Retry باید با همان `idempotencyKey` انجام شود؛ هیچ عملیات مالی خودکار با کلید جدید مجاز نیست.
- **مسیرهای legacy:** Transfer، Hold، AccountOpening، AccountService و چند Repository همچنان مستقیماً روی Connection مشترک تراکنش مدیریت می‌کنند و خارج از این اصلاح‌اند. برخی از آن‌ها پس از شکست commit هم rollback می‌کنند. ریسک مشترک اتصال باقی است.
- **مدیریت مرکزی اتصال/recovery:** گزینه 2 (مالکیت quarantine/recovery در `Database` و gate مشترک per-request) مرحله مستقل آینده است و اجرا نشده است.
- **هم‌زمانی واقعی:** سرور تک‌ریسمانی است و آزمون هم‌پوشانی واقعی دو تراکنش دیتابیس انجام نشده است.
- **آمادگی بهره‌برداری:** پذیرش محدود 1A معادل آمادگی کامل برای بهره‌برداری بانکی نیست.

## 5. وضعیت ادامه

- **1A:** پذیرفته‌شده در دامنه محدود، با محدودیت‌های مستند در همین سند.
- **1B:** شروع نشده و نیازمند تأیید جداگانه است.
- **Balance Slice:** شروع نشده است. شکاف هم‌زمانی قواعد مؤثر و نبود Override باید پیش از Balance Slice تعیین تکلیف شود.
- **بازآرایی گسترده `PROJECT_CONTEXT.md` و `PROJECT_HISTORY.md`:** همچنان کار مستقل است و در این نوبت انجام نشد.
