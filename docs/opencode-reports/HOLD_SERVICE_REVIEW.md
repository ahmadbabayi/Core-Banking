# HoldService — Independent Technical Review

> تاریخ: 2026-11-01
>
> دامنه: `src/application/holdservice.h` و `src/application/holdservice.cpp`
>
> مبنا: `docs/opencode-reports/HOLD_API_DESIGN.md`
>
> این گزارش فقط بازبینی است. هیچ کد، Build، Migration یا DDL/DML اجرا نشد. تنها فایل ایجادشده همین گزارش است.

## خلاصه وضعیت

| # | مورد | وضعیت | خلاصه |
|---|---|---|---|
| 1 | تطابق `createHold` و `createEvent` با طراحی | **تأیید** | منطق اصلی مطابق است؛ چند انحراف جزئی مستندشده وجود دارد |
| 2 | Idempotency، Advisory Lock، Unique Conflict و SAVEPOINT | **تأیید** | منطق درست است؛ ابهام دامنه کلید بین Hold و Transaction در مورد ۱۰ آمده |
| 3 | ترتیب قفل، Deadlock و Race | **تأیید** | ترتیب Account → Balance → Hold رعایت شده و Lock Inversion دیده نشد |
| 4 | `activeHolds`، `remainingAmount` و ظرفیت Hold | **تأیید** | محاسبات مطابق VIEW و تصمیم طراحی است |
| 5 | Transaction، Rollback و خطاهای SQL/Trigger | **تأیید** | Rollback مسیرها کامل است؛ نگاشت متن خطای Trigger تا آزمون واقعی نامشخص است |
| 6 | Fingerprint و نرمال‌سازی ورودی‌ها | **تأیید** | مطابق بخش ۸ طراحی است؛ ابهام قرارداد زمان بدون Timezone باقی است |
| 7 | تفکیک `replayed` برای 200/201 | **تأیید** | در همه مسیرهای Retry درست مقداردهی شده است |
| 8 | تاریخ‌ها و انواع Release | **تأیید** | قواعد زمانی و مبلغی مطابق Trigger است؛ منبع زمان انحراف جزئی دارد |
| 9 | استفاده از `numRowsAffected` برای SELECT | **تأیید** | هیچ استفاده‌ای وجود ندارد |
| 10 | محدودیت‌های معماری و قراردادهای پروژه | **نامشخص** | چند تصمیم لازم برای مرحله Controller و دامنه Idempotency باز است |

---

## 1. تطابق کامل `createHold` و `createEvent` با طراحی

**وضعیت: تأیید (با انحرافات جزئی مستندشده)**

### `createHold`

- اعتبارسنجی ورودی: `holdservice.cpp:360-420` مطابق بخش ۱۱.۳ طراحی. افزوده‌ها: کنترل طول `reasonCode` در `holdservice.cpp:383-385` و کنترل طول Reference در `holdservice.cpp:391-393` با کد جدید `HOLD_INVALID_REFERENCE`.
- Fingerprint: `holdservice.cpp:416-420` و `holdservice.cpp:126-149`؛ ترتیب بخش ۸.۱ رعایت شده است: `HOLD → accountId → amount → reasonCode → reasonDescription → referenceType → referenceId → effectiveAt → expiresAt`.
- تراکنش: `holdservice.cpp:422-443`.
- Advisory Lock: `holdservice.cpp:445-456` با پیشوند `HOLD:`.
- Idempotency Lookup: `holdservice.cpp:458-504` روی `account_hold` با `FOR UPDATE`.
- تعیین زمان‌ها: `holdservice.cpp:506-516`.
- قفل ترتیبی: `holdservice.cpp:518-569`.
- بارگذاری وضعیت حساب: `holdservice.cpp:571-605`.
- کنترل موجودی: `holdservice.cpp:607-626`.
- INSERT با Idempotency: `holdservice.cpp:630-747`.
- خواندن وضعیت پیش از Commit: `holdservice.cpp:749-761`.
- Commit: `holdservice.cpp:763-766`.

### `createEvent`

- اعتبارسنجی: `holdservice.cpp:779-818`.
- پارس زمان پیش از Idempotency: `holdservice.cpp:820-829` مطابق بخش ۱۱.۴ گام ۲.
- Fingerprint: `holdservice.cpp:831-834` و `holdservice.cpp:151-172`؛ ترتیب بخش ۸.۲ رعایت شده است.
- تراکنش و Advisory Lock: `holdservice.cpp:836-872` با پیشوند `HOLD-EVENT:`.
- Idempotency Pre-check: `holdservice.cpp:874-955` پیش از اعتبارسنجی وضعیت/مبلغ.
- خواندن Hold: `holdservice.cpp:957-976`.
- قفل ترتیبی: `holdservice.cpp:978-1029`.
- بازخوانی وضعیت: `holdservice.cpp:1031-1042`.
- کنترل زمان: `holdservice.cpp:1044-1079`.
- کنترل مبلغ: `holdservice.cpp:1081-1119`.
- INSERT و مدیریت Trigger: `holdservice.cpp:1121-1184`.
- خواندن وضعیت پیش از Commit و Commit: `holdservice.cpp:1186-1203`.

### انحرافات جزئی نسبت به متن طراحی

1. **منبع زمان پیش‌فرض:** طراحی در بخش‌های ۱۱.۳ گام ۶ و ۱۱.۴ گام ۱۰ از `CURRENT_TIMESTAMP` دیتابیس نام برده؛ پیاده‌سازی از `QDateTime::currentDateTimeUtc()` استفاده می‌کند (`holdservice.cpp:506-511` و `1044-1049`). روی همان Host عملاً یکسان است ولی تطابق حرف‌به‌حرف کامل نیست.
2. **SELECT گام ۷ رویداد:** طراحی `account_id, amount, effective_at, expires_at` را خواسته بود؛ پیاده‌سازی فقط `account_id` را می‌خواند (`holdservice.cpp:959-963`). چون وضعیت مرجع در گام ۹ از VIEW خوانده می‌شود (`holdservice.cpp:1035`)، اثر عملکردی ندارد.
3. **پارس زمان در `createHold` پیش از Idempotency:** طراحی گام ۶ را بعد از Idempotency گذاشته بود؛ پیاده‌سازی برای Fingerprint نرمال‌شده در `holdservice.cpp:402-420` این کار را قبل انجام می‌دهد. چون اولین درخواست با زمان نامعتبر هرگز ردیف موفق ایجاد نمی‌کند، رفتار Retry تغییر نمی‌کند.
4. **افزودن `replayed`:** `holdservice.h:18` و `holdservice.h:33`؛ برای تفکیک 201/200 لازم است ولی در ساختار طراحی نبود.
5. **افزودن `HOLD_INVALID_REFERENCE`:** `holdservice.cpp:391-393` و `809-811`؛ برای جلوگیری از خطای 500 ناشی از طول ستون‌ها لازم است.
6. **SAVEPOINT:** `holdservice.cpp:630-683`؛ برای ادامه تراکنش پس از خطای PostgreSQL در مسیر Unique Conflict لازم است و در طراحی صریحاً نیامده بود.

**جمع‌بندی:** منطق اصلی بدون تناقض است. موارد بالا یا مستندشده‌اند یا اثر عملکردی ندارند؛ فقط مورد ۱ برای تطابق کامل نیازمند تصمیم است.

---

## 2. Idempotency در درخواست‌های تکراری و هم‌زمان

**وضعیت: تأیید**

- Advisory Lock ایجاد Hold: `holdservice.cpp:445-456` با `pg_advisory_xact_lock(hashtextextended(:key, 0))` و پیشوند `HOLD:`؛ مطابق بخش ۱۵ طراحی.
- Advisory Lock رویداد: `holdservice.cpp:860-872` با پیشوند `HOLD-EVENT:`.
- بررسی نتیجه قبلی Hold: `holdservice.cpp:458-504`؛ `exec()` از «صفر ردیف» جدا شده و وجود ردیف با `next()` سنجیده می‌شود.
- بررسی نتیجه قبلی Event: `holdservice.cpp:874-955`؛ تطبیق نوع، مبلغ (فقط PARTIAL)، زمان (فقط اگر ارسال شده باشد)، Reference با NULL-safe equality و Reason.
- Unique Conflict و SAVEPOINT: `holdservice.cpp:630-734`. بعد از خطای `23505` با `ROLLBACK TO SAVEPOINT hold_insert` تراکنش از حالت Aborted خارج می‌شود و سپس ردیف رقیب خوانده می‌شود. بدون SAVEPOINT، Query بعدی در PostgreSQL شکست می‌خورد؛ بنابراین این افزودن فنی ضروری و درست است.
- Trigger RETURN NULL: `holdservice.cpp:1160-1183`؛ Event موجود با `(hold_id, idempotency_key)` خوانده می‌شود و نتیجه قبلی با `replayed = true` برمی‌گردد.

### نکات

- پیشوند Advisory Lock رویداد شامل `holdId` نیست (`holdservice.cpp:866`). مطابق طراحی است، ولی دو Hold مختلف با کلید یکسان را بی‌دلیل سریال می‌کند؛ خطای صحت ایجاد نمی‌کند.
- دامنه یکتایی کلید بین Hold و Transaction (Deposit/Transfer) در مورد ۱۰ بررسی شده است.

---

## 3. ترتیب قفل‌ها، Deadlock و Race Condition

**وضعیت: تأیید**

- `createHold`: Account → Balance → Holds با `ORDER BY id` در `holdservice.cpp:518-569`.
- `createEvent`: Account → Balance → Hold در `holdservice.cpp:978-1029`.
- این ترتیب با Deposit (`src/application/depositservice.cpp:420-469`) و Transfer (`src/application/transferservice.cpp:491-555`) یکسان است و مطابق بخش‌های ۴.۳ و ۱۱.۵ طراحی.
- Lock Inversion دیده نشد:
  - در `createHold` اگر ردیف Idempotency موجود باشد، مسیر بلافاصله Commit می‌شود و قفل Account گرفته نمی‌شود (`holdservice.cpp:473-503`).
  - در `createEvent` اگر Event موجود باشد، مسیر بلافاصله Commit می‌شود و قفل Account گرفته نمی‌شود (`holdservice.cpp:917-954`).
  - بنابراین هیچ تراکنشی هم‌زمان قفل ردیف Idempotency را نگه ندارد و منتظر قفل Account بماند.
- سریال‌شدن تغییرات Holdها روی یک حساب از طریق قفل ردیف Account تضمین می‌شود؛ همه مسیرهای Hold، Deposit و Transfer ابتدا Account را قفل می‌کنند.
- مسیر Unique Conflict پس از `ROLLBACK TO SAVEPOINT` فقط خواندن و Commit انجام می‌دهد (`holdservice.cpp:685-729`) و ترتیب قفل جدیدی ایجاد نمی‌کند.

---

## 4. `activeHolds`، `remainingAmount` و ظرفیت ایجاد Hold

**وضعیت: تأیید**

- `activeHolds` از VIEW محاسبه می‌شود: `holdservice.cpp:196-197` با `SUM(hs.currently_blocked_amount)` روی `account_hold_status`. این مقدار برای Holdهای SCHEDULED و EXPIRED صفر است و برای بقیه `GREATEST(remaining_amount, 0)` است؛ مطابق بخش ۶.۳ طراحی.
- `ledgerBalance` مستقیماً از `account_balance` خوانده می‌شود: `holdservice.cpp:195`.
- ظرفیت Hold: `holdservice.cpp:609-625` با شرط `amount <= ledger_balance - active_holds`. این دقیقاً تصمیم «بدون Overdraft» و `AvailableForHold = Ledger Balance - Active Holds` در بخش‌های ۷ و ۹ طراحی است.
- `remainingAmount` از VIEW خوانده می‌شود: `holdservice.cpp:254-255` و در کنترل مبلغ استفاده می‌شود: `holdservice.cpp:1084-1118`.
- مبلغ FULL/EXPIRY/CANCELLATION از `remainingAmount` گرفته می‌شود و مبلغ کلاینت نادیده گرفته می‌شود: `holdservice.cpp:1116-1118`.
- مبلغ PARTIAL با مقایسه NUMERIC دقیق کنترل می‌شود: `holdservice.cpp:1086-1113`.

### نکات طراحی

- حداقل مانده (`minimum_balance`) در ظرفیت Hold لحاظ نشده است. این مطابق تصمیم صریح طراحی است ولی با منطق Transfer تفاوت دارد و باید در قرارداد کسب‌وکار تأیید بماند.
- `available_balance` ذخیره‌شده پس از ایجاد/آزادسازی Hold به‌روزرسانی نمی‌شود؛ این محدودیت در بخش ۷ طراحی ثبت شده و سرویس مطابق آن عمل کرده است.
- Holdهای SCHEDULED در `activeHolds` شمرده نمی‌شوند؛ بنابراین می‌توان Hold جدیدی ایجاد کرد که پس از فعال‌شدن Hold آینده، مجموع Holdها از موجودی بیشتر شود. این رفتار مطابق طراحی فعلی است ولی یک ریسک کسب‌وکاری است.

---

## 5. مدیریت Transaction، Rollback و خطاهای SQL و Trigger

**وضعیت: تأیید (با یک مورد نامشخص در نگاشت خطای Trigger)**

- Lambdas خطا در `createHold`: `holdservice.cpp:428-443`.
- Lambdas خطا در `createEvent`: `holdservice.cpp:842-858`.
- مسیرهای اصلی Rollback:
  - خطای Idempotency Lookup: `holdservice.cpp:468-471` و `912-915`.
  - تعارض Fingerprint/پیلود: `holdservice.cpp:477-483` و `928-934`.
  - خطای قفل‌ها: `holdservice.cpp:525-533`, `543-551`, `561-564`, `987-1028`.
  - خطای خواندن وضعیت: `holdservice.cpp:575-582`, `749-761`, `1035-1042`, `1186-1198`.
  - خطای موجودی: `holdservice.cpp:617-625`.
  - خطای INSERT Hold: `holdservice.cpp:672-738`.
  - خطای INSERT Event: `holdservice.cpp:1148-1158`.
  - خطای Commit: `holdservice.cpp:440-443` و `854-858` (Rollback صریح + `DATABASE_COMMIT_FAILED`).
- نگاشت خطای Trigger: `holdservice.cpp:284-307` و `337-340`.
  - کدهای شناخته‌شده دقیقاً به همان نام RAISE نگاشت می‌شوند.
  - `ACCOUNT_HOLD_IS_IMMUTABLE` و `ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE` در لیست نیستند و به `HOLD_EVENT_CREATE_FAILED` می‌رسند؛ مطابق تبصره بخش ۱۴.۲ طراحی.
- **مورد نامشخص:** نگاشت خطا فقط بر اساس `QSqlError::text()` انجام می‌شود (`holdservice.cpp:286`). طراحی «متن/SQLSTATE» را مجاز دانسته ولی تا اجرای واقعی یک Trigger از طریق API، اطمینان قطعی وجود ندارد که متن خطای QPSQL همیشه شامل نام RAISE باشد. پیشنهاد: در مرحله Controller حداقل یک سناریوی Trigger آزمون شود.
- INSERT Event بدون SAVEPOINT است (`holdservice.cpp:1124-1183`). این درست است: خطای Trigger کل تراکنش را Abort می‌کند و سرویس Rollback می‌کند؛ مسیر `RETURN NULL` خطا نیست و خواندن بعدی معتبر است.

---

## 6. صحت Fingerprint و نرمال‌سازی ورودی‌ها

**وضعیت: تأیید (با یک ابهام قراردادی)**

- نرمال‌سازی مبلغ: `holdservice.cpp:84-106`؛ همان الگوی `DepositService`/`TransferService` (حداکثر ۸ رقم اعشار، حذف صفرهای انتهایی، رد صفر).
- پارس زمان: `holdservice.cpp:108-124`؛ ابتدا `ISODateWithMs` و سپس `ISODate`، نرمال‌سازی به UTC با میلی‌ثانیه.
- Fingerprint Hold: `holdservice.cpp:126-149` و فراخوانی `416-420`؛ مطابق بخش ۸.۱.
- Fingerprint Event: `holdservice.cpp:151-172` و فراخوانی `831-834`؛ مطابق بخش ۸.۲.
- مبلغ در Fingerprint رویداد فقط برای PARTIAL وارد می‌شود: `holdservice.cpp:832`.
- زمان در Fingerprint هر دو عملیات فقط در صورت ارسال وارد می‌شود: `holdservice.cpp:419-420` و `833`.
- رشته‌های اختیاری پس از `trim()` وارد Fingerprint می‌شوند: `holdservice.cpp:365-369` و `783-787`.

### ابهام‌ها

- زمان بدون Timezone (مثلاً `2026-11-01T10:00:00`) توسط Qt به‌عنوان زمان محلی تفسیر می‌شود. طراحی نمونه‌های دارای Offset داده است؛ بهتر است قرارداد Controller ارسال Offset اجباری کند. این مورد در سطح سرویس نامشخص/قراردادی است، نه باگ.
- بخش ۸.۱ طراحی زمان را در Fingerprint Hold می‌آورد، ولی جدول بخش ۱۱.۷ عبارت «Retry بدون time → 200» را برای هر دو عملیات ذکر کرده است. پیاده‌سازی از بخش ۸.۱ پیروی می‌کند. اگر منظور طراحی نادیده‌گرفتن تغییر زمان در Retry Hold بوده، نیازمند تصمیم و اصلاح است. در حالت «هر دو درخواست بدون زمان» رفتار فعلی درست است.

---

## 7. صحت تفکیک `replayed` برای پاسخ‌های 200 و 201

**وضعیت: تأیید**

- تعریف فیلد: `holdservice.h:18` و `holdservice.h:33`.
- مقداردهی `HoldResult`:
  - Retry از Idempotency Lookup: `holdservice.cpp:502` → `true`.
  - Retry از مسیر Unique Conflict: `holdservice.cpp:728` → `true`.
  - ایجاد جدید: `holdservice.cpp:766` → `false`.
- مقداردهی `HoldEventResult`:
  - Retry از Idempotency Pre-check: `holdservice.cpp:953` → `true`.
  - Retry ناشی از `RETURN NULL` تریگر: `holdservice.cpp:1179-1180` → `true`.
  - ایجاد جدید: `holdservice.cpp:1203` → `false`.
- این فیلد در طراحی اولیه نبود ولی برای نگاشت 201/200 در Controller لازم است و در گزارش پیاده‌سازی مستند شده است.
- برای نتایج `Rejected`/`Failed` مقدار `replayed` پیش‌فرض `false` است و کاربردی ندارد.

---

## 8. مدیریت تاریخ‌ها و تمام انواع Release

**وضعیت: تأیید (با انحراف جزئی در منبع زمان)**

- ایجاد Hold:
  - `effectiveAt`/`expiresAt` اختیاری: `holdservice.cpp:395-414`.
  - کنترل `expiresAt > effectiveAt`: `holdservice.cpp:513-516`.
  - درج با `CAST(... AS TIMESTAMPTZ)`: `holdservice.cpp:665-668`.
- ایجاد Event:
  - زمان پیش‌فرض و نرمال‌شده: `holdservice.cpp:1044-1049`.
  - `effectiveAt > now` → `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED`: `holdservice.cpp:1054-1057`.
  - `now < hold.effective_at` → `ACCOUNT_HOLD_NOT_YET_EFFECTIVE`: `holdservice.cpp:1059-1062`.
  - `effectiveAt < hold.effective_at` → `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME`: `holdservice.cpp:1064-1067`.
  - `EXPIRY_RELEASE` فقط پس از انقضا: `holdservice.cpp:1069-1073`.
  - سایر Releaseها روی Hold منقضی‌شده رد می‌شوند: `holdservice.cpp:1074-1078`.
- انواع Release:
  - `PARTIAL_RELEASE`: مبلغ مثبت، `< remaining`، و رد `== remaining` و `> remaining`: `holdservice.cpp:1104-1115`.
  - `FULL_RELEASE`/`EXPIRY_RELEASE`/`CANCELLATION_RELEASE`: مبلغ برابر `remaining`: `holdservice.cpp:1116-1118`.
  - `remaining <= 0` → `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`: `holdservice.cpp:1099-1102`.
- ترتیب کنترل‌ها با Trigger موجود (`database/schema/015_account_holds.sql:160-219`) هم‌خوان است.

### انحراف جزئی

- مقایسه‌های زمانی از ساعت برنامه (`QDateTime::currentDateTimeUtc`) استفاده می‌کنند، در حالی که Trigger از `clock_timestamp()` استفاده می‌کند. روی همان Host اختلاف عملاً صفر است؛ در محیط با Clock Skew ممکن است در مرز زمان اختلاف ایجاد شود. برای تطابق کامل با طراحی/Trigger، استفاده از زمان دیتابیس ترجیح دارد.

---

## 9. بررسی عدم استفاده از `numRowsAffected` برای SELECT

**وضعیت: تأیید**

- جست‌وجوی متنی در `holdservice.cpp` و `holdservice.h` هیچ استفاده‌ای از `numRowsAffected` یا `rowsAffected` نشان نداد.
- همه SELECTها ابتدا `exec()` را بررسی می‌کنند و وجود ردیف با `next()` سنجیده می‌شود؛ نمونه‌ها:
  - `holdservice.cpp:212-220` و `261-269` (توابع بارگذاری).
  - `holdservice.cpp:452` و `868` (Advisory Lock).
  - `holdservice.cpp:468-473` و `912-917` (Idempotency).
  - `holdservice.cpp:530-533`, `548-551`, `566`, `992`, `1009`, `1025` (قفل‌ها).
  - `holdservice.cpp:495-504`, `754-761`, `940-953`, `1191-1198` (خواندن وضعیت).
- تنها `numRowsAffected` در سرویس‌های دیگر پروژه (Deposit/Transfer) استفاده شده و در HoldService وجود ندارد.

---

## 10. محدودیت‌های معماری و قراردادهای پروژه

**وضعیت: نامشخص (نیازمند تصمیم برای مرحله بعد)**

### منطبق با قراردادهای پروژه

- `HoldService` مستقیماً SQL اجرا می‌کند و Repository جدید نساخته است؛ هم‌راستا با `DepositService` و `TransferService`.
- هیچ تغییری در `ledger_balance` یا `available_balance` اعمال نمی‌شود (`holdservice.h:38-40` و کل بدنه سرویس).
- هیچ Controller/Route/`main.cpp` در این مرحله تغییر نکرده است.
- نام‌گذاری Error Codeها، ساختار `Status { Completed, Rejected, Failed }` و سبک Lambdas با سرویس‌های موجود هم‌خوان است.
- `CMakeLists.txt` فقط برای افزودن دو فایل سرویس تغییر کرده است (با تأیید کاربر).

### موارد باز برای مرحله Controller

1. **داده‌های پاسخ 201/200:** طراحی بخش‌های ۱۳.۱ و ۱۳.۴ پاسخ را شامل `originalAmount`، `remainingAmount`، `status`، `currentlyBlockedAmount` و ... می‌داند؛ اما `HoldResult`/`HoldEventResult` فقط شناسه، `replayed`، کد و پیام دارند. سرویس وضعیت را پیش از Commit می‌خواند ولی آن را بازنمی‌گرداند. Controller برای ساخت پاسخ کامل به متدهای خواندن یا Query اضافی نیاز دارد. این مورد باید در مرحله Controller تصمیم‌گیری شود.
2. **متدهای خواندن:** `getHoldById`، `getHoldsByAccount` و `getEvents` در سرویس وجود ندارند؛ طبق محدوده این مرحله طبیعی است ولی پیش از Route شدن باید اضافه شوند یا از لایه دیگری تأمین شوند.
3. **دامنه Idempotency بین عملیات:** طراحی بخش ۹ «کلید جهانی، هماهنگ با Deposit/Transfer» را مطرح می‌کند، اما Migration 017 فقط ایندکس یکتا روی `account_hold.idempotency_key` ساخته است. بنابراین یک کلید می‌تواند هم‌زمان در `transaction` (Deposit/Transfer) و `account_hold` استفاده شود. اگر یکتایی سراسری بین همه عملیات مالی موردنظر است، نیازمند تصمیم و احتمالاً Migration دیگری است. پیاده‌سازی فعلی مطابق Migration 017 است.
4. **کدهای جدید:** `HOLD_INVALID_REFERENCE` و فیلد `replayed` باید در نگاشت HTTP مرحله Controller لحاظ شوند.
5. **`available_balance` کهنه:** پس از Hold/Release به‌روزرسانی نمی‌شود؛ باید در قرارداد خواندن مانده یا مرحله بعدی محاسبه پویا مدنظر باشد.

---

## نتیجه نهایی

پیاده‌سازی `HoldService` از نظر منطقی و هم‌زمانی سالم است و برای ورود به مرحله Controller/Route آماده است، مشروط به موارد زیر:

- آزمون واقعی حداقل یک خطای Trigger برای تأیید نگاشت `QSqlError::text()`.
- تصمیم درباره قرارداد داده پاسخ (وضعیت Hold در خروجی 201/200).
- تصمیم درباره دامنه یکتایی Idempotency بین Hold و Transaction.
- تصمیم اختیاری درباره استفاده از زمان دیتابیس به‌جای ساعت برنامه برای تطابق کامل با طراحی/Trigger.

در این بازبینی هیچ کد، Build، Migration یا DDL/DML اجرا نشد و هیچ فایلی جز همین گزارش تغییر نکرد.
