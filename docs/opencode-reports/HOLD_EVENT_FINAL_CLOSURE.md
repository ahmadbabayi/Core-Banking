# HOLD EVENT VERSIONED IDEMPOTENCY — جمع‌بندی نهایی و بستن موارد باز

تاریخ: 2026-11-11

این گزارش فقط موارد باز مرحله Hold را با کد فعلی تعیین تکلیف می‌کند. بازبینی کلی تکرار نشده است.

محدودیت‌های رعایت‌شده:

- هیچ اتصالی به دیتابیس انجام نشد.
- هیچ DDL/DML یا Migration اجرا نشد.
- `database/schema/018_hold_event_fingerprint.sql` و `database/CURRENT_SCHEMA.sql` تغییر نکردند.
- هیچ عملیات تغییردهنده Git انجام نشد.
- Deposit، Transfer و سایر سرویس‌ها تغییر نکردند.

---

## 1. موارد باز و نتیجه

### 1.1 Legacy — تفاوت مقایسه `amount` در آزادسازی کامل

**تفاوت واقعی:**

- Legacy Trigger در `database/schema/018_hold_event_fingerprint.sql:517-518` برای همه انواع رویداد `v_existing.amount = NEW.amount` را مقایسه می‌کند.
- Lookup سرویس در `src/application/holdservice.cpp:583-585` فقط برای `PARTIAL_RELEASE` مبلغ را مقایسه می‌کند و برای `FULL_RELEASE`، `EXPIRY_RELEASE` و `CANCELLATION_RELEASE` شرط `ELSE TRUE` دارد.
- این تفاوت تاریخی در Migration 015 هم وجود داشت؛ Trigger قبلی نیز `v_existing.amount = NEW.amount` را برای همه انواع مقایسه می‌کرد.

**اثر در مسیر واقعی API:**

- `createEvent` ابتدا Lookup ایدمپوتنسی را پیش از اعتبارسنجی وضعیت/مقدار انجام می‌دهد (`src/application/holdservice.cpp:1302-1376`). بنابراین Retry عادی یک آزادسازی کامل، بدون INSERT، همان رویداد قبلی را Replay می‌کند و Trigger اصلاً اجرا نمی‌شود.
- برای رویدادهای غیر‌جزئی، سرویس مبلغ را از `remainingAmount` فعلی می‌سازد (`src/application/holdservice.cpp:1543-1545`). در Retry بعد از آزادسازی کامل، این مقدار صفر است، در حالی که مبلغ ذخیره‌شده ردیف Legacy مقدار باقی‌مانده قبلی است. اگر Trigger زودتر از Lookup اجرا شود، این دو مقدار برابر نیستند.
- این وضعیت فقط در یک رقابت باریک ممکن است رخ دهد: ردیف موجود در Lookup اولیه دیده نشود و فقط هنگام INSERT/Trigger دیده شود. آن مسیر در Unique Conflict با Savepoint نیز Lookup مجدد سرویس را اجرا می‌کند (`src/application/holdservice.cpp:1592-1637`) و همان منطق «نادیده‌گرفتن مبلغ غیرجزئی» را دارد.
- «Replay اشتباه» رخ نمی‌دهد؛ مبلغ برای آزادسازی کامل بخشی از قرارداد درخواست نیست و سرویس آن را محاسبه می‌کند. «Conflict اشتباه» فقط در همان رقابت باریک Legacy ممکن است به‌صورت `409` دیده شود و رفتار جدیدی نیست.

**تصمیم:** قرارداد Legacy و Trigger 018 بدون تأیید کاربر تغییر نکرد. این مورد به‌عنوان تفاوت تاریخی حفظ‌شده و ریسک رقابتی باریک ثبت می‌شود، نه نقص قطعی مسیر عادی API.

### 1.2 Legacy — ریسک Round/Truncate در Retry Hold قدیمی

- Parser جدید کسر ثانیه را قبل از Parse به سه رقم Truncate می‌کند (`src/application/holdeventfingerprint.h:103-109`) و عمداً Round نمی‌کند.
- Parser قبلی `QDateTime::fromString(..., Qt::ISODateWithMs)` بود. بررسی عملی با Qt 5.15.8 نشان داد برای ورودی `2026-11-03T16:23:06.1239Z` مقدار `...06.124Z` تولید می‌شد؛ یعنی Round به سمت بالا.
- Fingerprint مربوط به `createHold` از Timestamp نرمال‌شده ساخته می‌شود و هیچ Legacy Fallback یا Version برای آن وجود ندارد (`src/application/holdservice.cpp:781-785`).
- برای Timestamp دارای Timezone و بیش از سه رقم اعشار، مسیر سازگاری صرفاً در کد اضافه شد: Fingerprint فعلی با Truncate و یک Fingerprint کاندید Legacy با Round قبلی Qt محاسبه می‌شود و Replay فقط با برابری Fingerprint ذخیره‌شده با یکی از آن دو پذیرفته می‌شود. این تغییر Schema، Migration یا قرارداد HTTP جدید نیاز ندارد.
- Timestamp بدون Timezone در Hold همچنان با قرارداد جدید `400` رد می‌شود. پشتیبانی از Retry قدیمی برای ورودی بدون Timezone نیازمند تصمیم درباره تقدم اعتبارسنجی و قرارداد API است و در این مرحله اعمال نشد.
- داده موجود دیتابیس نمی‌تواند ثابت کند ورودی اولیه بیش از سه رقم اعشار داشته یا نه؛ چون مقدار پیش از ذخیره به میلی‌ثانیه نرمال می‌شد. آزمون Regression این مسیر باید با Fixture آگاهانه پیش از 018 انجام شود.

### 1.3 خطاها — افشای متن خام دیتابیس

- در مسیر خطاهای Trigger، برای کدهای شناخته‌شده غیر‌Rejected قبلاً `insertError.text()` به‌عنوان پیام عمومی برگردانده می‌شد (`src/application/holdservice.cpp:1658-1660`).
- اکنون برای هر کد `ACCOUNT_HOLD_EVENT_FINGERPRINT_*`:
  - پیام عمومی ثابت از `triggerFailureMessage` استفاده می‌شود (`src/application/holdservice.cpp:531-547`).
  - متن خام فقط با `qWarning()` که سازوکار لاگ موجود پروژه است ثبت می‌شود (`src/application/holdservice.cpp:1647-1657`).
  - زیرساخت لاگ جدیدی ساخته نشد.
- مسیرهای Fail-Closed در Lookup از قبل پیام ثابت داشتند (`src/application/holdservice.cpp:691-704`).

### 1.4 خطاها — نگاشت `ACCOUNT_HOLD_EVENT_INVALID_*`

منشأ واقعی این چهار کد فقط شاخه دفاعی Trigger است:

| کد | منشأ واقعی | آیا از API عادی قابل‌دسترس است؟ | نگاشت فعلی |
|---|---|---|---|
| `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE` | `018_hold_event_fingerprint.sql:377-386` | خیر؛ سرویس در `holdservice.cpp:1189-1195` همان مجموعه را قبل از دیتابیس رد می‌کند | `422` |
| `ACCOUNT_HOLD_EVENT_INVALID_REASON_CODE` | `018_hold_event_fingerprint.sql:388-391` | خیر؛ سرویس در `holdservice.cpp:1201-1203` مقدار غیرخالی را الزام می‌کند | `422` |
| `ACCOUNT_HOLD_EVENT_INVALID_AMOUNT` | `018_hold_event_fingerprint.sql:421-447` | خیر؛ سرویس در `holdservice.cpp:1212-1214` و محاسبه `remainingAmount` همان قیود را اعمال می‌کند | `422` |
| `ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT` | `018_hold_event_fingerprint.sql:399-403` | خیر؛ سرویس Timestamp معتبر یا مقدار پیش‌فرض غیرNULL می‌فرستد | `422` |

نتیجه: این کدها در مسیر عادی API نشانه ورودی کاربر نیستند و بیشتر به ناسازگاری داخلی Service/Trigger یا INSERT مستقیم اشاره دارند. با این حال تغییر نگاشت `422` به `500` یک تغییر قرارداد HTTP قابل مشاهده است. طبق محدودیت این مرحله اعمال نشد و به‌عنوان تصمیم کاربر ثبت می‌شود.

### 1.5 آزمون‌ها — فراخوانی منطق واقعی

- آزمون‌های موجود مستقیماً `HoldEventFingerprintV1`، `HoldService` و `HoldController` را فراخوانی می‌کنند؛ نسخه کپی‌شده از Canonicalizer یا Lookup در آزمون وجود ندارد.
- برای آزمون همه ترکیب‌های Metadata، منطق تشخیص وضعیت Metadata از Service به تابع Production در `src/application/holdeventfingerprint.h:272-291` منتقل شد و Service همان تابع را در `src/application/holdservice.cpp:644-652` استفاده می‌کند. آزمون نیز همان تابع واقعی را فراخوانی می‌کند.
- آزمون‌های Unit لازم اضافه شدند:
  - همه ۸ ترکیب NULL/غیرNULL برای سه ستون Metadata در `tests/hold_event_fingerprint_and_validation_test.cpp:167-194`.
  - Versionهای `0`، `-1` و `2` و Fingerprint بدفرمت (کوتاه و uppercase) در همان فایل، بخش `testClassification`.
  - یکسان‌بودن Fingerprint برای `0.10` و `0.1` و نادیده‌گرفتن مبلغ محاسبه‌شده در `FULL_RELEASE` در `testFingerprintAmountIdentity` (`tests/hold_event_fingerprint_and_validation_test.cpp:196-241`).
  - Timestamp با Offset مثبت، Offset فقط‌ساعت، Offset منفی و Truncate بدون Round در `testTimestamp` (`tests/hold_event_fingerprint_and_validation_test.cpp:93-126`).

---

## 2. اصلاحات واقعی و فایل‌های تغییرکرده

اصلاحات واقعی این مرحله:

1. جلوگیری از افشای متن خام دیتابیس در پاسخ عمومی خطاهای جدید Fingerprint و ثبت جزئیات با `qWarning` موجود.
2. استخراج منطق تشخیص Metadata به تابع Production قابل‌آزمون و استفاده همان تابع در Lookup.
3. افزودن آزمون‌های Unit ضروریِ ذکرشده، بدون بازنویسی آزمون‌های موجود.

فایل‌های تغییرکرده:

- `src/application/holdeventfingerprint.h`
- `src/application/holdservice.cpp`
- `tests/hold_event_fingerprint_and_validation_test.cpp`
- `docs/opencode-reports/HOLD_EVENT_FINAL_CLOSURE.md` (این فایل)
- `docs/PROJECT_CONTEXT.md` (به‌روزرسانی کوتاه)
- `docs/PROJECT_HISTORY.md` (به‌روزرسانی کوتاه)

فایل‌های تغییرنکرده و عمداً دست‌نخورده:

- `database/schema/018_hold_event_fingerprint.sql`
- `database/CURRENT_SCHEMA.sql`
- `src/web/holdcontroller.cpp`
- `CMakeLists.txt`
- `DepositService`، `TransferService` و `main.cpp`

---

## 3. Build و Test اجراشده

### 3.1 Build پروژه

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق.

### 3.2 آزمون Unit بدون دیتابیس

```bash
g++ -std=c++17 -fPIC -I. \
  tests/hold_event_fingerprint_and_validation_test.cpp \
  src/application/holdservice.cpp \
  src/web/holdcontroller.cpp \
  src/web/apiresponse.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/hold_event_fingerprint_and_validation_test

/tmp/hold_event_fingerprint_and_validation_test
```

خروجی واقعی:

```text
All hold event fingerprint and validation tests passed
```

نتیجه: موفق. این آزمون به دیتابیس متصل نشد.

---

## 4. تصمیم‌های ضروریِ باقی‌مانده برای کاربر

1. **Timestamp بدون Timezone در Hold:** الزام جدید Timezone برای `effectiveAt`/`expiresAt` یک تغییر قرارداد API است؛ آیا Retry Holdهای قدیمی بدون Timezone باید پشتیبانی شود یا رد آن‌ها با `400` پذیرفته است؟
2. **آزمون Regression Holdهای Legacy با Timestamp چهاررقمی:** مسیر کد آماده است، اما Fixture آن باید پیش از 018 و با باینری قدیمی ساخته شود؛ تأیید این اقدام لازم است.
3. **نگاشت `ACCOUNT_HOLD_EVENT_INVALID_*`:** آیا `422` حفظ شود (قرارداد فعلی) یا در مرحله‌ای جداگانه به خطای داخلی `500` تغییر کند؟ این تغییر در این مرحله اعمال نشد.
4. **تفاوت مقایسه `amount` در Legacy:** آیا Trigger Legacy باید بدون تغییر بماند (پیشنهاد این گزارش) یا قرارداد Legacy به‌طور صریح بازنگری شود؟
5. **Restore/Rollback:** هیچ رویه‌ای در این مرحله آزمون یا تأیید نشد؛ انتخاب و آزمون رویه باید توسط کاربر انجام شود.

---

## 5. برنامه آزمون نهایی روی `corebanking` (آماده، اجرا نشده)

برنامه کامل و SQLهای آماده‌شده در `docs/opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md` قرار دارد. خلاصه:

- دیتابیس آزمون همان `corebanking` توسعه است؛ دیتابیس جدا ساخته نمی‌شود.
- پیش از 018: Backup، بررسی فقط‌خواندنی Schema/داده، انتخاب یا ساخت Fixtureهای Legacy، توقف سرویس قدیمی.
- پس از 018: اجرای فوری باینری جدید working tree فعلی، سپس آزمون Fingerprint SQL/C++، Create/Replay/Conflict، Legacy Retry (شامل Hold چهاررقمی)، Trigger RETURN NULL، رقابت و کنترل تعداد/مجموع رویدادها.
- Fixtureهای Legacy باید پیش از 018 ساخته شوند؛ غیرفعال‌کردن Trigger یا Constraint مجاز نیست.
- اگر باینری قدیمی برای ساخت Fixture در دسترس نبود، ساخت Fixture با SQL و حدس Fingerprint انجام نمی‌شود.

### 5.1 Restore / Rollback

- برگرداندن صرف باینری قدیمی پس از 018 کافی نیست: Trigger جدید برای هر INSERT Metadata کامل می‌خواهد و Service قدیمی این ستون‌ها را نمی‌فرستد؛ در نتیجه نوشتن رویداد متوقف می‌شود.
- برای ردیف‌های Legacy تولید حدسی Metadata پیشنهاد نمی‌شود.
- Reverse Migration رسمی برای 018 وجود ندارد؛ Rollback نیازمند Restore دامپ/Schema پیش از 018 یا اسکریپت معکوس تأییدشده توسط کاربر است.
- هیچ رویه Restore/Rollback در این مرحله تأییدشده اعلام نمی‌شود.

---

## 6. نتیجه

هیچ نقص قطعی جدیدی که مانع اجرای 018 باشد پیدا نشد. سازگاری Holdهای Legacy با Timestamp چهاررقمی در کد اضافه شد و آزمون Unit دارد. تصمیم مربوط به Timestamp بدون Timezone و نگاشت HTTP باید توسط کاربر تعیین شود.

**آماده درخواست مجوز اجرای 018 روی دیتابیس توسعه `corebanking`.**
