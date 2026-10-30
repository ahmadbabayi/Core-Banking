# Core Banking — Project History

> این سند برای ثبت شواهد تاریخی آزمون‌ها، نتایج اجرای دستورات، Migrationها، Commitها و اشکالات و رفع آن‌ها است.
>
> قواعد کسب‌وکار و تصمیم‌های معماری پایدار در `docs/PROJECT_CONTEXT.md` نگهداری می‌شوند.
>
> نتایج تاریخی، وضعیت فعلی برنامه یا دیتابیس را تضمین نمی‌کنند. قبل از استفاده از شناسه‌های آزمایشی یا نتیجه‌های قدیمی، وضعیت واقعی بررسی شود.
>
> مخزن رسمی: https://github.com/ahmadbabayi/Core-Banking

## 1. شیوه ثبت شواهد

برای هر آزمون مهم، تا حدی که اطلاعات موجود باشد، موارد زیر ثبت شوند:

- تاریخ و زمان اجرای آزمون.
- Commit یا نسخه کد.
- محیط و نام دیتابیس.
- درخواست یا دستور دقیق.
- کد وضعیت HTTP یا خروجی واقعی.
- نتیجه مورد انتظار.
- وضعیت نهایی مانده‌ها و ثبت‌های حسابداری.
- محدودیت‌ها و آزمون‌هایی که هنوز انجام نشده‌اند.

اگر خروجی کامل درخواست یا پاسخ تاریخی حفظ نشده است، نباید متن آن بازسازی یا جعل شود. نتیجه خلاصه را با برچسب «نتیجه ثبت‌شده» و نه «خروجی کامل» ثبت کنید.

## 2. محیط و دستورات ثبت‌شده

محیط توسعه ثبت‌شده:
- Debian 12
- Qt 5.15.8
- GCC 12.2
- CMake 3.25
- PostgreSQL 15.x
- مسیر پروژه: `/home/ahmad/projects/CoreBanking`
- مسیر Build: `/home/ahmad/projects/build-CoreBanking-Desktop-Debug`
- دیتابیس اصلی: `corebanking`
- پورت API: `8080`

اتصال به دیتابیس اصلی:

```bash
psql -U corebanking -h localhost -d corebanking -W
```

دستور Build:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

آزمون Health API:

```bash
curl -i http://localhost:8080/api/v1/health
```

این دستورات نمونه‌های ثبت‌شده هستند؛ به‌خودی‌خود به معنای اجرای دوباره آن‌ها در زمان حاضر نیستند.

## 3. تاریخچه Migrationها

فهرست Migrationهای تاریخی شناخته‌شده:

| شماره | فایل | موضوع کلی |
|---|---|---|
| 001 | `001_geography_currency.sql` | جغرافیا و ارز |
| 002 | `002_remove_legacy_tables.sql` | حذف جداول قدیمی |
| 003 | `003_customer.sql` | ساختار مشتری |
| 004 | `004_customer_number_and_indexes.sql` | شماره مشتری و شاخص‌ها |
| 005 | `005_account_product_ledger.sql` | حساب، محصول و دفتر کل |
| 006 | `006_account_relationships.sql` | روابط حساب |
| 007 | `007_balances.sql` | مانده‌ها |
| 008 | `008_transaction.sql` | تراکنش |
| 009 | `009_journal.sql` | Journal |
| 010 | `010_financial_consistency.sql` | کنترل‌های یکپارچگی مالی |
| 011 | `011_city_code_global_unique.sql` | یکتایی سراسری کد شهر |
| 012 | `012_product_ledger_mapping.sql` | نگاشت محصول به دفتر کل |
| 013 | `013_account_type_serial.sql` | سریال مستقل نوع حساب |
| 014 | `014_product_balance_rules.sql` | قواعد مانده محصول و Override حساب |
| 015 | `015_account_holds.sql` | مسدودی وجه و رویدادهای آزادسازی |
| 016 | `016_allow_overdraft_balances.sql` | تغییرات مانده قابل برداشت، اضافه‌برداشت و Idempotency طبق سابقه پروژه |

نام و موضوع جدول بالا از سابقه پروژه جمع‌بندی شده است. برای جزئیات قطعی هر فایل، خود Migration مرجع است.

### 3.1 Migration 014 — قواعد مانده محصول

ساختارهای مربوط به قواعد مانده محصول و Override حساب ایجاد شدند.

قواعد مهم ثبت‌شده:
- حداقل مانده و مجازبودن اضافه‌برداشت مستقل‌اند.
- قواعد دارای بازه اعتبار هستند.
- برای بازه‌های زمانی، محدودیت‌های عدم هم‌پوشانی تعریف شده‌اند.
- Extension موردنیاز `btree_gist` در Schema ثبت شده است.

Migration 014 طبق سابقه قبلی با موفقیت اعمال شده بود. قبل از استفاده مجدد، وضعیت دیتابیس واقعی بررسی شود.

### 3.2 Migration 015 — مسدودی وجه

تاریخ ثبت و اجرا: 2026-10-24.

Commitهای مرتبط:
- `3258fdd` — `Add initial account holds migration`
- `ec4706e` — `Add hold event idempotency validation`

در تاریخ ثبت‌شده، هر دو Commit به `origin/main` ارسال شدند.

نسخه پشتیبان قبل از اجرا:

`database/backups/corebanking_before_015_20261024.dump`

طبق گزارش، Migration روی دیتابیس اصلی محلی `corebanking` اجرا شد و با `COMMIT` پایان یافت. وجود موارد زیر در دیتابیس اصلی بررسی و تأیید شده بود:
- `account_hold`
- `account_hold_events`
- `account_hold_status`
- Triggerهای اعتبارسنجی و محافظت از داده‌ها

### 3.3 آزمون‌های SQL برای Migration 015

این آزمون‌ها روی دیتابیس آزمایشی اجرا شدند و با `ROLLBACK` پایان یافتند؛ بنابراین نباید با آزمون یکپارچه HTTP یا اجرای داده‌های واقعی در دیتابیس اصلی اشتباه گرفته شوند.

| مورد آزمون | نتیجه ثبت‌شده |
|---|---|
| آزادسازی جزئی و محاسبه مبلغ باقی‌مانده | موفق |
| جلوگیری از درج دوباره رویداد در تکرار درخواست یکسان | موفق |
| آزادسازی کامل مبلغ باقی‌مانده | موفق |
| رد آزادسازی بیش از مبلغ باقی‌مانده | موفق |
| جلوگیری از `UPDATE` رویداد آزادسازی | موفق |
| جلوگیری از `DELETE` رویداد آزادسازی | موفق |
| صفرشدن مبلغ مسدود در زمان انقضا بدون نیاز به رویداد انقضا | موفق |
| الزام به `EXPIRY_RELEASE` برای آزادسازی مسدودی منقضی‌شده | موفق |
| رد استفاده مجدد از کلید Idempotency با محتوای متفاوت | موفق |
| حفظ رویداد اصلی در صورت استفاده نادرست از کلید | موفق |

این نتایج، آزمون‌های SQL ثبت‌شده هستند؛ آزمون بار هم‌زمان و آزمون کامل HTTP API برای مسدودی و آزادسازی با این شواهد اثبات نشده‌اند.

### 3.4 Migration 016 — تغییرات مالی و Idempotency

در الحاقیه پروژه با تاریخ 2026-10-26، موارد زیر گزارش شده بود:

- فایل `database/schema/016_allow_overdraft_balances.sql` در مخزن ثبت شده است.
- Migration در دیتابیس اصلی محلی `corebanking` با موفقیت اجرا شده است.
- فیلدهای زیر برای Idempotency تراکنش ایجاد و بررسی شده‌اند:
  - `idempotency_key`
  - `request_fingerprint`
  - `result_code`
  - `result_message`
- شاخص یکتای `uq_transaction_api_idempotency` ایجاد و بررسی شده است.
- قید `ck_account_balance_available` حذف شده است.
- قید نامنفی‌بودن `ledger_balance` حفظ شده است.
- وضعیت `REJECTED` برای تراکنش مجاز شده است.

این قسمت بر اساس گزارش تاریخی است؛ برای ادامه کار باید فایل Migration و Schema جاری دوباره بررسی شوند.

## 4. آزمون Health API

مسیر:

`GET /api/v1/health`

نتیجه تاریخی ثبت‌شده:

- وضعیت HTTP: `200 OK`
- بدنه:

```json
{"status":"UP"}
```

این نتیجه متعلق به اجرای قبلی است، نه اثبات در دسترس‌بودن سرور در زمان حاضر.

## 5. آزمون Product Management API

Commit مرتبط:

`7ca1fd9` — `Implement product management API`

مسیرهای ثبت‌شده:
- `POST /api/v1/products`
- `GET /api/v1/products`
- `GET /api/v1/products/{id}`
- `PATCH /api/v1/products/{id}/status`

در محیط آزمون، نتایج زیر ثبت شده‌اند:

| آزمون | نتیجه ثبت‌شده |
|---|---|
| دریافت فهرست اولیه | `count: 0` |
| فیلترهای `status=ACTIVE`، `accountTypeId=1` و `currencyId=1` در شروع آزمون | `count: 0` |
| ایجاد محصول بدون `currencyId` | `400`, کد `PRODUCT_REQUIRED_FIELDS` |
| ایجاد محصول معتبر | `201 Created` |
| شناسه محصول ایجادشده | `1` |
| استفاده از `currencyId=999` | `400`, کد `PRODUCT_INVALID_ACCOUNT_TYPE_OR_CURRENCY` |
| دریافت فهرست و جزئیات محصول | موفق |
| ایجاد دوباره کد محصول | `409`, کد `PRODUCT_DUPLICATE_CODE` |
| تغییر وضعیت محصول به `INACTIVE` | `200 OK` |

داده ایجادشده:
- کد: `TEST-CURRENT-IRR`
- نام: `Test Current Account IRR`
- `accountTypeId`: `1`
- `currencyId`: `1`
- `effectiveFrom`: `2026-10-12`
- شناسه: `1`

آخرین وضعیت ثبت‌شده در پایان آزمون، `INACTIVE` بود. وضعیت جاری محصول باید از API واقعی دوباره بررسی شود.

## 6. آزمون نگاشت Product–Ledger

در تاریخچه، نگاشت فعالی با شناسه `3` برای محصول `1` ثبت شده بود:

- `productLedgerMappingId`: `3`
- `productId`: `1`
- `ledgerAccountId`: `1`
- شروع اعتبار: `2026-10-14`
- پایان اعتبار: خالی
- وضعیت: فعال

این نگاشت در آزمون افتتاح حساب استفاده شد. این رکورد تاریخی است و پیش از استفاده مجدد باید از دیتابیس بررسی شود.

## 7. آزمون Ledger Account API

Commit مرتبط:

`7ca1fd9` — `Implement product management API`

مسیرهای API دفتر کل:
- `POST /api/v1/ledger-accounts`
- `GET /api/v1/ledger-accounts`
- `GET /api/v1/ledger-accounts/{id}`
- `PATCH /api/v1/ledger-accounts/{id}/status`

نتایج HTTP ثبت‌شده:

| آزمون | نتیجه |
|---|---|
| دریافت فهرست | `200 OK` |
| دریافت سرفصل با شناسه 1 | `200 OK` |
| فیلتر `currencyId=1` | `200 OK` |
| فیلتر `currencyId=abc` | `400 Bad Request` |
| ایجاد سرفصل با کد `20100102` | `201 Created` |
| ایجاد مجدد همان کد | `409 Conflict` |
| دریافت سرفصل با شناسه 2 | `200 OK` |
| تغییر وضعیت به `INACTIVE` | `200 OK` |
| فیلتر بر اساس وضعیت | موفق |
| بازگرداندن وضعیت به `ACTIVE` | `200 OK` |
| کد نامعتبر `123` | `400 Bad Request` |
| دسته‌بندی نامعتبر `SAVINGS` | `400 Bad Request` |
| والد ناموجود با شناسه `999999` | `404 Not Found` |

پس از آزمون‌های منفی، تعداد سرفصل‌ها همچنان دو مورد گزارش شد؛ بنابراین آزمون‌های نامعتبر رکورد جدید ایجاد نکرده بودند.

### داده‌های آزمون ثبت‌شده

1. شناسه `1`، کد `20100101`، نام `Test Current Account IRR`، ارز `1`، وضعیت `ACTIVE`، بدون والد.
2. شناسه `2`، کد `20100102`، نام `Test Savings Account IRR`، ارز `1`، والد `1`، وضعیت `ACTIVE`.

این نام‌ها نشان‌دهنده داده‌های آزمون‌اند و نباید معادل داده تولیدی فرض شوند.

## 8. آزمون Account Opening API

مسیر:

`POST /api/v1/accounts`

فیلدهای درخواست ثبت‌شده:
- `productId`
- `openingBranchId`
- `ownerCustomerIds`

مقادیر موجود در محیط آزمون:
- مشتری فعال: شناسه `49`
- محصول فعال: شناسه `1`
- شعبه فعال: شناسه `4`
- نگاشت محصول به سرفصل: شناسه `3`
- سرفصل دفتر کل: شناسه `1`

نتایج:

| آزمون | نتیجه |
|---|---|
| افتتاح حساب اول از طریق HTTP | `201 Created` |
| دریافت حساب اول | `200 OK` |
| افتتاح حساب دوم با همان مشتری، محصول و شعبه | `201 Created` |
| دریافت حساب دوم | `200 OK` |
| متفاوت‌بودن شماره حساب دو افتتاح | موفق |
| مانده اولیه حساب جدید | صفر |
| ایجاد Journal برای افتتاح حساب صفرمانده | انجام نشد |

شماره‌های مشاهده‌شده:
- `0100000000017`
- `0100000000022`

این دو شماره نمونه نتیجه اجرای الگوریتم هستند؛ نباید در کد به‌صورت ثابت استفاده شوند.

نتیجه تاریخی فوق نشان می‌دهد دو افتتاح حساب و دریافت جزئیات آن‌ها در محیط آزمون موفق بوده‌اند. نام دقیق دیتابیس مورد استفاده در این رکورد تاریخی مشخص نشده است؛ در نتیجه نباید بدون بررسی، آن را آزمون تأییدشده روی دیتابیس اصلی محسوب کرد.

## 9. آزمون Account API و چرخه عمر حساب

مسیرهای بررسی‌شده:
- `GET /api/v1/accounts/{id}`
- `PATCH /api/v1/accounts/{id}/status`

نتایج ثبت‌شده:

| آزمون | نتیجه |
|---|---|
| دریافت حساب موجود | `200 OK` |
| دریافت حساب `999999` | `404 Not Found` |
| وضعیت مقصد `UNKNOWN` | `400`, کد `INVALID_ACCOUNT_STATUS` |
| انتقال `ACTIVE` به `DORMANT` | `200 OK` |
| انتقال `DORMANT` به `ACTIVE` | `200 OK` |
| انتقال `ACTIVE` به `BLOCKED` | `200 OK` |
| انتقال `BLOCKED` به `ACTIVE` | `200 OK` |
| انتقال `ACTIVE` به `ACTIVE` | `422`, کد `ACCOUNT_STATUS_TRANSITION_REJECTED` |
| بستن حساب دوم | `200 OK` |
| فعال‌سازی حساب بسته‌شده | `422`, کد `ACCOUNT_STATUS_TRANSITION_REJECTED` |

در آزمون ثبت‌شده، پس از انتقال نامعتبر وضعیت قبلی حساب حفظ شد. مقدار `closedAt` حساب بسته‌شده نیز پس از رد فعال‌سازی مجدد حفظ شده بود.

وضعیت پایانی گزارش‌شده:
- حساب `1`: `ACTIVE`
- حساب `2`: `CLOSED`

این نتایج تمام ترکیب‌های وضعیت و تمام قواعد کسب‌وکار حساب را پوشش نمی‌دهند.

## 10. آزمون Deposit API در دیتابیس اصلی

تاریخ ثبت گزارش: 2026-10-26.

مسیر:

`POST /api/v1/deposits`

نتیجه ثبت‌شده:
- واریز یک واحد به حساب `1` در دیتابیس اصلی محلی `corebanking` موفق بود.
- تراکنش ایجادشده: شناسه `1`.
- Journal ایجادشده: شناسه `1`.
- وضعیت Journal: `POSTED`.
- سند متوازن گزارش شد.
- پس از واریز، `ledger_balance` حساب `1` برابر `1` و `available_balance` نیز برابر `1` بود.
- Retry با همان کلید Idempotency همان تراکنش را برگرداند و رکورد تکراری ایجاد نکرد.

سرفصل‌های استفاده‌شده در سابقه:
- سرفصل تسویه `10100101`: شناسه `4`، ارز `1`، دسته `ASSET`، فعال و قابل ثبت.
- سرفصل سپرده مشتری `20100101`: دسته `LIABILITY`.

این گزارش به معنای آزمون کامل همه حالت‌های Idempotency یا تأیید نهایی یکپارچگی در بار هم‌زمان نیست. آزمون کلید یکسان با payload متفاوت، هم‌زمانی واقعی و تمام مسیرهای Rollback جداگانه لازم‌اند.

## 11. آزمون Internal Transfer API

مسیر:

`POST /api/v1/transfers`

**Snapshot تاریخی در 2026-10-26** (و نه وضعیت نهایی کنونی):
- در آن تاریخ، API انتقال داخلی در مخزن پیاده‌سازی شده بود.
- در آن snapshot، انتقال موفق روی دیتابیس اصلی `corebanking` هنوز ثبت نشده بود.
- حساب `2` در آن گزارش `CLOSED` بود؛ وضعیت جاری هر حساب باید از API/دیتابیس همان محیط بررسی شود.
- مسیر نیازمند تسویه اضافه‌برداشت با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شد.
- آزمون موفق انتقال در دیتابیس آزمایشی در سابقه قدیمی‌تر گزارش شده بود.

**اصلاح وضعیت در 2026-10-28:** کاربر تأیید کرده API انتقال داخلی تکمیل و تست شده است. این تأیید، عبارت «API تست نشده» را به‌عنوان وضعیت فعلی رفع می‌کند. با این حال، چون جزئیات تست جدید (محیط دیتابیس، payload، پاسخ و نتایج مانده/Journal) هنوز در این سند درج نشده، از آن نمی‌توان نتیجه گرفت که حتماً تست موفق در دیتابیس اصلی یا همه حالت‌های شکست اجرا شده است.

مواردی که همچنان در آزمون جامع نیازمند پوشش/ثبت شواهدند:
- انتقال با موجودی ناکافی و حفظ بی‌تغییری مانده در صورت رد.
- Rollback در تمام نقاط شکست.
- درخواست‌های هم‌زمان برای یک حساب مبدأ یا مقصد.
- Retry با کلید یکسان و محتوای متفاوت.
- تطبیق مانده حساب‌ها، مانده دفتر کل و Journal در همه سناریوها.
- مسیر نیازمند تسویه اضافه‌برداشت؛ تا تکمیل چرخه اضافه‌برداشت، رد امن این مسیر باید حفظ شود.

## 12. تاریخچه Commitهای مرتبط

Commitهای تاریخی ثبت‌شده:

| Commit | پیام |
|---|---|
| `ec035ff` | `Implement account transfer and financial posting` |
| `47815bd` | `Remove startup tests from main` |
| `b5c836a` | `Add product ledger mapping schema` |
| `b619965` | `Ignore database backup dumps` |
| `7ca1fd9` | `Implement product management API` |
| `0c5ac38` | `Add account opening and lifecycle API` |
| `eaf34ce` | `Add product and account balance rules schema` |
| `3258fdd` | `Add initial account holds migration` |
| `ec4706e` | `Add hold event idempotency validation` |
| `c37a1b7` | `Fix ledger balance posting for internal transfers` |
| `1fe1c1d` | `Implement deposit API and redesign internal transfers` |

این جدول بخشی از تاریخچه Commitهاست، نه تضمین ترتیب فعلی همه Commitها. در تاریخ 2026-10-26، دو Commit آخر بالا گزارش شده‌اند؛ SHA کامل HEAD و هماهنگی فعلی `origin/main` باید هنگام ادامه پروژه مجدداً بررسی شوند.

### جزئیات Commit افتتاح حساب

Commit `0c5ac38` شامل این تغییرات گزارش شده بود:
- سرویس افتتاح حساب.
- Controller افتتاح حساب.
- Controller دریافت حساب و تغییر وضعیت.
- اتصال مسیرها به HTTP Router.
- تغییرات لازم در `main.cpp` و `CMakeLists.txt`.
- Schema شماره 013 برای سریال مستقل نوع حساب.

### جزئیات تغییرات مسدودی

- `3258fdd`: افزودن Migration اولیه مسدودی.
- `ec4706e`: افزودن اعتبارسنجی Idempotency برای رویدادهای مسدودی.

### آخرین وضعیت Git ثبت‌شده

در تاریخ 2026-10-24، SHA محلی و ریموت زیر گزارش شده بود:

`ec4706e1a2d6dcb1b9ee0647d78b727ceed882c4`

این SHA پس از ثبت Commitهای 2026-10-26 دیگر نباید به‌عنوان آخرین HEAD فرض شود.

## 13. اشکالات و محدودیت‌های شناخته‌شده

### سازگاری سرویس‌های مالی با Schema جدید

سرویس‌های قدیمی مانده و ثبت حسابداری مرجع معماری جدید محسوب نمی‌شوند. مسیر انتقال واریز باید با قواعد و Schema جاری بررسی شود؛ این مورد به‌معنای منکرشدن تست APIهایی که کاربر تأیید کرده نیست، بلکه یکپارچگی مالی جامع و پوشش تمام سناریوها را هدف می‌گیرد.

### محاسبه مانده قابل برداشت

`available_balance` در Schema ذخیره می‌شود. وجود ستون به معنی تضمین محاسبه سازگار با مسدودی و اعتبار باقی‌مانده اضافه‌برداشت نیست.

### اضافه‌برداشت

تسویه اضافه‌برداشت و ثبت کامل بدهی مشتری و مطالبات بانک هنوز کامل و آزمون‌شده محسوب نمی‌شوند. مسیر انتقال نیازمند اضافه‌برداشت طبق سابقه با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود.

### مسدودی

Migration و Triggerهای مسدودی در دیتابیس ثبت شده‌اند، اما سرویس C++ و API کامل مسدودی و آزادسازی در وضعیت ثبت‌شده تأیید نشده‌اند.

### هم‌زمانی

آزمون SQL با `ROLLBACK` جایگزین آزمون بار هم‌زمان واقعی نیست. سناریوهای رقابتی در واریز، برداشت، انتقال، برگشت و آزادسازی مسدودی باید جداگانه آزمون شوند.

## 14. گام‌های بعدی ثبت‌شده در تاریخچه قبلی

این فهرست در تاریخچه قبلی نوشته شده بود و در بازبینی 2026-10-28 اصلاح شده است:
1. بررسی سازگاری `account_balance` و لایه‌های Repository/Service با Schema فعلی.
2. طراحی و تکمیل API ایجاد Hold، آزادسازی جزئی/کامل، انقضا و لغو؛ مسیرهای HTTP آن در Router بررسی‌شده وجود ندارند.
3. تکمیل Idempotency رویدادهای Hold: بازیابی نتیجه در Retry با همان `(hold_id, idempotency_key)` و یکسان‌بودن fingerprint/effective time.
4. تکمیل محاسبه اعتبار باقی‌مانده اضافه‌برداشت و ثبت بدهی مشتری/مطالبات بانک.
5. پوشش آزمون جامع HTTP و تطبیق مانده، Transaction، Journal، `ledger_account_balance` و رویدادهای Hold.
6. ثبت شواهد آزمون‌های مستقیم Province، Account Type و Product–Ledger Mapping در صورت موجودبودن/نیاز.
7. افزودن یا تعیین تکلیف endpointهای Withdrawal، Reversal، Balance و جست‌وجوی Transaction/Journal.
8. آزمون Idempotency با payload متفاوت، Rollback در نقاط شکست، موجودی ناکافی و بار هم‌زمان واقعی.
9. ثبت گزارش شفاف Build، تست HTTP و وضعیت Git. Commit/Push فقط پس از ارائه نتیجه و تأیید کاربر.

## 15. قاعده پایانی تاریخچه

History شاهدی از اتفاقات گذشته است. هر نتیجه باید همراه با محدودیت‌ها و محیط اجرای آن خوانده شود.

اگر در ادامه پروژه قاعده کسب‌وکار جدیدی تصویب شد، باید به `PROJECT_CONTEXT.md` منتقل شود. اگر آزمون جدیدی اجرا شد، نتیجه دقیق آن باید در همین فایل ثبت شود. سابقه قدیمی بدون آزمون جدید، مدرک کافی برای تأیید وضعیت جاری نیست.

## 16. بازبینی Router و اصلاح فهرست APIها — 2026-10-28

### منبع بررسی

- Commit گزارش‌شده از سوی کاربر: `038638e76a89012e1d94275b192514b4afb4696d`.
- فایل Router بررسی‌شده: `src/web/httprouter.cpp`.
- فایل سیم‌کشی Controllerها در برنامه: `src/main.cpp`.
- فهرست فایل‌های Build: `CMakeLists.txt`.
- اسناد بررسی‌شده: `docs/PROJECT_CONTEXT.md` و `docs/PROJECT_HISTORY.md`.

این بازبینی از کد Router برای استخراج method/path استفاده می‌کند، و از تأیید مستقیم کاربر برای وضعیت «تکمیل و تست‌شده» در دوازده گروه استفاده می‌کند. وجود یک route بدون گزارش تست، به‌تنهایی به‌عنوان تست‌شده ثبت نشده است. این بازبینی فقط مستنداتی/کدخوانی بود و در این نوبت Build یا آزمون HTTP جدیدی اجرا نشد؛ وضعیت آزمون APIهای دوازده‌گانه بر اساس تأیید کاربر است.

فایل‌های اصلاح‌شده‌ای که همراه این گزارش تحویل می‌شوند هنوز جایگزین فایل‌های درخت کاری کاربر نشده‌اند و Commit/Push جدیدی برای آن‌ها انجام نشده است.

### 16.1 فهرست کامل مسیرهای موجود در Router بررسی‌شده

| گروه | روش‌ها و مسیرها |
|---|---|
| Health | `GET /api/v1/health` |
| Transfers | `POST /api/v1/transfers` |
| Deposits | `POST /api/v1/deposits` |
| Accounts | `POST /api/v1/accounts`; `GET /api/v1/accounts/{id}`; `PATCH /api/v1/accounts/{id}/status` |
| Customers | `POST /api/v1/customers`; `GET /api/v1/customers?nationalId={value}`; `GET /api/v1/customers/{id}` |
| Currencies | `POST /api/v1/currencies`; `GET /api/v1/currencies`; `GET /api/v1/currencies/{id}` |
| Account Types | `POST /api/v1/account-types`; `GET /api/v1/account-types` (فیلتر `status`, `currencyId`); `GET /api/v1/account-types/{id}`; `PATCH /api/v1/account-types/{id}/status` |
| Products | `POST /api/v1/products`; `GET /api/v1/products` (فیلتر `status`, `accountTypeId`, `currencyId`); `GET /api/v1/products/{id}`; `PATCH /api/v1/products/{id}/status` |
| Product–Ledger Mappings | `POST /api/v1/product-ledger-mappings`; `GET /api/v1/product-ledger-mappings` (فیلتر `status`, `productId`, `ledgerAccountId`); `GET /api/v1/product-ledger-mappings/{id}`; `PATCH /api/v1/product-ledger-mappings/{id}/status`; `PATCH /api/v1/product-ledger-mappings/{id}/effective-to` |
| Ledger Accounts | `POST /api/v1/ledger-accounts`; `GET /api/v1/ledger-accounts` (فیلتر `status`, `category`, `currencyId`); `GET /api/v1/ledger-accounts/{id}`; `PATCH /api/v1/ledger-accounts/{id}/status` |
| Countries | `POST /api/v1/countries`; `GET /api/v1/countries`; `GET /api/v1/countries/{id}` |
| Provinces | `POST /api/v1/provinces`; `GET /api/v1/provinces`; `GET /api/v1/provinces/{id}` |
| Cities | `POST /api/v1/cities`; `GET /api/v1/cities`; `GET /api/v1/cities/{id}` |
| Branches | `POST /api/v1/branches`; `GET /api/v1/branches`; `GET /api/v1/branches/{id}`; `PUT /api/v1/branches/{id}`; `PATCH /api/v1/branches/{id}/status` |

Router در صورت ناشناخته‌بودن مسیر، پاسخ `404 Not Found` با کد `ROUTE_NOT_FOUND` برمی‌گرداند. برای Deposit و Transfer، اگر Controller مربوطه تنظیم نشده باشد، Router پاسخ `503 Service Unavailable` برمی‌گرداند.

### 16.2 APIهای تکمیل و تست‌شده — تأیید مستقیم کاربر در 2026-10-28

کاربر صریحاً تأیید کرد همه گروه‌های زیر پیاده‌سازی و تست شده‌اند. این جدول مستقل است تا هنگام ادامهٔ پروژه این قابلیت‌ها به‌اشتباه «باقی‌مانده» معرفی نشوند.

| گروه API | مسیرهای Router | وضعیت ثبت‌شده |
|---|---|---|
| Health | `GET /api/v1/health` | تکمیل و تست‌شده |
| Countries | `POST /api/v1/countries`؛ `GET /api/v1/countries`؛ `GET /api/v1/countries/{id}` | تکمیل و تست‌شده |
| Currencies | `POST /api/v1/currencies`؛ `GET /api/v1/currencies`؛ `GET /api/v1/currencies/{id}` | تکمیل و تست‌شده |
| Cities | `POST /api/v1/cities`؛ `GET /api/v1/cities`؛ `GET /api/v1/cities/{id}` | تکمیل و تست‌شده |
| Branches | `POST /api/v1/branches`؛ `GET /api/v1/branches`؛ `GET /api/v1/branches/{id}`؛ `PUT /api/v1/branches/{id}`؛ `PATCH /api/v1/branches/{id}/status` | تکمیل و تست‌شده |
| Customers | `POST /api/v1/customers`؛ `GET /api/v1/customers?nationalId={value}`؛ `GET /api/v1/customers/{id}` | تکمیل و تست‌شده |
| Products | `POST /api/v1/products`؛ `GET /api/v1/products`؛ `GET /api/v1/products/{id}`؛ `PATCH /api/v1/products/{id}/status` | تکمیل و تست‌شده |
| Ledger Accounts | `POST /api/v1/ledger-accounts`؛ `GET /api/v1/ledger-accounts`؛ `GET /api/v1/ledger-accounts/{id}`؛ `PATCH /api/v1/ledger-accounts/{id}/status` | تکمیل و تست‌شده |
| Account Opening | `POST /api/v1/accounts` | تکمیل و تست‌شده |
| Account Details / Status | `GET /api/v1/accounts/{id}`؛ `PATCH /api/v1/accounts/{id}/status` | تکمیل و تست‌شده |
| Deposits | `POST /api/v1/deposits` | تکمیل و تست‌شده |
| Internal Transfers | `POST /api/v1/transfers` | تکمیل و تست‌شده |

این وضعیت بر اساس تأیید مستقیم کاربر است. چون جزئیات کامل تست همه گروه‌ها (تاریخ، محیط، payload، پاسخ و تطبیق مانده/Journal) در History موجود نیست، هیچ خروجی HTTP جدیدی بازسازی یا حدس زده نشده و نباید همه این تست‌ها را خودبه‌خود «تست روی دیتابیس اصلی» یا «پوشش کامل همه سناریوها» نامید.

### 16.3 مسیرهای پیاده‌سازی‌شده که شواهد تست آن‌ها در History کافی نیست

برای Province، Account Type و تمام عملیات Product–Ledger Mapping، مسیر در Router وجود دارد ولی شواهد تست HTTP مستقیم هر عملیات در سابقه فعلی کامل نیست. نگاشت با شناسه `3` در آزمون افتتاح حساب استفاده شده؛ این تنها، به معنی تست همه endpointهای نگاشت نیست. این سه گروه را API مفقود تلقی نکنید؛ شواهد آزمون را بیابید یا فقط در صورت اجرا نشدن آزمون لازم، تست اضافه کنید.

### 16.4 مواردی که Router مسیرشان را ندارد

در Router بررسی‌شده endpointی برای Hold، Hold release/cancel/expiry، Withdrawal، Reversal، دریافت Balance، فهرست/جزئیات Transaction یا Journal، مدیریت `product_balance_rule` و `account_balance_rule_override`، مدیریت روابط/اختیارات حساب و مدیریت مستقل محدودیت‌های عملیاتی دیده نشد. وجود جدول یا Migration مرتبط به معنی وجود API نیست. این موارد در بخش «کارهای باقی‌مانده» ثبت می‌شوند؛ هیچ مسیر پیشنهادی به‌عنوان قرارداد فعلی فرض نشده است.

## 17. فهرست کارهای باقی‌مانده پس از بازبینی 2026-10-28

1. **Hold API:** تکمیل Controller/Service/Repository و ثبت مسیرهای HTTP برای ایجاد، مشاهده و آزادسازی جزئی/کامل، لغو و انقضا. منطق دیتابیس Migration 015 وجود دارد، اما endpointهای آن در Router نیستند.
2. **Withdrawal:** تعیین قرارداد و پیاده‌سازی برداشت با بررسی وجه قابل برداشت، Holdها، حداقل مانده، اضافه‌برداشت مجاز و Journal متوازن.
3. **Reversal:** ایجاد API برگشت که تراکنش و Journal معکوس مستقل بسازد، برگشت دوگانه را با کنترل هم‌زمانی رد کند و اثر کارمزد/Hold/Overdraft را لحاظ کند.
4. **Overdraft settlement:** جایگزینی مسیر رد `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` فقط پس از پیاده‌سازی و آزمون ثبت بدهی مشتری و مطالبات بانک؛ مانده سپرده مشتری نباید برای نمایش بدهی منفی شود.
5. **Balance and history endpoints:** تصمیم درباره نیاز و قرارداد APIهای دریافت مانده، تراکنش‌ها و Journal؛ در صورت تأیید نیاز، پیاده‌سازی با مجوزهای دسترسی لازم.
6. **Available balance invariant:** بررسی یکپارچگی فرمول `ledger_balance - active_holds + remaining_overdraft_limit` در Deposit، Transfer و مسیرهای مالی آینده.
7. **بازنویسی/هم‌ترازی سرویس‌های مالی قدیمی:** سرویس‌ها و Repositoryهای قدیمی مانده حساب، تراکنش و ثبت دفتر کل باید با Schema بازطراحی‌شده تطبیق داده یا بازنویسی شوند. تا وقتی سازگاری آن‌ها با `CURRENT_SCHEMA.sql` و قواعد جدید (Hold، Idempotency، Journal و اضافه‌برداشت) اثبات نشده، آن‌ها را مرجع معماری فعلی فرض نکنید.
8. **Financial reconciliation:** کنترل اینکه هر Journal ثبت‌شده متوازن باشد و مانده حساب‌ها با Transactionها و مجموع Debit/Credit دفتر کل سازگار بماند.
9. **Concurrency / Idempotency / Rollback:** تست درخواست‌های رقابتی؛ کلید یکسان با payload متفاوت؛ Retry؛ موجودی ناکافی؛ شکست بین درج Transaction، Journal و به‌روزرسانی مانده؛ اطمینان از عدم ثبت تکراری.
10. **ثبت شواهد آزمون:** ثبت مستقیم تست‌های Province، Account Type و Product–Ledger Mapping؛ تکمیل جزئیات تست‌های APIهای تأییدشده توسط کاربر در صورت امکان، بدون جعل خروجی تاریخی.
11. **بازبینی نهایی Router و راه‌اندازی:** تأیید اینکه `main.cpp` Controllerها را به Router متصل می‌کند، Build جاری موفق است و پاسخ 503 ناشی از Controller تنظیم‌نشده رخ نمی‌دهد.
12. **به‌روزرسانی اسناد بعد از هر کار:** Context برای قراردادها، تصمیم‌ها و وضعیت فعلی؛ History برای SHA، محیط، دستور، پاسخ، کنترل مانده/Journal و محدودیت‌های آزمون.

این فهرست به معنای باقی‌ماندن دوازده گروه API تأییدشده از سوی کاربر نیست؛ موارد فوق عمدتاً endpointهایی هستند که Router ندارد، عمق کامل آزمون مالی را می‌خواهند، یا شواهد مستقیمشان در History ناقص است.

## 18. قاعده پایانی تاریخچه

History شاهد اتفاقات گذشته است. برای هر نتیجه، محیط و محدوده آزمون مشخص شود. اگر کاربر وضعیت جدیدی از تست تأیید کرد، وضعیت خلاصه باید به‌روز شود، ولی نباید جزئیات نامشخص مانند نام دیتابیس، پاسخ HTTP یا خروجی SQL حدس زده شود.

