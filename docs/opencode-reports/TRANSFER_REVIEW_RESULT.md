# گزارش بازبینی محدود Transfer — 2026-11-16

نوع مرحله: بازبینی کد و تهیه برنامه اصلاح مبتنی بر شواهد. در این مرحله **هیچ کد، Build، تست مالی، restart سرویس، تغییر دیتابیس یا Git تغییر نکرد**. فقط همین گزارش ایجاد شد.

## ۱. SHA و وضعیت Git واقعی

- شاخه: `main`
- HEAD: `d0f53a4931ccf74175f9904176713dac025e0c9b`
- `origin/main`: همین `d0f53a4`
- Working tree: پاک (`git status --porcelain` بدون خروجی).
- Commitهای اخیر:
  - `d0f53a4` — `docs: record withdrawal completion and validation limits`
  - `56de608` — `feat(withdrawal): add atomic API with minimum balance policy and tests`
  - `d5a3149` — `docs: record deposit completion and balance design`
  - `8513718` — `fix(deposit): reject lossy numeric amount input`
  - `fd23100` — `refactor(deposit): isolate persistence behind application contracts`

بدون هیچ `stage`/`commit`/`push` یا تغییر فایل دیگر.

## ۲. نقشه کوتاه مسیر اجرا

```text
POST /api/v1/transfers
  → src/web/httprouter.cpp (route، 503 TRANSFER_SERVICE_UNAVAILABLE در صورت نبود Controller)
  → src/web/transfercontroller.cpp::transfer
  → src/application/transferservice.cpp::TransferService::transfer
  → تراکنش مستقیم روی QSqlDatabase مشترک (بدون UnitOfWork)
  → SQL روی: transaction، account، account_balance، account_hold،
             transaction_entry، journal، journal_entry، ledger_account_balance
  → پاسخ 201 موفق / 422 رد کسب‌وکاری / 500 خطای فنی
```

نکته مهم: برخلاف Deposit و Withdrawal، مسیر Transfer از `ITransactionUnitOfWork`، `ITransactionRepository`، `TransactionService` و Repositoryهای Application استفاده نمی‌کند و SQL مالی را مستقیماً در سرویس اجرا می‌کند.

## ۳. یافته‌های قطعی (تأییدشده در کد)

### C1 — خطر نبود تفکیک «نتیجه نامعلوم Commit» و rollback در آن وضعیت (شدت: زیاد)

- مکان:
  - `src/application/transferservice.cpp:911-916` (Commit نهایی):
    ```cpp
    if (!db.commit())
        return rollbackFailure(
            QStringLiteral("DATABASE_COMMIT_FAILED"),
            db.lastError().text(),
            transactionId, transactionNumber);
    ```
  - `src/application/transferservice.cpp:322-326` (rollbackFailure):
    ```cpp
    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0, const QString& number = QString()) {
        db.rollback();
        return failed(code, message, id, number);
    };
    ```
  - همچنین commitهای مسیر Replay/Rejected در `transferservice.cpp:378-382` و `:392-396` و `:483-486`.
- اثر: شکست `db.commit()` در اینجا به‌عنوان «شکست قطعی» فرض و سپس `db.rollback()` فراخوانی می‌شود. شکست Commit به‌تنهایی اثبات نمی‌کند که سرور واقعاً Rollback کرده است؛ نتیجه می‌تواند نامعلوم باشد. در چنین حالتی:
  - `rollback` پس از Commit ناموفق، ایمن نیست و می‌تواند وضعیت را مبهم‌تر کند.
  - هیچ حالت صریح `CommitOutcomeUnknown` وجود ندارد؛ نتیجه به کاربر «شکست» گزارش می‌شود در حالی که ممکن است عملیات واقعاً ثبت شده باشد.
  - هیچ Quarantine/بستن اتصال مشترک انجام نمی‌شود و `db.rollback()` بدون بررسی نتیجه رها می‌شود.
  - این با F1/F2 مصوب (`UnitOfWork`: `CommitOutcomeUnknown`، عدم Rollback پس از Commit نامعلوم، fail-closed) ناسازگار است.
- صورت‌بندی دقیق: این یافته **اثبات Commitment نامعلوم بودن یک تراکنش مشخص نیست**؛ یافته این است که کد برای وضعیت «Commit ناموفق» تفکیک نتیجه نامعلوم ندارد و در همان وضعیت Rollback می‌زند. این یک نقص کنترل‌جریان قطعی است.
- اصلاح محدود پیشنهادی (نیازمند تصمیم): حالت/نتیجه‌ای جدا برای Commit نامعلوم، عدم Rollback پس از آن، Quarantine/بستن اتصال، و عدم Retry خودکار با کلید جدید. گزینه کم‌هزینه: وصله هدفمند در Transfer؛ گزینه سازگار: مهاجرت Transfer به `ITransactionUnitOfWork`. انتخاب بین این دو نیازمند تأیید کاربر است.

### C2 — ناسازگاری حداقل مانده با سیاست تأییدشده (شدت: زیاد)

- مکان: `src/application/transferservice.cpp:122-134`
  ```sql
  COALESCE((SELECT o.minimum_balance FROM account_balance_rule_override o
             WHERE ... minimum_balance IS NOT NULL AND effective_from <= CURRENT_TIMESTAMP
               AND (effective_to IS NULL OR effective_to > CURRENT_TIMESTAMP)
             ORDER BY effective_from DESC LIMIT 1),
           (SELECT pbr.minimum_balance FROM product_balance_rule pbr
             WHERE ... effective_from <= CURRENT_TIMESTAMP
               AND (effective_to IS NULL OR effective_to > CURRENT_TIMESTAMP)
             ORDER BY effective_from DESC LIMIT 1), 0)
  ```
- اثر: `Override` جایگزین حداقل محصول می‌شود و می‌تواند آن را **کاهش** دهد؛ این با سیاست قطعی مصوب (`effective_minimum = MAX(product_minimum, account_override_minimum)`؛ محصول کف است و Override فقط افزایش‌دهنده) ناسازگار است. Withdrawal در `loadLockedAccountSnapshot` به `GREATEST(...)` اصلاح شده، اما Transfer SQL مستقل خود را دارد.
- اصلاح محدود پیشنهادی: استفاده از `GREATEST(COALESCE(product,0), COALESCE(override,0))` در همان Subquery، با حفظ `CURRENT_TIMESTAMP` و بازه‌های اعتبار. بدون تغییر سایر بخش‌ها.

### C3 — گردکردن بی‌صدای مبلغ در ورودی JSON number (شدت: زیاد)

- مکان: `src/web/transfercontroller.cpp:82-84`
  ```cpp
  const QString amount = amountValue.isString()
      ? amountValue.toString().trimmed()
      : QString::number(amountValue.toDouble(), 'f', 8);
  ```
- اثر: مبلغ JSON number با بیش از ۸ رقم اعشار پیش از اعتبارسنجی سرویس به ۸ رقم گرد می‌شود؛ مبلغی متفاوت از درخواست می‌تواند منتقل و در `request_fingerprint` ثبت شود. این همان کلاس نقصی است که در Deposit با `8513718` اصلاح شد. همچنین برای اعداد بزرگ JSON، تبدیل `double` می‌تواند دقت را از دست بدهد.
- اصلاح محدود پیشنهادی: همان گارد round-trip در Deposit (`canonical.toDouble() != numericAmount` → `422 INVALID_AMOUNT`) یا الزام رشته‌ای برای مبلغ. این یک تصمیم قرارداد API است و باید تأیید شود.

### C4 — عبور از State Machine تراکنش (شدت: متوسط)

- مکان: `src/application/transferservice.cpp:893-907` (`UPDATE transaction SET status='COMPLETED' ... WHERE id=:id AND status='INITIATED'`) و `persistRejection` در `:224-229` (اجازه `INITIATED` به `REJECTED`).
- مدل دامنه: `src/domain/transaction.cpp:139-146` فقط `INITIATED → PROCESSING/CANCELLED` و `PROCESSING → COMPLETED/REJECTED/...` را مجاز می‌داند.
- اثر: Transfer بدون `TransactionService` و بدون گذر از `PROCESSING` مستقیماً `INITIATED → COMPLETED` می‌کند؛ Schema هم Trigger محدودکننده ندارد. بنابراین قواعد چرخه عمر Domain در این مسیر اعمال نمی‌شوند.
- اصلاح محدود پیشنهادی: گذر از `INITIATED → PROCESSING → COMPLETED` (مانند Deposit/Withdrawal) یا استفاده از `TransactionService`. انتخاب رویکرد نیازمند تصمیم است.

### C5 — نشت متن خام خطای دیتابیس در پاسخ 500 (شدت: متوسط)

- مکان‌ها (نمونه): `src/application/transferservice.cpp:320, 338, 359, 381, 395, 423, 466, 486, 507, 528, 549, 632, 668, 686, 730, 754, 899, 914` (استفاده از `lastError().text()`).
- اثر: متن خام درایور/SQL می‌تواند به پاسخ API نشت کند؛ این با پیام‌های امن ثابت Deposit/Withdrawal ناسازگار است.
- اصلاح محدود پیشنهادی: نگاشت هر کد به پیام امن ثابت و نگه‌داشتن متن خام فقط در `qWarning`.

### C6 — نادیده‌گرفتن شکست Rollback و نبود حالت RollbackFailed (شدت: متوسط)

- مکان: `src/application/transferservice.cpp:322-326`.
- اثر: اگر `db.rollback()` شکست بخورد، نتیجه نادیده گرفته می‌شود و همان کد خطای اولیه برگردانده می‌شود؛ اتصال مشترک Quarantine نمی‌شود و ممکن است تراکنش باز/نامعلوم بماند.
- اصلاح محدود پیشنهادی: بررسی نتیجه Rollback، بازگشت `DATABASE_ROLLBACK_FAILED` و سیاست fail-closed.

### C7 — یکسان‌سازی «حساب بدون مانده» با «حساب ناموجود» (شدت: کم)

- مکان: `src/application/transferservice.cpp:566-569` همراه با JOIN داخلی `account_balance` در `loadLockedAccounts` (`:114-179`).
- اثر: اگر حساب وجود داشته باشد اما ردیف `account_balance` نداشته باشد، `accounts.size() != 2` شده و `ACCOUNT_NOT_FOUND` برگردانده می‌شود؛ Deposit/Withdrawal این حالت را با `ACCOUNT_BALANCE_NOT_INITIALIZED` جدا می‌کنند.
- اصلاح محدود پیشنهادی: LEFT JOIN یا Lookup جدا و کد خطای متمایز.

### C8 — نبود اعتبارسنجی طول description (شدت: کم)

- مکان: `src/application/transferservice.cpp:298` (بدون بررسی طول) و نبود بررسی در `transfercontroller.cpp`.
- اثر: description بیش از ۵۰۰ کاراکتر (محدودیت ستون) باعث خطای دیتابیس و پاسخ 500 با متن خام می‌شود، به‌جای رد 422 ساخت‌یافته.
- اصلاح محدود پیشنهادی: بررسی `description.size() <= 500` با کد `INVALID_DESCRIPTION`.

## ۴. موارد مشکوک / اثبات‌نشده (جدا از نقص‌های قطعی)

- **S1 — Lookup ارز حساب مبدأ پیش از قفل:** `transferservice.cpp:412-433` قبل از قفل‌ها اجرا می‌شود؛ اگر ارز حساب بین خواندن و قفل تغییر کند (هیچ API تغییری برای ارز حساب مشاهده نشد) ممکن است ناسازگاری رخ دهد. TOCTOU اثبات‌نشده.
- **S2 — تعامل تراکنش مستقیم با Connection مشترک:** `transferservice.cpp:318` روی Connection مشترک `db.transaction()` می‌زند؛ هم‌زمانی با UoW سایر سرویس‌ها آزموده نشده و سرور تک‌ریسمانی است. race اثبات نشده.
- **S3 — ترتیب قفل A→B و B→A:** از نظر کد، Accountها `ORDER BY id` (`:499`)، Balanceها `ORDER BY account_id` (`:520`) و Holdها `ORDER BY account_id, id` (`:541`) هستند و برای هر دو جهت یکسان‌اند؛ اما آزمون هم‌زمانی واقعی انجام نشده است.
- **S4 — بازمحاسبه `available_balance` مقصد:** `recalculateAvailableBalance` (`:243-276`) برای مقصد از اسنپ‌شات قبلی همان UoW استفاده می‌کند؛ فرمول با Deposit/Withdrawal هم‌راستاست، اما شاهد اجرایی مالی مستقل برای Transfer وجود ندارد.
- **S5 — Commit مسیر Replay:** در مسیر Replay یک تراکنش بدون نوشتن مالی Commit می‌شود (`:378`)؛ رفتار قراردادی موجود است اما آزمون مستقیم ندارد.
- **S6 — Fingerprint بدون ارز:** `fingerprintFor` (`:79-92`) ارز را شامل نمی‌شود (از حساب مبدأ مشتق می‌شود)؛ ریسک پایین، اثبات‌نشده.
- **S7 — نبود سقف مبلغ:** `normalizePositiveAmount` (`:51-77`) هیچ سقف بالایی ندارد؛ برای رشته دقیق است، اما برای JSON number دقت `double` می‌تواند از دست برود.
- **S8 — مسیر Rejected مستقیم از INITIATED:** `persistRejection` (`:224-229`) حالت `INITIATED → REJECTED` را در DB مجاز می‌کند؛ مرتبط با C4.

## ۵. موارد منطبق تأییدشده (برای ثبت)

- **`available_balance` مبنای مجوز نیست:** تصمیم وجه در `transferservice.cpp:611-627` با NUMERIC روی `ledger_balance - holds - minimum` و ظرفیت Overdraft است، نه `available_balance`.
- **Hold و minimum هر کدام یک بار:** در همان Query، `holds` یک بار و `minimum` یک بار کسر می‌شوند؛ دوبار کسر نمی‌شوند.
- **Overdraft موفق اعلام نشده:** مسیر نیازمند Overdraft در `:639-649` با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود؛ چرخه واقعی Overdraft پیاده/آزموده نشده است.
- **اتمیک‌بودن نسبی:** همه نوشتن‌های مالی در یک `db.transaction()` هستند (`:318` تا `:911`)؛ هرچند بدون UoW و با نقص Commit (C1/C6).
- **Idempotency:** Advisory Lock (`:329-339`)، lookup با `FOR UPDATE` (`:345-410`)، Replay/Conflict/InProgress و `IDEMPOTENCY_KEY_REUSED` پیاده شده‌اند.
- **ترتیب قفل صعودی:** برای دو حساب در هر جهت یکسان است (S3).

## ۶. تست‌های موجود و شکاف‌های پوشش

### موجود

- `tests/account_hold_persistence_test.cpp`
- `tests/balance_repository_unitofwork_test.cpp`
- `tests/deposit_orchestration_test.cpp`
- `tests/hold_event_fingerprint_and_validation_test.cpp`
- `tests/journal_repository_unitofwork_test.cpp`
- `tests/withdrawal_orchestration_test.cpp`

**هیچ فایل تست اختصاصی Transfer وجود ندارد.** شواهد تاریخی Transfer فقط در `docs/PROJECT_HISTORY.md` (بخش ۱۱ در 2026-10-26، بخش ۱۶.۲ تأیید کاربر 2026-10-28، بخش ۱۷ کارهای باقی‌مانده) آمده و شامل payload/پاسخ کامل و تطبیق مانده/Journal نیست.

### تست‌های لازم برای هر اصلاح

| اصلاح | تست پیشنهادی |
|---|---|
| C2 حداقل | آزمون fake برای محاسبه minimum + harness PostgreSQL با `VALUES/CTE` برای `MAX(product, override)` + (در صورت تأیید) ردیف Rule موقت در دیتابیس تست |
| C1/C6 Commit/Rollback | harness با درایور/اتصال جعلی برای اثبات عدم Rollback پس از Commit ناموفق، Quarantine و عدم Retry با کلید جدید |
| C3 مبلغ عددی | آزمون Controller برای رشته/عدد معتبر/۸ رقم/۹ رقم/عدد lossy/عدد بزرگ |
| C4 State machine | آزمون orchestration برای ترتیب `INITIATED → PROCESSING → COMPLETED` |
| C5 خطای امن | آزمون عدم نشت متن خام در پیام خطا |
| C7 مانده مقدارنیافته | آزمون QSQLITE/PostgreSQL برای حساب بدون `account_balance` و کد متمایز |
| C8 description | آزمون طول > ۵۰۰ → 422 |
| S3 قفل‌ها | آزمون orchestration ترتیب قفل؛ هم‌زمانی واقعی فقط با PostgreSQL و اتصال‌های موازی (نه سریالی) |

## ۷. تصمیم‌هایی که به تأیید کاربر نیاز دارند

- **D1 — رویکرد رفع Commit/F1-F2:** وصله هدفمند (حالت unknown + عدم Rollback + Quarantine) در `TransferService`، یا مهاجرت Transfer به `ITransactionUnitOfWork`. **تصمیم حداقل باز نگردد؛ این تصمیم جداست.**
- **D2 — قرارداد مبلغ عددی Transfer:** هم‌راستا کردن با گارد round-trip موجود Deposit، یا الزام رشته‌ای. تغییر قرارداد فعلی Transfer.
- **D3 — State machine:** گذر از `PROCESSING` در Transfer یا پذیرش رسمی رفتار فعلی.
- **D4 — ترتیب مرحله‌ها:** تأیید می‌کنید اصلاح حداقل (C2) اولین و محدود به یک فایل کد + تست باشد؟

## ۸. برنامه مرحله بعد (حداقل فایل‌ها)

پیشنهاد می‌شود اصلاح‌ها مرحله‌ای و جدا باشند؛ در هر مرحله فقط یک موضوع:

1. **T1 — حداقل مانده (کوچک‌ترین و قطعی):**
   - `src/application/transferservice.cpp` (فقط بلوک minimum در `loadLockedAccounts`)
   - تست جدید `tests/transfer_minimum_policy_test.cpp` + harness PostgreSQL read-only با `VALUES/CTE`
2. **T2 — Commit/F1-F2/Rollback (نیازمند D1):**
   - `src/application/transferservice.cpp`، `src/application/transferservice.h`، `src/web/transfercontroller.cpp` (نگاشت نتیجه جدید) و در صورت انتخاب، `src/infrastructure/unitofwork.*`
   - تست harness شبیه F1/F2
3. **T3 — مبلغ عددی (نیازمند D2):**
   - `src/web/transfercontroller.cpp` + تست Controller
4. **T4 — State machine، خطای امن، description و کد مانده (نیازمند D3):**
   - `src/application/transferservice.{h,cpp}`، `src/web/transfercontroller.cpp` + تست‌ها

در هیچ مرحله‌ای Deposit، Withdrawal و Hold نباید تغییر کنند.

## ۹. محدودیت‌های این بازبینی

- فقط کد و مستندات خوانده شد؛ هیچ Build، تست مالی، SQL نوشتنی، DDL، restart یا تغییر Git انجام نشد.
- هیچ Query به دیتابیس در این مرحله اجرا نشد؛ بنابراین هیچ ادعای رفتاری مبتنی بر داده واقعی مطرح نیست.
- آزمون هم‌زمانی واقعی انجام نشد؛ مسیر تک‌ریسمانی و Connection مشترک است و درخواست‌های سریالی race محسوب نمی‌شوند.
- تست fake/QSQLITE در هیچ‌جا معادل PostgreSQL یا Fault Injection معرفی نشده است.
- این گزارش آمادگی انتشار Transfer را تأیید نمی‌کند؛ فقط یافته‌ها و برنامه اصلاح را ثبت می‌کند.
