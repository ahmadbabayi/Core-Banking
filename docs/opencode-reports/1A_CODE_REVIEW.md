# 1A Code Review — Evidence (Permanent Copy)

> Source: `/tmp/core-banking-1a-review.md` (temporary working report).
> Type: read-only code/document review; not a new test execution.
> This permanent copy preserves the original report content. Dates and results are historical and were not re-run during documentation reorganization.
> Line numbers and Context section references in this report refer to the code/document state at review time; they were not silently rewritten during the later documentation reorganization.
> No credentials or secrets are included.

---

# بازبینی متمرکز و فقط‌خواندنی زیرمرحله ۱A

## ۰. دامنه، روش و محدودیت‌ها

- این نوبت فقط بازبینی read-only بود. هیچ کد، مستند، Schema یا داده‌ای تغییر نکرد.
- Build و آزمون‌های HTTP موفق قبلی دوباره اجرا نشدند. نتایج آزمون نقل‌شده از `docs/PROJECT_HISTORY.md` فقط به‌عنوان شاهد تاریخی ذکر می‌شوند، نه اجرای مجدد.
- Git base برای فایل‌های ردیابی‌شده: `HEAD = 69c1b44b94d9d62f91210b6e7b892609ff7da8ce` (شاخه `main`). فایل `depositservice.cpp` و `CMakeLists.txt` نسبت به همین `HEAD` diff معتبر دارند؛ مقایسه حدسی ساخته نشد.
- فایل‌های `unitofwork.*`، `transactionservice.*`، `transaction.*`، `itransactionrepository.h` و `transactionrepository.*` در وضعیت Git به‌صورت untracked/modified هستند؛ برای فایل‌های untracked مبنای «پیش از تغییر» در Git وجود ندارد و صرفاً متن کامل فعلی بررسی شده است. عدم وجود base برای فایل‌های جدید، محدودیت است و در گزارش با همین عنوان ذکر می‌شود.
- مسیرهای زیر مبنای بازبینی هستند:
  - `src/infrastructure/unitofwork.h`
  - `src/infrastructure/unitofwork.cpp`
  - `src/domain/transaction.h`
  - `src/domain/transaction.cpp`
  - `src/infrastructure/repository/itransactionrepository.h`
  - `src/infrastructure/repository/transactionrepository.h`
  - `src/infrastructure/repository/transactionrepository.cpp`
  - `src/application/transactionservice.h`
  - `src/application/transactionservice.cpp`
  - `src/application/depositservice.cpp`
  - `src/infrastructure/database.cpp` (فقط برای فهم مالکیت Connection)
  - `CMakeLists.txt`

---

## ۱. چرخه کامل UnitOfWork

### ۱.۱ وضعیت‌های داخلی و انتقال‌ها

وضعیت‌ها در `src/infrastructure/unitofwork.h:26-33`:

- `NotStarted` (مقدار اولیه، `unitofwork.h:38`)
- `Active`
- `Committed`
- `RolledBack`
- `CommitOutcomeUnknown`

انتقال‌ها:

| عملیات | از | به | محل |
|---|---|---|---|
| `begin()` موفق | `NotStarted` | `Active` | `unitofwork.cpp:36-43` |
| `begin()` تکراری/نامعتبر | هر چیزی جز `NotStarted` | بدون تغییر | `unitofwork.cpp:24-28` |
| `begin()` با Connection بسته | `NotStarted` | `NotStarted` (بدون تغییر) | `unitofwork.cpp:30-34` |
| `begin()` با شکست `transaction()` | `NotStarted` | `NotStarted` (بدون تغییر) | `unitofwork.cpp:36-40` |
| `commit()` موفق | `Active` | `Committed` | `unitofwork.cpp:54-64` |
| `commit()` ناموفق | `Active` | `CommitOutcomeUnknown` | `unitofwork.cpp:54-60` |
| `rollback()` موفق | `Active` | `RolledBack` | `unitofwork.cpp:82-91` |
| `rollback()` ناموفق | `Active` | `RolledBack` (همچنان ثبت می‌شود) | `unitofwork.cpp:82-88` |
| `rollback()` پس از `CommitOutcomeUnknown` | `CommitOutcomeUnknown` | بدون تغییر + خطا | `unitofwork.cpp:69-74` |
| `rollback()` پس از `Committed`/`RolledBack` | غیر `Active` | بدون تغییر + خطا | `unitofwork.cpp:76-80` |
| Destructor | فقط `Active` | `RolledBack` | `unitofwork.cpp:10-20` |
| Destructor | `CommitOutcomeUnknown` | بدون اقدام | `unitofwork.cpp:15` |

نتیجه: انتقال‌های عادی درست و قابل ردیابی‌اند؛ اما دو نقطه ضعف مهم دارد (بندهای ۱.۴ و ۱.۵).

### ۱.۲ جلوگیری از کپی یا مالکیت هم‌زمان یک تراکنش

- Copy constructor و copy assignment صریحاً `delete` شده‌اند: `unitofwork.h:13-14`.
- Move constructor/assignment به‌صورت صریح تعریف یا delete نشده‌اند. چون copy constructor توسط کاربر اعلام شده است، move constructor به‌طور ضمنی تولید نمی‌شود؛ بنابراین `UnitOfWork` در عمل non-movable هم هست. این نکته از نظر رفتاری درست است، اما صریح نیست و بهتر است بعداً `= delete` برای move هم نوشته شود (پیشنهاد، نه اصلاح فعلی).
- جلوگیری از «مالکیت هم‌زمان» فقط در سطح کپی شیء `UnitOfWork` انجام شده است. هیچ مکانیزمی وجود ندارد که مانع ساخت دو `UnitOfWork` مستقل روی همان Connection مشترک شود. `UnitOfWork` یک کپی از `QSqlDatabase` نگه می‌دارد (`unitofwork.h:37`) و `QSqlDatabase` در Qt به‌صورت implicitly shared است؛ بنابراین دو نمونه می‌توانند همان Connection فیزیکی را به اشتراک بگذارند. در این slice فقط یک `UnitOfWork` در `DepositService::deposit` ساخته می‌شود (`depositservice.cpp:232`)، اما این محدودیت را کد تضمین نمی‌کند.
- `database()` یک reference قابل تغییر برمی‌گرداند (`unitofwork.h:23`, `unitofwork.cpp:104-107`). بنابراین مصرف‌کننده می‌تواند `commit`/`rollback` را مستقیم روی Connection صدا بزند و مالکیت UnitOfWork را دور بزند. در کد فعلی این کار انجام نمی‌شود.

### ۱.۳ رفتار شکست `begin`

- اگر Connection معتبر/باز نباشد یا `m_database.transaction()` شکست بخورد، `m_state` روی `NotStarted` می‌ماند (`unitofwork.cpp:30-40`). این رفتار صحیح است: Destructor چیزی برای rollback نمی‌بیند.
- `DepositService` این حالت را به `TRANSACTION_START_FAILED` نگاشت می‌کند (`depositservice.cpp:235-237`).
- تفکیک «Connection بسته» از «شکست شروع تراکنش» در پیام خطا انجام می‌شود (`unitofwork.cpp:32` در برابر `unitofwork.cpp:38`)، اما هر دو در DepositService یک کد خطای واحد می‌گیرند. این یک ابهام عملیاتی کم‌شدت است.

### ۱.۴ رفتار `rollback` و شکست آن — یافته

- در `rollback()` مقدار بازگشتی `m_database.rollback()` در متغیر `rolledBack` نگه داشته می‌شود (`unitofwork.cpp:82`)، اما `m_state` **قبل از بررسی نتیجه** به `RolledBack` تغییر می‌کند (`unitofwork.cpp:83`).
- اثر: اگر rollback در سطح درایور شکست بخورد، شیء UnitOfWork ادعا می‌کند Rollback انجام شده است. سپس Destructor هیچ تلاش دوباره‌ای نمی‌کند (`unitofwork.cpp:15`) و فراخوان (`depositservice.cpp:244-248`) هم نتیجه rollback را نادیده می‌گیرد. نتیجه این است که تراکنش ممکن است باز بماند ولی لایه بالاتر گمان کند پاک شده است.
- شدت: متوسط تا بالا (بسته به رفتار درایور در شکست rollback).
- اثر: احتمال ادامه درخواست بعدی روی Connection دارای تراکنش باز، یا گزارش خطای کسب‌وکاری در حالی که وضعیت دیتابیس نامشخص است.
- کوچک‌ترین اصلاح پیشنهادی (اجرا نشده): `m_state` فقط در صورت موفقیت `m_database.rollback()` به `RolledBack` تغییر کند؛ در صورت شکست یک وضعیت صریح مانند `RollbackFailed`/`CommitOutcomeUnknown` ثبت شود و خطا به فراخوان برگردد. همچنین `rollbackFailure` در DepositService باید نتیجه rollback را بررسی و در صورت شکست کد خطای جداگانه برگرداند.

### ۱.۵ رفتار `commit` ناموفق و وضعیت Connection مشترک — یافته اصلی

- در `commit()` شکست به `CommitOutcomeUnknown` منتقل می‌شود (`unitofwork.cpp:54-60`). این تصمیم درست است که «نتیجه نامعلوم» را از «Rollback قطعی» جدا می‌کند.
- Destructor در این حالت rollback نمی‌کند (`unitofwork.cpp:15`, با توضیح `unitofwork.cpp:12-14`). این نیز به‌تنهایی منطقی است، چون rollback پس از commit ناموفق می‌تواند ادعای نادرست بسازد.
- **اما هیچ لایه‌ای مسئول پاک‌سازی یا قرنطینه Connection پس از `CommitOutcomeUnknown` نیست.**
  - `CommitOutcomeUnknown` فقط روی همان شیء UnitOfWork زنده است (`unitofwork.cpp:99-102`) و UnitOfWork در پایان درخواست از بین می‌رود.
  - `DepositService` در همه مسیرهای شکست commit مستقیماً `failed(...)` برمی‌گرداند و هیچ cleanup/quarantine انجام نمی‌دهد: `depositservice.cpp:269-272`، `depositservice.cpp:285-288`، `depositservice.cpp:361-364`، `depositservice.cpp:756-759`.
  - مالک Connection مشترک `Database` است (`src/infrastructure/database.cpp:29-46`) و `connect()` وقتی Connection از قبل باز است مستقیماً `true` برمی‌گرداند (`database.cpp:32-35, 42-45`). هیچ health check، reset یا reconnect اجباری وجود ندارد. (در این گزارش هیچ credential یا مقدار حساسی بازتولید نشده است.)
  - `QSqlDatabase` به‌صورت shared به `DepositService` و سپس به `UnitOfWork` کپی می‌شود (`depositservice.cpp:232`, `unitofwork.h:37`)، بنابراین وضعیت فیزیکی Connection بین درخواست‌ها مشترک است.
- نتیجه: «نتیجه عملیات قبلی نامعلوم است» به‌درستی ثبت می‌شود، اما «آماده‌بودن Connection برای درخواست بعدی» هیچ‌جا اثبات یا تضمین نمی‌شود. درخواست بعدی می‌تواند با Connection دارای تراکنش باز/aborted یا وضعیت نامعتبر ادامه دهد.
- شدت: بالا (High) در صورت وقوع شکست commit؛ در مسیر موفق اثری ندارد.
- اثر: احتمال خطاهای زنجیره‌ای، تراکنش باز باقی‌مانده، یا رفتار نامشخص در درخواست بعدی. ادعای «Rollback نشدن در Destructor صحت مسیر را ثابت می‌کند» نادرست است؛ فقط نشان می‌دهد ادعای نادرست Rollback ساخته نمی‌شود، ولی مسئله آمادگی Connection را حل نمی‌کند.
- کوچک‌ترین اصلاح پیشنهادی (اجرا نشده): تعریف صریح مالک cleanup (احتمالاً لایه Connection/Database) برای حالت `CommitOutcomeUnknown`؛ مثلاً غیرفعال‌کردن/بستن Connection و اجبار reconnect پیش از درخواست بعدی، یا health check اجباری (مثل `SELECT 1` و بررسی transaction state) در شروع هر Unit of Work. تا زمانی که این تصمیم گرفته نشود، نباید مسیرهای بعدی (۱B و Balance) را روی همین فرض بنا کرد.

### ۱.۶ تفکیک صریح دو مفهوم

- **نتیجه نامعلوم عملیات قبلی:** در `UnitOfWork::m_state == CommitOutcomeUnknown` بیان می‌شود (`unitofwork.cpp:54-60`, `unitofwork.cpp:99-102`). معنای آن این است: «نمی‌دانیم COMMIT در سرور انجام شد یا نه». این وضعیت فقط در طول عمر همان شیء UnitOfWork معنا دارد.
- **آماده‌بودن Connection برای درخواست بعدی:** هیچ وضعیت، پرچم، health check یا قراردادی برای آن وجود ندارد. `Database::connect()` صرفاً `isOpen()` را بررسی می‌کند (`database.cpp:32-35, 42-45`). `UnitOfWork::begin()` هم فقط `isValid()/isOpen()` را بررسی می‌کند (`unitofwork.cpp:30-34`).
- بنابراین این دو مفهوم در کد فعلی به‌درستی از هم جدا نشده‌اند؛ دومی عملاً تضمین نشده است.

### ۱.۷ نتیجه بخش ۱

چرخه عادی `begin -> commit/rollback` و جلوگیری از کپی درست پیاده‌سازی شده است. اما (الف) شکست rollback به‌اشتباه `RolledBack` ثبت می‌شود، و (ب) برای `CommitOutcomeUnknown` هیچ مالک cleanup/quarantine برای Connection مشترک وجود ندارد. مورد (ب) باید پیش از اتکا به این زیرساخت در مراحل بعد تعیین تکلیف شود.

---

## ۲. تطبیق جدول انتقال Transaction با سند مصوب

### ۲.۱ سند مصوب

- `docs/PROJECT_CONTEXT.md:304-320` (بخش ۱۰، قواعد کسب‌وکار):
  - `INITIATED -> PROCESSING` یا `INITIATED -> CANCELLED` (خط ۳۱۵)
  - `PROCESSING -> COMPLETED / REJECTED / FAILED / CANCELLED` (خط ۳۱۶)
  - `COMPLETED -> REVERSED` فقط از مسیر برگشت معتبر (خط ۳۱۷)
  - وضعیت نهایی دوباره وارد چرخه نمی‌شود (خط ۳۱۸)
- `docs/PROJECT_CONTEXT.md:725` (گزارش وضعیت ۱A): «جدول انتقال مجاز `INITIATED -> PROCESSING -> COMPLETED/REJECTED`».
- Schema فقط مجموعه مقادیر مجاز status را محدود می‌کند و جدول انتقال را اعمال نمی‌کند: `database/CURRENT_SCHEMA.sql:1515`.

### ۲.۲ کد فعلی

`src/domain/transaction.cpp:139-163`:

- `INITIATED -> PROCESSING || CANCELLED` (خط ۱۴۴)
- `PROCESSING -> COMPLETED || REJECTED || FAILED || CANCELLED` (خطوط ۱۴۶-۱۵۰)
- `COMPLETED -> REVERSED` (خطوط ۱۵۲-۱۵۳)
- `REJECTED/FAILED/CANCELLED/REVERSED -> false` (خطوط ۱۵۵-۱۵۹)
- مقدار ناشناخته `from` هم در انتها `false` برمی‌گرداند (خط ۱۶۲) — fail-closed.

### ۲.۳ پاسخ صریح درباره `INITIATED -> CANCELLED`

- بله، در کد **مجاز است**: `src/domain/transaction.cpp:144`.
- حذف نشده است؛ بنابراین «دلیل حذف» و «اثر حذف» وجود ندارد.
- این مجازبودن با سند کسب‌وکار بخش ۱۰ (`PROJECT_CONTEXT.md:315`) سازگار است.
- تنها نکته، ابهام مستنداتی است: خط `PROJECT_CONTEXT.md:725` فقط مسیر مصرف‌شده در Deposit را فهرست می‌کند (`INITIATED -> PROCESSING -> COMPLETED/REJECTED`) و می‌تواند به‌اشتباه «تنها انتقال مجاز مدل» خوانده شود، در حالی که مدل کامل‌تر است. این ابهام مستنداتی است، نه تغییر کد؛ در این نوبت سند تغییر داده نشد.
- `TransactionService` فقط سه انتقال را ارائه می‌دهد: `INITIATED->PROCESSING` (`transactionservice.cpp:100-162`)، `PROCESSING->COMPLETED` (`transactionservice.cpp:164-185`) و `PROCESSING->REJECTED` (`transactionservice.cpp:187-208`). انتقال‌های `CANCELLED`/`FAILED` در مدل هست ولی در این slice استفاده نمی‌شوند.

### ۲.۴ یافته در لایه Repository

- `TransactionRepository::updateStatus` هر جفت `expectedStatus/nextStatus` را می‌پذیرد و فقط با `WHERE id = :id AND status = :expected_status` و `numRowsAffected() == 1` کنترل می‌کند (`transactionrepository.cpp:454-525`, به‌خصوص ۴۹۶ و ۵۱۶). این تابع `Transaction::isAllowedTransition(expected, next)` را صدا نمی‌زند.
- اثر: اگر در آینده کسی `ITransactionRepository::updateStatus` را مستقیم با جفت نامعتبر صدا بزند (مثلاً `PROCESSING -> INITIATED`)، دامنه از نظر SQL ممکن است عبور کند و جدول انتقال در Domain دور زده شود. در کد فعلی فقط از طریق `TransactionService` استفاده می‌شود و این مسیرها معتبرند، بنابراین نقض فعلی رخ نمی‌دهد.
- شدت: پایین (Latent/Defense-in-depth).
- کوچک‌ترین اصلاح پیشنهادی (اجرا نشده): در `updateStatus` پیش از ساخت Query، `Transaction::isAllowedTransition(expectedStatus, nextStatus)` بررسی شود و در صورت false خطا برگردد.

---

## ۳. بازبینی مسیرهای Deposit

### ۳.۱ شروع و مالکیت

- اعتبارسنجی ورودی و fingerprint پیش از begin انجام می‌شود: `depositservice.cpp:201-230`.
- `UnitOfWork` روی `m_database` ساخته و `begin` می‌شود: `depositservice.cpp:232-237`.
- `QSqlDatabase& db = unitOfWork.database()`: `depositservice.cpp:239`.
- `TransactionRepository` و `TransactionService` با همان Connection ساخته می‌شوند: `depositservice.cpp:241-242`.
- Commit/Rollback مستقل در Service و Repository وجود ندارد؛ با grep روی این فایل‌ها هیچ `commit/rollback/transaction()` پیدا نشد. تنها مالک `UnitOfWork` است.

### ۳.۲ تمام خروج‌ها پس از `begin`

جدول زیر همه مسیرهای بازگشت پس از `begin` را فهرست می‌کند. «Rollback» یعنی `rollbackFailure` در `depositservice.cpp:244-248` (که نتیجه rollback را نادیده می‌گیرد) یا فراخوانی مستقیم `unitOfWork.rollback()`.

| # | شرط/محل | اقدام | خط |
|---|---|---|---|
| 1 | Lookup ناموفق | Rollback + `Failed` | `depositservice.cpp:255-256` |
| 2 | تعارض Fingerprint/Type | Rollback + `Rejected(IDEMPOTENCY_KEY_REUSED)` | `depositservice.cpp:258-264` |
| 3 | Replay موفق (`Completed`) | Commit + `Completed` | `depositservice.cpp:266-280` |
| 4 | Replay ردشده (`Rejected`) | Commit + `Rejected` | `depositservice.cpp:282-296` |
| 5 | رکورد در جریان (`InProgress`) | Rollback + `Failed(IDEMPOTENCY_IN_PROGRESS)` | `depositservice.cpp:298-303` |
| 6 | خطای Query ارز | Rollback + `Failed` | `depositservice.cpp:312-315` |
| 7 | نبود حساب در Query ارز | Rollback + `Rejected(ACCOUNT_NOT_FOUND)` | `depositservice.cpp:317-322` |
| 8 | شکست ایجاد Transaction/Processing | Rollback + `Failed(TRANSACTION_CREATE_FAILED)` | `depositservice.cpp:345-348` |
| 9 | شکست persist رد | Rollback + `Failed(REJECTION_PERSIST_FAILED)` | `depositservice.cpp:353-358` |
| 10 | شکست Commit در rejectAndCommit | بدون Rollback + `Failed(DATABASE_COMMIT_FAILED)` | `depositservice.cpp:360-364` |
| 11 | رد کسب‌وکاری موفق‌شده | Commit + `Rejected` | `depositservice.cpp:366` |
| 12 | خطای قفل account | Rollback + `Failed` | `depositservice.cpp:376-379` |
| 13 | خطای قفل account_balance | Rollback + `Failed` | `depositservice.cpp:393-396` |
| 14 | خطای قفل account_hold | Rollback + `Failed` | `depositservice.cpp:410-413` |
| 15 | خطای خواندن قفل‌شده | Rollback + `Failed` | `depositservice.cpp:423-426` |
| 16 | نبود balance | rejectAndCommit | `depositservice.cpp:428-431` |
| 17 | حساب غیرACTIVE | rejectAndCommit | `depositservice.cpp:433-436` |
| 18 | محصول غیرACTIVE | rejectAndCommit | `depositservice.cpp:438-441` |
| 19 | سرفصل مشتری غیرقابل ثبت | rejectAndCommit | `depositservice.cpp:443-449` |
| 20 | عدم تطابق ارز سرفصل | rejectAndCommit | `depositservice.cpp:451-454` |
| 21 | خطای Query سرفصل تسویه | Rollback + `Failed` | `depositservice.cpp:468-471` |
| 22 | نبود سرفصل تسویه | rejectAndCommit | `depositservice.cpp:473-476` |
| 23 | خطای init ledger balance | Rollback + `Failed` | `depositservice.cpp:501-504` |
| 24 | خطای قفل ledger balance | Rollback + `Failed` | `depositservice.cpp:521-524` |
| 25 | تعداد قفل ناکافی | Rollback + `Failed` | `depositservice.cpp:533-537` |
| 26 | خطای transaction_entry | Rollback + `Failed` | `depositservice.cpp:560-563` |
| 27 | خطای journal | Rollback + `Failed` | `depositservice.cpp:582-585` |
| 28 | خطای journal_entry | Rollback + `Failed` | `depositservice.cpp:617-620` |
| 29 | خطای کنترل توازن | Rollback + `Failed` | `depositservice.cpp:633-636` |
| 30 | نامتوازن‌بودن Journal | Rollback + `Failed` | `depositservice.cpp:638-642` |
| 31 | خطای به‌روزرسانی account_balance | Rollback + `Failed` | `depositservice.cpp:662-665` |
| 32 | خطای prepare به‌روزرسانی ledger | Rollback + `Failed` | `depositservice.cpp:698-703` |
| 33 | خطای exec به‌روزرسانی ledger | Rollback + `Failed` | `depositservice.cpp:712-717` |
| 34 | شکست POST شدن Journal | Rollback + `Failed` | `depositservice.cpp:736-742` |
| 35 | شکست complete | Rollback + `Failed` | `depositservice.cpp:751-753` |
| 36 | شکست Commit نهایی | بدون Rollback + `Failed(DATABASE_COMMIT_FAILED)` | `depositservice.cpp:755-759` |
| 37 | موفقیت | Commit + `Completed` | `depositservice.cpp:761` |

نتیجه: هیچ مسیر بازگشتی وجود ندارد که UnitOfWork را در حالت `Active` رها کند مگر از طریق Destructor؛ Destructor هم فقط Active را Rollback می‌کند. مسیرهای شکست Commit عمداً Rollback نمی‌کنند (بند ۱.۵).

### ۳.۳ مسیر Replay موفق

- `lockAndLookup` ابتدا Advisory Lock و سپس Lookup را انجام می‌دهد (`transactionservice.cpp:26-50`).
- اگر رکورد `COMPLETED` باشد، `existing.status = Completed` (`transactionservice.cpp:77-79`).
- Deposit بدون اعتبارسنجی مجدد حساب، `commit` می‌کند (`depositservice.cpp:266-280`). Commit صرفاً قفل‌ها را آزاد می‌کند؛ نوشتن مالی جدیدی انجام نمی‌شود.
- اگر Commit شکست بخورد به `CommitOutcomeUnknown` می‌رود و `Failed` برمی‌گردد (`depositservice.cpp:269-272`). این دقیقاً همان مسئله بند ۱.۵ است.
- صحت Replay از نظر «عدم تکرار مالی» با آزمون قبلی پشتیبانی می‌شود (`PROJECT_HISTORY.md:1364-1368`)، اما در این نوبت آزمون تکرار نشد.

### ۳.۴ مسیر Replay ردشده

- رکورد `REJECTED` به `existing.status = Rejected` نگاشت می‌شود (`transactionservice.cpp:81-83`).
- Deposit بدون اجرای دوباره منطق کسب‌وکار، `commit` و سپس `rejected` با `resultCode/resultMessage` قبلی برمی‌گرداند (`depositservice.cpp:282-296`).
- رفتار کلی درست است. همان مسئله Commit ناموفق در بند ۱.۵ صدق می‌کند.

### ۳.۵ مسیر تعارض Fingerprint

- تعارض وقتی رخ می‌دهد که `type != expectedType` یا `requestFingerprint != expectedFingerprint`: `transactionservice.cpp:68-73`.
- این بررسی **پیش از** switch وضعیت انجام می‌شود؛ بنابراین حتی اگر رکورد قبلی در وضعیت `PROCESSING` یا `REJECTED` باشد، تعارض محتوا اولویت دارد. این با قرارداد Idempotency سازگار است.
- Deposit تعارض را به `Rejected(IDEMPOTENCY_KEY_REUSED)` با `transactionId/Number` قبلی نگاشت می‌کند و Rollback می‌کند: `depositservice.cpp:258-264`.
- نکته: `getRequestFingerprint()` برای مقدار NULL در دیتابیس رشته خالی برمی‌گرداند (`transactionrepository.cpp:232`). برای ردیف‌های قدیمی که fingerprint دارند مشکلی نیست. برای ردیفی که fingerprint آن واقعاً NULL باشد، هر Retry با fingerprint غیرخالی به تعارض می‌خورد. این سناریو در داده‌های فعلی دیده نشده و آزمون نشده است.

### ۳.۶ تفکیک خطای Query از نبود رکورد

- `findTransaction` در `transactionrepository.cpp:250-280`:
  - شکست `query.exec()` → `Failed` با متن خطا (خط ۲۶۰-۲۶۱).
  - `!query.next()` → `NotFound` بدون خطا (خط ۲۶۳-۲۶۸).
  - خطای parse ردیف → `Failed` (خط ۲۷۳-۲۷۴).
- `TransactionService::lockAndLookup` این‌ها را به `Failed` و `NotFound` جدا نگاشت می‌کند (`transactionservice.cpp:38-50`).
- Deposit نیز `Failed` را با Rollback و `NotFound` را با ادامه مسیر ایجاد معامله مدیریت می‌کند (`depositservice.cpp:255-256` و ادامه).
- نتیجه: تفکیک درست است. این مورد کدخوانی است و آزمون اختصاصی خطای Query در این نوبت اجرا نشد.

### ۳.۷ حفظ دقیق Fingerprint و Advisory Lock قبلی

- تابع `fingerprintFor` بدون تغییر باقی مانده است؛ در diff فایل Deposit هیچ تغییری در بدنه آن دیده نمی‌شود (`depositservice.cpp:78-90`). Payload همان `DEPOSIT\n%1\n%2\n%3` با SHA-256 است.
- ورودی‌های fingerprint (accountId، amount نرمال‌شده، description trim‌شده) بدون تغییرند (`depositservice.cpp:205-230`).
- Advisory Lock قبلی با همان SQL و همان پیشوند `API:` به Repository منتقل شده است:
  - قبل: `SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))` با `"API:" + idempotencyKey` (طبق diff، حذف‌شده از Deposit).
  - بعد: `transactionrepository.cpp:294-307` دقیقاً همان SQL و همان پیشوند.
- نتیجه: الگوریتم fingerprint، canonicalization، دامنه Idempotency و کلید Advisory Lock حفظ شده‌اند.

### ۳.۸ انتقال وضعیت با expected status و تأیید affected rows

- `createProcessing` ابتدا `INITIATED` را با `createInitiated` درج می‌کند (`transactionservice.cpp:118-147`) و سپس `INITIATED -> PROCESSING` را با expected status انجام می‌دهد (`transactionservice.cpp:149-159`).
- `complete` و `reject` هر دو `PROCESSING` را به‌عنوان expected status می‌فرستند (`transactionservice.cpp:178-184` و `201-207`).
- `TransactionRepository::updateStatus` در SQL از `WHERE id = :id AND status = :expected_status` استفاده می‌کند (`transactionrepository.cpp:496`) و اگر `numRowsAffected() != 1` باشد خطا برمی‌گرداند (`transactionrepository.cpp:516-522`).
- نتیجه: expected status و تأیید affected rows درست پیاده‌سازی شده است.

### ۳.۹ خطا پس از نوشتن‌های مالی و قبل از Commit

- تمام نوشتن‌های مالی (transaction_entry، journal، journal_entry، account_balance، ledger_account_balance، POST journal) در یک UnitOfWork هستند.
- خطا در هر مرحله پس از نوشتن، به `rollbackFailure` می‌رود و Rollback صدا زده می‌شود: مسیرهای ۲۶ تا ۳۵ جدول ۳.۲.
- **اما** اگر `rollback()` خودش شکست بخورد، طبق بند ۱.۴ وضعیت به‌اشتباه `RolledBack` ثبت می‌شود و خطای Rollback به فراخوان منتقل نمی‌شود. بنابراین «Rollback صدا زده شد» با «Rollback قطعاً انجام شد» یکی نیست.
- آزمون قطعی این مسیر (شکست میانی + Rollback) در تاریخچه اجرا نشده است (`PROJECT_HISTORY.md:1403`). این مورد صرفاً با آزمون/Fault Injection قابل اثبات است و در این نوبت انجام نشد.

### ۳.۱۰ نبود Commit/Rollback مستقل در Service و Repository

- `TransactionService` هیچ `commit`/`rollback`/`transaction()` ندارد.
- `TransactionRepository` هیچ `commit`/`rollback`/`transaction()` ندارد.
- تنها محل‌های Commit/Rollback در `DepositService` هستند: `depositservice.cpp:246, 259, 269, 285, 318, 361, 756`.
- نتیجه: این بخش مطابق تصمیم ۱A است.

### ۳.۱۱ تأیید نبود تغییر در SQL و ترتیب Balance/Journal

- با diff نسبت به `HEAD`، hunkهای تغییر `depositservice.cpp` فقط این نواحی را پوشش می‌دهند:
  - includeها (`depositservice.cpp:1-5`)
  - حذف `persistRejection` (کد قبلی)
  - بلوک Idempotency/Transaction (`depositservice.cpp:229-367`)
  - حذف INSERT قدیمی transaction و UPDATE نهایی `INITIATED -> COMPLETED`
  - جایگزینی با `TransactionService::createProcessing/reject/complete`.
- بلوک‌های زیر در diff تغییری ندارند و متن فعلی همان SQL قبلی است:
  - قفل account/balance/hold: `depositservice.cpp:369-418`
  - `loadLockedAccount`: `depositservice.cpp:108-185` (بدون تغییر)
  - اعتبارسنجی‌های حساب/محصول/سرفصل: `depositservice.cpp:428-479`
  - init و قفل `ledger_account_balance`: `depositservice.cpp:481-538`
  - `transaction_entry`: `depositservice.cpp:540-564`
  - `journal`: `depositservice.cpp:566-588`
  - `journal_entry` و کنترل توازن: `depositservice.cpp:590-643`
  - به‌روزرسانی `account_balance`: `depositservice.cpp:645-666`
  - به‌روزرسانی `ledger_account_balance`: `depositservice.cpp:668-718`
  - POST کردن Journal: `depositservice.cpp:720-743`
- نتیجه: SQL و ترتیب عملیات Balance/Journal نسبت به قبل تغییر نکرده است. تنها تغییر منطقی، مسیر وضعیت Transaction است (`INITIATED -> PROCESSING -> COMPLETED/REJECTED` به‌جای `INITIATED -> COMPLETED` و UPDATE رد با `IN ('INITIATED','PROCESSING')`). این تغییر در محدوده مصوب ۱A است و در History ثبت شده است (`PROJECT_HISTORY.md:1344`).

---

## ۴. جمع‌بندی یافته‌ها بر اساس شدت

| # | یافته | شدت | محل | نیازمند آزمون؟ |
|---|---|---|---|---|
| F1 | پس از `CommitOutcomeUnknown` هیچ لایه‌ای Connection مشترک را پاک‌سازی/قرنطینه نمی‌کند؛ درخواست بعدی ممکن است روی Connection نامعتبر ادامه دهد | بالا | `unitofwork.cpp:54-60, 99-102`; `depositservice.cpp:269-272, 285-288, 361-364, 756-759`; `database.cpp:29-46` | بله؛ با Fault Injection اثبات نشده |
| F2 | `rollback()` حتی در شکست `m_database.rollback()` وضعیت را `RolledBack` ثبت می‌کند و نتیجه به فراخوان نمی‌رسد | متوسط/بالا | `unitofwork.cpp:82-89`; فراخوان `depositservice.cpp:244-248` | بله؛ شکست rollback آزمون نشده |
| F3 | Destructor نتیجه rollback را نادیده می‌گیرد و فقط Active را پاک می‌کند؛ این به‌تنهایی صحت مسیر Commit ناموفق را ثابت نمی‌کند | متوسط | `unitofwork.cpp:10-20` | خیر (کدخوانی) |
| F4 | مالکیت هم‌زمان UnitOfWork روی یک Connection مشترک در سطح کد جلوگیری نمی‌شود؛ `database()` هم دسترسی مستقیم به commit/rollback می‌دهد | متوسط (ریسک معماری) | `unitofwork.h:23, 37`; `unitofwork.cpp:104-107` | آزمون هم‌زمانی واقعی انجام نشده |
| F5 | `TransactionRepository::updateStatus` جدول انتقال Domain را اعمال نمی‌کند و هر جفت وضعیت را می‌پذیرد | پایین (Latent) | `transactionrepository.cpp:454-525` | خیر (کدخوانی) |
| F6 | `FAILED`/`CANCELLED`/`REVERSED` در Lookup به `InProgress` نگاشت می‌شوند و Replay ترمینال این وضعیت‌ها با `IDEMPOTENCY_IN_PROGRESS` پاسخ می‌گیرد | پایین/متوسط | `transactionservice.cpp:85-91` | بله برای رفتار انتهایی؛ در Deposit فعلی رخ نمی‌دهد |
| F7 | ابهام مستنداتی: خط `PROJECT_CONTEXT.md:725` فقط مسیر Deposit را می‌گوید و می‌تواند با جدول کامل بخش ۱۰ اشتباه شود؛ `INITIATED->CANCELLED` در کد مجاز است و حذف نشده | پایین (مستنداتی) | `PROJECT_CONTEXT.md:725` در برابر `transaction.cpp:144` | خیر |
| F8 | تفکیک پیام خطای «Connection بسته» و «شکست begin» در deposit یک کد واحد می‌گیرد | پایین | `unitofwork.cpp:30-40`; `depositservice.cpp:235-237` | خیر |

### مواردی که فقط با آزمون قابل اثبات‌اند و در این نوبت آزمون نشده‌اند

- Rollback قطعی پس از چند نوشتن مالی و پیش از Commit (شکست میانی). شاهد کدی وجود دارد، اما Fault Injection اجرا نشده است. (`PROJECT_HISTORY.md:1403`)
- رفتار واقعی Connection پس از Commit ناموفق و درخواست بعدی روی همان Connection. (`PROJECT_HISTORY.md:1404`)
- رفتار rollback ناموفق (F2). هیچ آزمونی ثبت نشده است.
- هم‌پوشانی واقعی دو تراکنش دیتابیس. سرور تک‌ریسمانی و Connection مشترک است؛ آزمون قبلی فقط Replay منطقی را نشان داد. (`PROJECT_HISTORY.md:1380`)
- Retry با شرایط متغیر حساب بین درخواست اول و Retry. (`PROJECT_HISTORY.md:1405`)

---

## ۵. حکم نهایی بازبینی ۱A

- چرخه عادی UnitOfWork، جلوگیری از کپی، تفکیک خطای Query از نبود رکورد، حفظ fingerprint/advisory lock، expected-status update و عدم وجود Commit/Rollback مستقل در Service/Repository درست‌اند.
- جدول انتقال Domain با قواعد بخش ۱۰ سازگار است و `INITIATED -> CANCELLED` مجاز است؛ حذف نشده و تغییری بدون تأیید سند انجام نشده است.
- دو ایراد نیازمند تصمیم/اصلاح پیش از اتکا در مراحل بعد هستند:
  1. **F1 (بالا):** فقدان مالک صریح برای cleanup/quarantine پس از `CommitOutcomeUnknown`.
  2. **F2 (متوسط/بالا):** ثبت نادرست `RolledBack` هنگام شکست rollback و نادیده‌گرفتن نتیجه آن.
- سایر موارد (F4 تا F8) ریسک‌های پایین‌تر یا ابهام مستنداتی‌اند و می‌توانند در مراحل بعد تعیین تکلیف شوند.
- هیچ اصلاحی در این نوبت انجام نشد. شروع ۱B مجاز نیست و منتظر تصمیم کاربر است.
