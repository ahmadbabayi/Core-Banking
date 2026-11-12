# 1A F1/F2 Implementation Report — Evidence (Permanent Copy)

> Source: `/tmp/core-banking-1a-f1-f2-implementation.md` (temporary working report).
> Type: implementation report for the approved minimal F1/F2 option; includes the build result from that turn.
> This permanent copy preserves the original report content. It is not a new implementation or test run.
> No credentials or secrets are included.

---

# پیاده‌سازی محدود F1/F2 — گزارش نهایی

> وضعیت: «پیاده‌سازی‌شده با اعتبارسنجی ناقص شکست». این گزارش به‌معنای پذیرش نهایی ۱A یا اثبات رفتار runtime شکست نیست.
>
> در این نوبت فقط سه فایل مجاز تغییر کردند. آزمون HTTP، تغییر داده، Fault Injection، قطع شبکه، خاتمه backend، restart، تغییر دیتابیس، test seam در کد تولیدی یا فایل تست در مخزن انجام نشد. Git index/Commit/Branch تغییر نکرد. هیچ credential یا مقدار حساسی در این گزارش نیست.

---

## ۱. فایل‌های تغییرکرده و محدوده

فایل‌های تغییرکرده در این نوبت (فقط همین سه فایل):

1. `src/infrastructure/unitofwork.h`
2. `src/infrastructure/unitofwork.cpp`
3. `src/application/depositservice.cpp`

تغییر نکردند: `database.*`، Router، Controllerها، `CMakeLists.txt`، مستندات مخزن، `TransactionService`، `TransactionRepository`، Transfer، Hold، افتتاح/بستن حساب، Schema، Migration و تنظیمات اتصال.

توجه: پیش از این نوبت، مجموعه‌ای از تغییرات ۱A و فایل‌های untracked در مخزن وجود داشت (`CMakeLists.txt`، مستندات، `transaction.*`، `itransactionrepository.h`، `transactionrepository.*`، `transactionservice.*`، `unitofwork.*`). این نوبت فقط سه فایل بالا را تغییر داد؛ بقیه همان تغییرات قبلی کاربر هستند.

---

## ۲. تغییرات دقیق UnitOfWork

### ۲.۱ `src/infrastructure/unitofwork.h`

- افزودن وضعیت مستقل `RollbackFailed` به enum: خط ۳۲.
- حفظ `CommitOutcomeUnknown` به‌صورت جدا: خط ۳۳.
- افزودن پرچم خصوصی مستقل برای نیاز به بستن اتصال در Destructor: خط ۴۰ (`m_closeConnectionOnDestruct`).
- هیچ API عمومی جدید یا getter اضافه نشد. API عمومی همان `begin/commit/rollback/isActive/isCommitOutcomeUnknown/database` است: خطوط ۱۶-۲۳.

### ۲.۲ `src/infrastructure/unitofwork.cpp`

- `#include <QDebug>` برای لاگ پاک‌سازی‌شده: خط ۳.
- Destructor: خطوط ۱۱-۳۹
  - فقط در وضعیت `Active` یک بار `rollback()` تلاش می‌شود: خطوط ۱۳-۱۵.
  - موفق → `RolledBack`: خط ۱۷.
  - ناموفق → `RollbackFailed` + پرچم بستن + `qWarning` بدون خطای خام: خطوط ۱۹-۲۶.
  - در `CommitOutcomeUnknown` یا `RollbackFailed` هیچ commit/rollback دوباره‌ای انجام نمی‌شود؛ فقط اتصال مشترک بسته می‌شود: خطوط ۲۹-۳۸.
  - کامنت قرارداد scope: خطوط ۳۳-۳۶ (بستن در Destructor پس از پایان Queryهای محلی Deposit؛ ادعای عمومی «همیشه ممنوع» نشده است).
- `begin()`: خطوط ۴۱-۶۷
  - فراخوانی روی وضعیت غیر `NotStarted` همان خطای قبلی است و با شکست واقعی BEGIN یکسان تلقی نمی‌شود: خطوط ۴۳-۴۷.
  - اتصال بسته/نامعتبر → `NotStarted` و خطای فعلی، بدون close اضافه: خطوط ۴۹-۵۳.
  - شکست `transaction()` روی اتصال باز → خطای اصلی قبل از cleanup ذخیره می‌شود (خط ۶۰)، وضعیت `NotStarted` می‌ماند و فقط پرچم بستن در مرز امن فعال می‌شود: خطوط ۵۵-۶۳.
- `commit()`: خطوط ۶۹-۸۹
  - شکست commit → ذخیره خطا قبل از cleanup (خط ۸۱) و `CommitOutcomeUnknown` (خط ۸۲) و پرچم بستن (خط ۸۳).
  - هیچ rollback خودکاری پس از شکست commit وجود ندارد.
- `rollback()`: خطوط ۹۱-۱۲۶
  - در `CommitOutcomeUnknown` → عدم تلاش rollback، ثبت نیاز به close: خطوط ۹۳-۹۹.
  - در `RollbackFailed` → عدم تلاش مجدد خودکار، ثبت نیاز به close: خطوط ۱۰۱-۱۰۷.
  - وضعیت غیر `Active` → همان خطای قبلی: خطوط ۱۰۹-۱۱۳.
  - شکست واقعی rollback → ذخیره خطای اصلی قبل از cleanup (خط ۱۱۸) و `RollbackFailed` (خط ۱۱۹) و پرچم بستن (خط ۱۲۰).
  - موفقیت واقعی → `RolledBack` فقط در همین حالت: خط ۱۲۴.
- لاگ‌ها فقط پیام ثابت و غیرحساس دارند؛ raw database error، SQL، payload، credential و connection string لاگ نمی‌شوند.

---

## ۳. تغییرات دقیق Deposit

### ۳.۱ افزودن بررسی نتیجه rollback

- `#include <QDebug>`: خط ۹.
- helper محلی `rollbackForError`: خطوط ۲۴۵-۲۵۷
  - نتیجه `unitOfWork.rollback(&rollbackError)` بررسی می‌شود.
  - خطای rollback پیش از close در Destructor در `rollbackError` گرفته می‌شود، اما متن خام آن لاگ نمی‌شود.
  - در شکست فقط `originalCode` و پیام ثابت quarantine لاگ می‌شود.
- `rollbackFailure` بازنویسی شد تا نتیجه rollback را بررسی کند: خطوط ۲۵۹-۲۶۸
  - شکست rollback → `Failed` با `DATABASE_ROLLBACK_FAILED` و پیام ثابت غیرحساس.
  - موفقیت rollback → همان پاسخ قبلی دقیقاً حفظ می‌شود.
- هر سه فراخوانی rollback اکنون از این helper عبور می‌کنند:
  1. داخل `rollbackFailure`: خطوط ۲۵۹-۲۶۸ (فراخوانی در خط ۲۶۱).
  2. شاخه `Conflict`: خطوط ۲۷۸-۲۸۹؛ بررسی rollback در خط ۲۷۹.
  3. شاخه `ACCOUNT_NOT_FOUND`: خطوط ۳۴۲-۳۵۱؛ بررسی rollback در خط ۳۴۳.
- تنها فراخوانی مستقیم `unitOfWork.rollback(...)` در فایل، داخل helper است (خط ۲۴۹). grep هیچ فراخوانی مستقیم دیگری نشان نمی‌دهد.

### ۳.۲ قرارداد خطا

- شکست واقعی rollback:
  - `code = DATABASE_ROLLBACK_FAILED`
  - `message = "Database transaction could not be safely rolled back"` (ثابت و غیرحساس)
  - `status = Failed`
  - HTTP 500 از طریق نگاشت موجود Controller بدون تغییر Controller.
- نگاشت Controller به‌صورت read-only بررسی شد: `DepositController` هر `DepositResult::Status::Failed` را به `500 Internal Server Error` نگاشت می‌کند (`src/web/depositcontroller.cpp:96-101`). بنابراین کد جدید بدون تغییر Controller قابل اجراست.
- `DATABASE_COMMIT_FAILED` در همه مسیرهای شکست commit حفظ شد:
  - Replay موفق: `depositservice.cpp:294-297`
  - Replay ردشده: `depositservice.cpp:310-313`
  - Reject-and-commit: `depositservice.cpp:391-393` (طبق grep)
  - Commit نهایی: `depositservice.cpp:785-788`
- مسیرهای عادی، شکل JSON، fingerprint، canonicalization، دامنه idempotency و رفتار Retry تغییر نکردند.
- هیچ عملیات مالی با کلید جدید و هیچ Retry داخلی خودکار اضافه نشد.

### ۳.۳ عدم اجرای SQL پس از نامطمئن‌شدن اتصال

- پس از شکست commit: مسیر بلافاصله `failed(...)` برمی‌گرداند و هیچ Query بعدی اجرا نمی‌شود (`depositservice.cpp:294-297, 310-313, 391-393, 785-788`).
- پس از شکست rollback: helper بلافاصله false برمی‌گرداند و مسیر بلافاصله `failed(...)` برمی‌گرداند؛ Query بعدی اجرا نمی‌شود.
- پس از شکست begin: `deposit()` بلافاصله `failed(...)` برمی‌گرداند (`depositservice.cpp:235-237`) و Query بعدی اجرا نمی‌شود.
- close فقط در Destructor انجام می‌شود، پس از پایان عمر Queryهای محلی؛ در لحظه rollback صریح با Queryهای زنده close انجام نمی‌شود.

### ۳.۴ عدم تغییر SQL مالی

بلوک‌های مالی و ترتیب آن‌ها دست‌نخورده‌اند. شماره خط فعلی:

- `loadLockedAccount`: خط ۱۰۹.
- قفل account/balance/hold: از خط ۳۹۸.
- `transaction_entry`: خط ۵۶۹.
- `journal`: خط ۵۹۹.
- `journal_entry` و کنترل توازن: خط ۶۲۲ و ۶۵۲.
- `account_balance`: خط ۶۷۴.
- `ledger_account_balance`: خط ۶۹۷.
- POST کردن Journal: خط ۷۴۹.

هیچ تغییر SQL یا بازچینش عملیات Balance/Journal در این نوبت انجام نشد.

---

## ۴. جدول نهایی Stateها و cleanup

| وضعیت فعلی | رویداد | وضعیت بعدی | اقدام Connection |
|---|---|---|---|
| `NotStarted` | `begin()` با اتصال معتبر/باز و BEGIN موفق | `Active` | هیچ |
| `NotStarted` | `begin()` با اتصال بسته/نامعتبر | `NotStarted` | هیچ (از قبل غیرقابل استفاده) |
| `NotStarted` | شکست `transaction()` روی اتصال باز | `NotStarted` + پرچم close | بستن در Destructor (مرز امن) |
| `Active` | `commit()` موفق | `Committed` | هیچ |
| `Active` | `commit()` ناموفق | `CommitOutcomeUnknown` + پرچم close | بستن در Destructor؛ بدون rollback |
| `Active` | `rollback()` موفق | `RolledBack` | هیچ |
| `Active` | `rollback()` ناموفق | `RollbackFailed` + پرچم close | بستن در Destructor |
| `Active` | Destructor: rollback موفق | `RolledBack` | هیچ |
| `Active` | Destructor: rollback ناموفق | `RollbackFailed` + پرچم close | بستن در همان Destructor |
| `CommitOutcomeUnknown` | `rollback()`/`commit()` دوباره | همان وضعیت + خطا | بستن؛ بدون تلاش مجدد commit/rollback |
| `RollbackFailed` | `rollback()`/`commit()` دوباره | همان وضعیت + خطا | بستن؛ بدون تلاش مجدد |
| `Committed`/`RolledBack` | `rollback()`/`commit()` | بدون تغییر + خطا | هیچ |

قواعد کلیدی:

- `RolledBack` فقط پس از `m_database.rollback() == true` ثبت می‌شود.
- `RollbackFailed` و `CommitOutcomeUnknown` دو وضعیت مستقل با معنای متفاوت‌اند.
- Destructor هیچ exception صریحی پرتاب نمی‌کند.
- اتصال مشترک پس از وضعیت نامطمئن بسته می‌شود و **بدون بازگشایی خودکار** بسته می‌ماند.

---

## ۵. نتیجه Build

دستور اجراشده:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق. هشدار کامپایلری در خروجی مشاهده نشد.

این Build فقط صحت کامپایل را نشان می‌دهد و اثبات رفتار runtime شکست نیست.

---

## ۶. بررسی حفظ قرارداد API

- `DATABASE_COMMIT_FAILED`: حفظ شد؛ HTTP 500 فعلی از نگاشت `Failed` بدون تغییر Controller.
- `DATABASE_ROLLBACK_FAILED`: کد جدید فقط در شکست واقعی rollback؛ `Failed` → HTTP 500 از نگاشت موجود `depositcontroller.cpp:96-101`. تغییر Controller لازم نبود و انجام نشد.
- در صورت موفقیت rollback، پاسخ‌های قبلی (شامل `IDEMPOTENCY_KEY_REUSED`، `ACCOUNT_NOT_FOUND` و سایر کدهای مسیرهای خطا) دقیقاً حفظ شدند.
- مسیرهای موفق، Replay، Conflict، رد کسب‌وکاری، شکل JSON، fingerprint، canonicalization، دامنه idempotency و رفتار Retry تغییر نکردند.
- هیچ پاسخ یا کد خطای مسیر عادی تغییر نکرد.
- بستن اتصال به‌معنای «Commit قبلی قطعاً انجام نشده» گزارش یا بازنمایی نمی‌شود.

---

## ۷. آزمون‌های اجراشده و اجرانشده

### اجراشده در این نوبت

1. بررسی متمرکز diff سه فایل (کدخوانی).
2. بررسی read-only نگاشت `DepositController` برای اطمینان از نگاشت `Failed` به HTTP 500.
3. یک Build موفق پس از تکمیل تغییرات.
4. بررسی grep برای اطمینان از اینکه هر سه rollback در Deposit بررسی می‌شوند و SQL مالی/ترتیب آن تغییر نکرده است.
5. بررسی Git status برای اطمینان از تغییرنکردن سایر فایل‌ها توسط این نوبت.

### اجرانشده در این نوبت

- آزمون HTTP و تغییر داده: اجرا نشد.
- Fault Injection، قطع شبکه، خاتمه backend، restart سرویس یا تغییر دیتابیس: اجرا نشد.
- test seam در کد تولیدی یا فایل تست در مخزن: اضافه نشد.
- اثبات runtime برای موارد زیر وجود ندارد: «شکست rollback به RolledBack ختم نشود»، «شکست commit موجب rollback بعدی نشود»، «close روی اتصال مشترک استفاده از کپی دیگر را متوقف کند»، «Destructor در مسیر شکست cleanup را انجام دهد».

بنابراین وضعیت این اصلاح: **پیاده‌سازی‌شده با اعتبارسنجی ناقص شکست**؛ نه پذیرفته‌شده نهایی.

---

## ۸. روش پیشنهادی آزمون هدفمند شکست (اجرا نشده — نیازمند تأیید)

### گزینه الف: Harness مستقل در `/tmp` با QSQLITE (بدون تغییر مخزن و بدون تماس با `corebanking`)

- فایل‌ها:
  - `/tmp/core-banking-f1-f2-test/main.cpp`
  - فایل build موقت در همان مسیر (اختیاری CMakeLists یا دستور مستقیم g++).
- وابستگی‌ها: Qt5Core، Qt5Sql و درایور `QSQLITE`. Connection از نوع `:memory:` با نام یکتا.
- اثرات: هیچ تغییری در کد تولیدی، دیتابیس `corebanking`، Schema یا مخزن ندارد.
- محدودیت: رفتار QSQLITE با QPSQL/PostgreSQL یکسان نیست؛ این harness فقط منطق state و shared-close در Qt را می‌سنجد، نه رفتار واقعی PostgreSQL.

موارد قابل بررسی:

1. **State honesty:** `begin` موفق → `Active`؛ `commit` موفق → `Committed`؛ `rollback` موفق → `RolledBack`؛ `isActive()` در وضعیت‌های نهایی false باشد.
2. **Rollback failure:** `begin`، سپس بستن Connection از یک کپی دیگر، سپس `rollback` → باید false برگرداند و وضعیت به `RolledBack` نرود؛ فراخوانی دوباره `rollback` باید false با پیام «Rollback already failed» بدهد (بدون تلاش مجدد خودکار).
3. **Commit failure:** `begin`، بستن Connection قبل از `commit`، سپس `commit` → باید false و `isCommitOutcomeUnknown() == true` باشد؛ سپس `rollback` باید false بدهد و هیچ rollback دوباره‌ای رخ ندهد.
4. **Destructor cleanup:** ساخت UnitOfWork در یک scope، `begin`، بستن Connection از کپی دیگر، خروج از scope → Destructor نباید crash یا exception بدهد؛ باید rollback را ناموفق ببیند، `qWarning` بدهد و Connection بسته بماند.
5. **Shared close:** دو `QSqlDatabase` با یک connection name؛ `close()` روی یکی؛ بررسی `isOpen() == false` روی دیگری. این مستقیماً فرض «close روی اتصال مشترک استفاده از کپی دیگر را متوقف می‌کند» را در سطح Qt می‌سنجد.

دستور پیشنهادی (اجرا نشده):

```bash
mkdir -p /tmp/core-banking-f1-f2-test
g++ -std=c++17 -fPIC /tmp/core-banking-f1-f2-test/main.cpp \
    /home/ahmad/projects/CoreBanking/src/infrastructure/unitofwork.cpp \
    $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
    -o /tmp/core-banking-f1-f2-test/test
/tmp/core-banking-f1-f2-test/test
```

پیش‌نیاز: نصب بودن plugin `QSQLITE` (با `QSqlDatabase::drivers()` بررسی شود). این دستور فقط پیشنهاد است و در این نوبت اجرا نشد.

### گزینه ب: Fault Injection واقعی روی PostgreSQL (نیازمند تأیید جداگانه)

برای اثبات رفتار واقعی QPSQL:

- یک اتصال اختصاصی تست به دیتابیس توسعه، سپس در لحظه commit/rollback قطع یا خاتمه backend (مثلاً با مکانیزم کنترل‌شده خارج از کد تولیدی)؛
- سپس بررسی state، بسته‌شدن Connection مشترک، fail-closed شدن درخواست بعدی، و Retry با **همان idempotency key** و بازیابی نتیجه پایدار از دیتابیس.

این گزینه با محدودیت‌های این نوبت (ممنوعیت خاتمه backend، تغییر دیتابیس، قطع شبکه و Fault Injection) اجرا نشد و نیازمند تأیید جداگانه است.

### گزینه ج: API-level بدون test seam

بدون یک test seam یا کنترل محیط، trigger قطعی commit/rollback failure از مسیر HTTP قابل انجام نیست. هر ادعای API-level بدون چنین کنترلی، اثبات‌شده نیست.

---

## ۹. محدودیت عملیاتی پذیرفته‌شده

- پس از شکست commit، شکست rollback یا شکست begin روی اتصال باز، اتصال مشترک بسته می‌شود و **بدون بازگشایی خودکار** بسته می‌ماند.
- چون `Database::connect()` فقط در startup فراخوانی می‌شود و سرویس‌ها از handle ذخیره‌شده استفاده می‌کنند، دسترسی دیتابیس سرویس تا **restart** متوقف می‌شود.
- این یک مصالحه موقت برای ایمنی (fail-closed) است، نه معماری نهایی مدیریت اتصال.
- Retry با همان idempotency key پس از recovery/restart همچنان معتبر است؛ هیچ عملیات مالی خودکار با کلید جدید انجام نمی‌شود. اما تا restart، سرویس در دسترس نیست.

---

## ۱۰. ریسک باز مسیرهای legacy و خارج‌بودن از دامنه

این اصلاح فقط شکست‌های `UnitOfWork` در Deposit را مهار می‌کند و ادعای حل سراسری مشکل اتصال مشترک ندارد.

ریسک‌های باز (خارج از این نوبت):

- `TransferService`، `HoldService`، `AccountOpeningService`، `AccountService` و چند Repository هنوز مستقیماً روی همان Connection مشترک تراکنش مدیریت می‌کنند.
- برخی از آن‌ها پس از شکست commit هم `rollback()` را صدا می‌زنند (مثلاً `transferservice.cpp:911-915`، `holdservice.cpp:824-828, 1283-1287`، `accountopeningservice.cpp:291-294`). این رفتار با تصمیم ۱A در Deposit ناسازگار است و در این نوبت اصلاح نشد.
- `begin()` ممکن است تراکنش بازِ باقی‌مانده از مسیر legacy را به‌طور قابل اتکا تشخیص ندهد؛ راه‌حل سراسری نیازمند گزینه ۲ (مالکیت quarantine/recovery در `Database` و gate مشترک per-request) است که مجاز نشده و اجرا نشد.
- `available_balance` همچنان cache کهنه‌شونده است و موضوع مرحله Balance باقی می‌ماند.

---

## ۱۱. تأیید عدم تغییر سایر فایل‌های مخزن توسط این نوبت

- فقط سه فایل مجاز توسط این نوبت تغییر کردند: `src/infrastructure/unitofwork.h`، `src/infrastructure/unitofwork.cpp`، `src/application/depositservice.cpp`.
- `database.*`، Router، Controllerها، `CMakeLists.txt`، مستندات، `TransactionService`، `TransactionRepository`، Transfer، Hold، افتتاح/بستن حساب، Schema، Migration و تنظیمات اتصال تغییر نکردند.
- سایر فایل‌های modified/untracked که در `git status` دیده می‌شوند، از قبل این نوبت وجود داشتند و توسط این نوبت دست نخوردند.
- Git index، Commit، Branch، Push یا stage انجام نشد.

---

## ۱۲. جمع‌بندی و توقف

- F1/F2 طبق گزینه ۱ حداقلی پیاده‌سازی شد: `RollbackFailed` مستقل، `RolledBack` فقط در موفقیت واقعی، عدم rollback پس از `CommitOutcomeUnknown`، قرنطینه fail-closed با بستن Connection مشترک در مرز امن، و بررسی هر سه فراخوانی rollback در Deposit.
- Build موفق بود.
- اعتبارسنجی runtime شکست انجام نشد؛ وضعیت اصلاح «پیاده‌سازی‌شده با اعتبارسنجی ناقص شکست» است.
- ۱A نهایی اعلام نمی‌شود و ۱B شروع نشد.
- منتظر تأیید برای اجرای Harness هدفمند (گزینه الف) یا Fault Injection واقعی (گزینه ب) هستم.
