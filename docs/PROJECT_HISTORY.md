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

## 18. پیاده‌سازی HoldService — 2026-11-01

### 18.1 محدوده و فایل‌ها

- ایجاد `src/application/holdservice.h`
- ایجاد `src/application/holdservice.cpp`
- تغییر `CMakeLists.txt` فقط برای افزودن دو فایل بالا به target (با تأیید کاربر)
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد گزارش `docs/opencode-reports/HOLD_SERVICE_IMPLEMENTATION.md`

در این مرحله هیچ Controller، Router، Endpoint، `main.cpp`، Migration یا تغییر Git انجام نشد.

### 18.2 پیش‌نیاز Schema

کاربر اجرای موفق Migration 017 را تأیید کرد. با اتصال read-only به `corebanking` موارد زیر تأیید شدند:

- ستون‌های `account_hold.idempotency_key` (`varchar(100)`, nullable) و `account_hold.request_fingerprint` (`varchar(64)`, nullable)
- `ck_hold_idempotency_pair`، `ck_hold_idempotency_key_length` و `ck_hold_fingerprint_length`
- ایندکس یکتای جزئی `uq_hold_api_idempotency` روی `idempotency_key` برای مقادیر غیر NULL
- وجود VIEW `account_hold_status` و Trigger اعتبارسنجی رویداد مطابق Migration 015

هیچ DDL یا DML اجرا نشد.

### 18.3 Build

دستور اجراشده:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی خروجی:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ahmad/projects/build-CoreBanking-Desktop-Debug
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/database.cpp.o
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

Build موفق بود. هیچ هشدار کامپایلر در خروجی مشاهده نشد. پس از یک اصلاح کوچک در بایندینگ پارامتر Idempotency، Build افزایشی دوباره اجرا شد و مجدداً با `[100%] Built target CoreBanking` پایان یافت.

### 18.4 آزمون‌ها

- آزمون HTTP انجام نشد؛ در این مرحله هیچ Controller/Route برای Hold وجود ندارد و فراخوانی سرویس از طریق API محلی ممکن نیست.
- داده آزمایشی ایجاد نشد و هیچ نوشتن مستقیم در دیتابیس انجام نشد.
- بررسی دیتابیس فقط read-only با نقش `corebanking_reader` انجام شد.
- آزمون عملکردی `createHold`/`createEvent`، Idempotency، قفل‌ها و سناریوهای خطا باید در مرحله Controller/Route و با API واقعی انجام و ثبت شود.

### 18.5 نکات پیاده‌سازی

- `createHold`: اعتبارسنجی ورودی، fingerprint بخش 8.1 طراحی، Advisory Lock با پیشوند `HOLD:`، Idempotency Lookup روی `account_hold`، قفل Account → Balance → Hold، بررسی وضعیت حساب/محصول/سرفصل، `AvailableForHold = ledger_balance - active holds`، INSERT با Idempotency، خواندن `account_hold_status` پیش از Commit.
- `createEvent`: پارس و نرمال‌سازی زمان پیش از Idempotency، fingerprint بخش 8.2، Advisory Lock با پیشوند `HOLD-EVENT:`، Pre-check رویداد پیش از اعتبارسنجی وضعیت/مبلغ، قفل Account → Balance → Hold، بازخوانی وضعیت از VIEW، قواعد مبلغ PARTIAL/FULL/EXPIRY/CANCELLATION و نگاشت خطاهای Trigger.
- در همه SELECTها خطای `exec()` از نتیجه صفر ردیف (`query.next() == false`) جدا مدیریت می‌شود و `numRowsAffected()` مبنای تشخیص وجود ردیف نیست.
- در مسیر رقابت نادر `uq_hold_api_idempotency`، برای امکان ادامه Query پس از خطای PostgreSQL از `SAVEPOINT hold_insert` و `ROLLBACK TO SAVEPOINT` استفاده شد؛ سپس ردیف موجود خوانده و نتیجه قبلی برگردانده می‌شود.
- فیلد `replayed` به `HoldResult` و `HoldEventResult` اضافه شد تا Controller مرحله بعد بتواند 201 (ایجاد جدید) را از 200 (Retry) تفکیک کند.
- کد خطای `HOLD_INVALID_REFERENCE` برای طول بیش از حد `referenceType`/`referenceId` اضافه شد؛ این مورد در جدول خطاهای طراحی به‌صراحت نیامده بود ولی برای جلوگیری از خطای 500 ناشی از طول ستون لازم است.
- برای محاسبه موجودی Hold، تابع بارگذاری وضعیت حساب علاوه بر فیلدهای Deposit، ستون `b.ledger_balance` را نیز می‌خواند؛ به همین دلیل SQL آن نسخه تطبیق‌یافته‌ی `loadLockedAccount` است و بخش Overdraft (که برای Hold استفاده نمی‌شود) حذف شده است.

### 18.6 محدودیت‌های باقی‌مانده

- Controller، Route و Endpointهای Hold هنوز وجود ندارند.
- قرارداد پاسخ JSON و نگاشت HTTP (201/200/422/409/404/500) در مرحله Controller پیاده‌سازی و آزمون می‌شود.
- آزمون هم‌زمانی واقعی، Retry با payload متفاوت، موجودی ناکافی و بررسی مالی پس از Commit هنوز اجرا نشده‌اند.

### 18.7 تکمیل HoldService — 2026-11-02

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.h`
- تغییر `src/application/holdservice.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md`
- به‌روزرسانی همین سند
- به‌روزرسانی `docs/opencode-reports/HOLD_SERVICE_IMPLEMENTATION.md`

هیچ Controller، Route، Endpoint، Migration، تغییر Schema، `main.cpp`، `CMakeLists.txt`، `DepositService` یا `TransferService` تغییر نکرد. هیچ عملیات Git انجام نشد.

#### تغییرات پیاده‌سازی

- افزودن `databaseNow()` با خواندن `CURRENT_TIMESTAMP` داخل تراکنش و Truncate به میلی‌ثانیه. زمان پیش‌فرض Hold و Event اکنون از دیتابیس می‌آید.
- Fingerprint بدون زمان ارسالی تغییر نکرده است؛ زمان پیش‌فرض سرور وارد Fingerprint نمی‌شود.
- Idempotency Lookup در `createHold` و `createEvent` پیش از اعتبارسنجی‌های وابسته به وضعیت فعلی Hold باقی مانده است.
- افزودن `HoldStatusData`، `HoldEventData`، `HoldQueryResult`، `HoldListResult` و `HoldEventsResult`.
- افزودن `getHoldById`، `getHoldsByAccount` و `getHoldEvents`.
- افزودن `snapshot` و `hasSnapshot` به `HoldResult` و `HoldEventResult` و پرکردن آن در همه مسیرهای `Completed` پیش از Commit، شامل Retry و بازیابی Unique Conflict.
- مقاوم‌سازی نگاشت خطا با SQLSTATE، `databaseText()`، `text()` و نام Constraint/Index. کدهای اعتبارسنجی Trigger به Rejected و کدهای immutability/ناشناخته به Failed نگاشت می‌شوند.
- افزودن `SAVEPOINT hold_event_insert` و مسیر بازیابی `uq_account_hold_event_idempotency`. شکست `ROLLBACK TO SAVEPOINT` باعث پایان تراکنش با خطا می‌شود و ادامه Query انجام نمی‌شود.

#### Build

دستور اجراشده:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق. هشدار کامپایلری در خروجی مشاهده نشد.

#### آزمون‌ها

- **آزمون HTTP: انجام نشد.** در این مرحله Controller/Route/Endpoint وجود ندارد و نوشتن از طریق API ممکن نیست.
- **آزمون نوشتن Hold/Event: انجام نشد.** هیچ داده آزمایشی ایجاد نشد و هیچ DML اجرا نشد.
- **بررسی Schema: فقط read-only** با نقش `corebanking_reader` روی `corebanking`:
  - وجود `account_hold.idempotency_key` و `request_fingerprint`
  - وجود ایندکس `uq_hold_api_idempotency`
  - وجود ستون‌های VIEW `account_hold_status`
  - وجود Triggerهای `trg_validate_account_hold_event` و `trg_account_hold_event_immutable`
  - خروجی نمونه `CURRENT_TIMESTAMP` با فرمت ISO-8601 UTC و میلی‌ثانیه
- **آزمون مسیرهای خواندن:** یک Harness موقت خارج از مخزن در `/tmp/opencode/hold_read_test.cpp` با اتصال read-only به نقش `corebanking_reader` اجرا شد. موارد بررسی‌شده:
  - `getHoldById(0)` → `Failed` با `HOLD_INVALID_HOLD_ID`
  - `getHoldById(999999999)` → `NotFound` با `HOLD_NOT_FOUND`
  - `getHoldsByAccount(0)` → `Failed` با `HOLD_INVALID_ACCOUNT_ID`
  - `getHoldsByAccount(1)` → `Completed` با لیست خالی
  - `getHoldEvents(0)` → `Failed` با `HOLD_INVALID_HOLD_ID`
  - `getHoldEvents(999999999)` → `HoldNotFound` با `HOLD_NOT_FOUND`
  - خروجی: `READ_METHOD_CHECKS_PASSED`
- در زمان اجرای آزمون، جدول‌های `account_hold` و `account_hold_events` صفر ردیف داشتند؛ بنابراین مسیرهای خواندن با داده واقعی Hold و Event و همچنین Snapshot مسیر Create هنوز آزمون نشده‌اند.

#### شناسه‌های ایجادشده

هیچ شناسه کسب‌وکاری جدیدی ایجاد نشد.

#### محدودیت‌های باقی‌مانده

- Controller، Route و Endpointهای Hold هنوز وجود ندارند.
- آزمون HTTP، Idempotency واقعی، Snapshot پس از Create، خطاهای Trigger و هم‌زمانی باید در مرحله Controller با API واقعی انجام شوند.
- `database/CURRENT_SCHEMA.sql` نسبت به Migration 017 همچنان قدیمی است؛ در این مرحله تغییر نکرد.

### 18.8 HoldController و اتصال HTTP — 2026-11-02

#### محدوده و فایل‌ها

- ایجاد `src/web/holdcontroller.h`
- ایجاد `src/web/holdcontroller.cpp`
- تغییر `src/web/httprouter.h` و `src/web/httprouter.cpp`
- تغییر `src/main.cpp`
- تغییر `CMakeLists.txt` برای افزودن فایل‌های Controller
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد `docs/opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md`

هیچ Migration، DDL/DML مستقیم، تغییر Schema، تغییر `DepositService`/`TransferService` یا عملیات Git انجام نشد.

#### Endpointهای پیاده‌شده

- `POST /api/v1/holds`
- `GET /api/v1/holds/{id}`
- `GET /api/v1/holds?accountId={id}`
- `POST /api/v1/holds/{id}/events`
- `GET /api/v1/holds/{id}/events`

Controller برای پاسخ Create از Snapshot سرویس استفاده می‌کند و پس از Commit هیچ Query جدیدی اجرا نمی‌کند. ایجاد جدید `201`، Retry دقیق `200`، تعارض Idempotency `409`، ورودی نامعتبر `400`، Hold/Account ناموجود `404`، رد کسب‌وکاری `422` و خطای داخلی `500` برمی‌گرداند. نبود Controller در Router نیز `503 HOLD_SERVICE_UNAVAILABLE` است.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ahmad/projects/build-CoreBanking-Desktop-Debug
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

در تلاش اول، خطای نوع `QString` در برابر `HttpResponse::status` مشاهده و اصلاح شد؛ Build نهایی موفق بود.

#### آزمون HTTP محلی

سرور محلی از Build فعلی روی `http://localhost:8080` اجرا و پس از آزمون متوقف شد. همه نوشتن‌ها از API انجام شدند.

| سناریو | نتیجه |
|---|---|
| `POST /api/v1/holds` با `stage3-hold-001-20261102` و مبلغ `0.10` | `201`، `holdId=1`، status=`ACTIVE`، snapshot کامل |
| Retry همان Hold با همان Payload | `200`، همان `holdId=1`، `replayed=true` |
| همان کلید با Payload متفاوت | `409 IDEMPOTENCY_KEY_REUSED` |
| `GET /api/v1/holds/1` | `200`، Snapshot منطبق |
| `GET /api/v1/holds/999999999` | `404 HOLD_NOT_FOUND` |
| `GET /api/v1/holds?accountId=1` | `200`، `count=1` |
| `GET /api/v1/holds` بدون `accountId` | `400 MISSING_ACCOUNT_ID` |
| `GET /api/v1/holds?accountId=abc` | `400 HOLD_INVALID_ACCOUNT_ID` |
| `GET /api/v1/holds/abc` | `400 INVALID_HOLD_ID` |
| `POST /holds/1/events` با PARTIAL `0.04` | `201`، `eventId=1`، remaining=`0.06`، status=`PARTIALLY_RELEASED` |
| Retry همان Event | `200`، همان `eventId=1`، `replayed=true` |
| `POST /holds/1/events` با FULL_RELEASE | `201`، `eventId=2`، remaining=`0`، status=`RELEASED` |
| Retry FULL_RELEASE | `200`، همان `eventId=2`، `replayed=true` |
| همان کلید Event با Payload متفاوت | `409 IDEMPOTENCY_KEY_REUSED` |
| `GET /holds/1/events` | `200`، `count=2`، هر دو Event |
| `POST /holds/999999999/events` | `404 HOLD_NOT_FOUND` |
| `GET /holds/999999999/events` | `404 HOLD_NOT_FOUND` |
| JSON نامعتبر | `400 INVALID_JSON` |
| `eventType` نامعتبر | `400 HOLD_INVALID_EVENT_TYPE` |
| PARTIAL برابر remaining | `422 PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT` |
| رویداد با `effectiveAt` آینده | `422 ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED` |
| Hold با مبلغ بیشتر از موجودی قابل Hold | `422 INSUFFICIENT_AVAILABLE_FUNDS` |
| Hold برای حساب ناموجود | `404 ACCOUNT_NOT_FOUND` |

داده‌های ساخته‌شده:

- Hold `1`: `stage3-hold-001-20261102`، مبلغ `0.10`، وضعیت نهایی `RELEASED`
- Event `1`: `PARTIAL_RELEASE` مبلغ `0.04` روی Hold `1`
- Event `2`: `FULL_RELEASE` مبلغ `0.06` روی Hold `1`
- Hold `2`: `stage3-hold-002-20261102`، مبلغ `0.05`، وضعیت نهایی `RELEASED`
- Event `3`: `FULL_RELEASE` مبلغ `0.05` روی Hold `2`

بررسی read-only دیتابیس:

- `account_balance` حساب `1` قبل و بعد از همه عملیات: `ledger_balance=1`، `available_balance=1`
- `account_hold_status`: Holdهای `1` و `2` با `remaining_amount=0`، `currently_blocked_amount=0` و وضعیت `RELEASED`
- `account_hold_events`: سه رویداد مطابق موارد بالا

#### محدودیت‌ها و انحراف‌های ثبت‌شده

- `HoldEventResult` در این مرحله فیلدهای واقعی Event را برنمی‌گرداند و Controller آن‌ها را از ورودی درخواست منعکس می‌کرد. این محدودیت در بخش 18.9 رفع شد.
- آزمون هم‌زمانی، بار، Expiry Release واقعی، CANCELLATION_RELEASE و پوشش کامل همه حالت‌های مرزی Trigger انجام نشده است.
- `database/CURRENT_SCHEMA.sql` همچنان نسبت به Migration 017 قدیمی است و تغییر نکرد.

### 18.9 اصلاح HoldEventResult برای بازگرداندن داده واقعی Event — 2026-11-02

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.h`
- تغییر `src/application/holdservice.cpp`
- تغییر `src/web/holdcontroller.h`
- تغییر `src/web/holdcontroller.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد `docs/opencode-reports/HOLD_EVENT_RESULT_FIX.md`

هیچ Migration، DDL/DML مستقیم، تغییر Schema، تغییر منطق کسب‌وکار یا تغییر در سرویس‌های دیگر انجام نشد.

#### تغییرات

- `HoldEventResult` اکنون فیلدهای `eventId`، `holdId`، `eventType`، `amount`، `effectiveAt` و `reasonCode` را دارد.
- یک Loader جدید، رکورد Event را با `eventId` از `account_hold_events` داخل همان تراکنش می‌خواند.
- این Loader در مسیر ایجاد جدید، Trigger RETURN NULL، بازیابی Unique Conflict و Idempotency Retry پیش از Commit فراخوانی می‌شود.
- Controller دیگر از `amount`، `effectiveAt` و `reasonCode` ورودی برای ساخت پاسخ استفاده نمی‌کند و پاسخ را از `HoldEventResult` می‌سازد.
- هیچ Query پس از Commit اضافه نشد.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  7%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  8%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

#### آزمون HTTP

- Hold جدید `3` از API با مبلغ `0.01` و کلید `stage4-hold-001-20261102` ایجاد شد.
- Event بدون `effectiveAt` با `FULL_RELEASE` و کلید `stage4-evt-full-001` ایجاد شد:
  - پاسخ ایجاد: `201`، `eventId=4`، `amount="0.01"`، `effectiveAt="2026-11-02T18:41:31.810Z"`، `holdStatus="RELEASED"`
  - Retry همان درخواست: `200`، همان `eventId=4`، همان `amount` و همان `effectiveAt`، `replayed=true`
- بررسی read-only دیتابیس:
  - رکورد Event `4`: `amount=0.01`، `effective_at=2026-11-02 22:11:31.81+03:30`، `reason_code=API_TEST_EVENT_DEFAULT_TIME`
  - زمان پاسخ `18:41:31.810Z` با مقدار ذخیره‌شده در دیتابیس منطبق است.
- `account_balance` حساب `1` پس از آزمون: `ledger_balance=1`، `available_balance=1`
- Hold `3` در `account_hold_status`: `remaining_amount=0`، `currently_blocked_amount=0`، `status=RELEASED`

#### محدودیت

آزمون هم‌زمانی و مسیرهای Expiry/Cancellation همچنان خارج از این اصلاح باقی مانده‌اند.

### 18.10 اصلاح دقت مبلغ در Hold API — 2026-11-02

#### محدوده و فایل‌ها

- تغییر `src/web/holdcontroller.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- به‌روزرسانی `docs/opencode-reports/HOLD_CONTROLLER_DESIGN.md`
- به‌روزرسانی `docs/opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md`
- ایجاد `docs/opencode-reports/HOLD_AMOUNT_VALIDATION_FIX.md`

هیچ Migration یا تغییر Schema، DDL/DML مستقیم، تغییر `HoldService`، تغییر `DepositService`/`TransferService` یا عملیات Git انجام نشد.

#### منطق اعتبارسنجی

- `amountToText` که ورودی عددی JSON را با `QString::number(value, 'f', 8)` تبدیل می‌کرد حذف شد.
- `amount` در Hold و Event فقط به‌صورت JSON string پذیرفته می‌شود.
- رشته خالی، فاصله ابتدا/انتها و ورودی عددی JSON در Controller با `400 INVALID_AMOUNT` رد می‌شوند.
- اعتبارسنجی دقیق‌تر مقدار (مثبت بودن، حداکثر ۸ رقم اعشار، رد قالب علمی/جداکننده/صفر) در `HoldService` باقی مانده و پاسخ آن `400 HOLD_INVALID_AMOUNT` است.
- `HoldService` مقدار معتبر را قبل از Fingerprint نرمال می‌کند؛ بنابراین `"0.10"` و `"0.1"` یک مقدار canonical دارند.
- ستون‌های مبلغ در Schema فعلی `numeric` بدون precision/scale ثابت هستند؛ محدودیت ۸ رقم اعشار قاعده موجود سرویس است و فرض جدیدی درباره دقت ارز ایجاد نشد.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

#### آزمون‌های HTTP

| ورودی | نتیجه |
|---|---|
| `"0.12"` | `201`، مبلغ ذخیره‌شده `0.12` |
| `"0.12345678"` | `201`، مبلغ ذخیره‌شده دقیقاً `0.12345678` |
| `"0.123456789"` | `400 HOLD_INVALID_AMOUNT` |
| عدد JSON `0.1` | `400 INVALID_AMOUNT` |
| `"0"` | `400 HOLD_INVALID_AMOUNT` |
| `"-0.1"` | `400 HOLD_INVALID_AMOUNT` |
| `""` | `400 INVALID_AMOUNT` |
| `" 0.1"` و `"0.1 "` | `400 INVALID_AMOUNT` |
| `"   "` | `400 INVALID_AMOUNT` |
| `"1e-3"` | `400 HOLD_INVALID_AMOUNT` |
| `"1,000.00"` | `400 HOLD_INVALID_AMOUNT` |
| `"+0.1"` | `400 HOLD_INVALID_AMOUNT` |
| `"01.0"` | `400 HOLD_INVALID_AMOUNT` |
| Event با `amount` عددی JSON | `400 INVALID_AMOUNT` |
| Event با `"0.123456789"` | `400 HOLD_INVALID_AMOUNT` |

آزمون Idempotency canonical:

- ایجاد Hold با `amount="0.10"` و کلید `amount-canonical-20261102` → `201`، `holdId=6`
- Retry همان کلید با `amount="0.1"` → `200`، همان `holdId=6`، `replayed=true`
- مقدار ذخیره‌شده در دیتابیس: `0.1`

#### داده‌های آزمایشی جدید

- Hold `4`، مبلغ `0.12`، کلید `amount-valid-2dp-20261102`، وضعیت نهایی `RELEASED`
- Hold `5`، مبلغ `0.12345678`، کلید `amount-valid-8dp-20261102`، وضعیت نهایی `RELEASED`
- Hold `6`، مبلغ `0.1`، کلید `amount-canonical-20261102`، وضعیت نهایی `RELEASED`
- Eventهای `5`، `6` و `7` برای آزادسازی کامل این Holdها

`account_balance` حساب `1` پس از آزمون: `ledger_balance=1`، `available_balance=1`.

#### محدودیت‌ها

- آزمون هم‌زمانی انجام نشد.
- هیچ مبلغی در مسیر عددی JSON گرد نشد؛ چنین ورودی‌ای اکنون رد می‌شود.

### 18.11 اصلاح Idempotency مربوط به `effectiveAt` رویداد — 2026-11-03

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.cpp`
- تغییر `src/web/holdcontroller.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد `docs/opencode-reports/HOLD_EVENT_EFFECTIVE_AT_IDEMPOTENCY_FIX.md`

هیچ Migration یا تغییر Schema، DDL/DML مستقیم، تغییر `DepositService`/`TransferService`، تغییر `main.cpp`/`CMakeLists.txt` یا عملیات Git انجام نشد.

#### بررسی Fingerprint و علت نقص

- `fingerprintForEvent` شامل `holdId`، `eventType`، `amount` (فقط `PARTIAL_RELEASE`)، `effectiveAt` (نرمال‌شده یا رشته خالی)، `reasonCode`، `referenceType` و `referenceId` است.
- نتیجه Fingerprint رویداد ذخیره نمی‌شود؛ `account_hold_events` ستون `request_fingerprint` ندارد و Lookup فیلدبه‌فیلد است.
- در `lookupEventByKey` قدیمی، اگر Retry بدون `effectiveAt` بود، شرط زمان Wildcard می‌شد و زمان ذخیره‌شده مقایسه نمی‌شد؛ در نتیجه Retry بدون زمان برای رویداد اولیه صریح به‌اشتباه Replay می‌شد.

#### منطق اصلاح‌شده

- Presence/absence زمان در Lookup اضافه شد. برای درخواست بدون `effectiveAt`، ردیف ذخیره‌شده باید `effective_at = date_trunc('milliseconds', created_at)` باشد. برای درخواست با زمان صریح، مقدار ذخیره‌شده باید با زمان نرمال‌شده کلاینت برابر باشد و شرط پیش‌فرض بالا را نداشته باشد.
- عدم تطابق `effectiveAt` با کد `IDEMPOTENCY_CONFLICT` و HTTP `409` پاسخ داده می‌شود. این کد در `HoldController` به `409 Conflict` نگاشت شد.
- سایر فیلدها همچنان با `IDEMPOTENCY_KEY_REUSED` و HTTP `409` رد می‌شوند.
- `fingerprintForEvent` تغییر نکرد، چون از قبل «غایب» را با رشته خالی و «صریح» را با مقدار نرمال‌شده تفکیک می‌کرد؛ نقص در منطق Lookup بود.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

#### آزمون‌های HTTP

سرور محلی `http://localhost:8080`، دیتابیس `corebanking`. Hold آزمایشی `7` روی حساب `1` با مبلغ `0.50` ساخته شد. زمان‌های صریح: `T1=2026-11-03T16:23:06.000Z` و `T2=2026-11-03T16:23:07.000Z`.

| سناریو | نتیجه |
|---|---|
| Retry یکسان با `effectiveAt` صریح | `201` سپس `200`، `replayed=true`، همان `eventId=8` |
| Retry یکسان بدون `effectiveAt` | `201` سپس `200`، `replayed=true`، همان `eventId=9` |
| اولیه صریح، Retry بدون زمان | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| اولیه بدون زمان، Retry صریح | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| دو `effectiveAt` صریح متفاوت | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| تفاوت `reasonCode` بدون زمان | `201` سپس `409 IDEMPOTENCY_KEY_REUSED` |

#### بررسی دیتابیس

با نقش read-only `corebanking_reader`:

- برای `hold_id=7` دقیقاً ۶ رویداد با ۶ کلید مجزا ثبت شد؛ Retryها رکورد تکراری ایجاد نکردند.
- مجموع آزادشده `0.06` از مبلغ اولیه `0.50` بود.
- ردیف‌های بدون زمان دارای `effective_at = date_trunc('milliseconds', created_at)` و ردیف‌های صریح دارای مقدار T1 بودند.

#### داده‌های آزمایشی جدید

- Hold `7`، مبلغ `0.50`، کلید `effat-review-1793723166-hold`
- Eventهای `8` تا `13` با کلیدهای `effat-review-1793723166-t1` تا `effat-review-1793723166-t6`

#### محدودیت‌ها

- تشخیص Presence زمان به رابطه `effective_at` با `created_at` وابسته است، چون ستون Presence/Fingerprint در جدول رویداد وجود ندارد.
- اگر زمان صریح درخواست اولیه دقیقاً برابر `date_trunc('milliseconds', created_at)` باشد، تشخیص ممکن است اشتباه شود. رفع قطعی نیازمند Migration و ذخیره Presence/Fingerprint است و در این مرحله انجام نشد.
- آزمون هم‌زمانی برای این تغییر انجام نشد.

### 18.12 پیاده‌سازی Versioned Idempotency رویداد Hold — 2026-11-09

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.cpp`
- تغییر `src/web/holdcontroller.cpp`
- ایجاد `src/application/holdeventfingerprint.h`
- ایجاد `tests/hold_event_fingerprint_and_validation_test.cpp`
- ایجاد `docs/opencode-reports/HOLD_EVENT_VERSIONED_IDEMPOTENCY_IMPLEMENTATION.md`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند

انجام نشد: اجرای Migration 018، هر نوع DDL/DML یا نوشتن در دیتابیس، تغییر فایل Migration یا `CURRENT_SCHEMA.sql`، آزمون HTTP، تغییر `CMakeLists.txt`، `DepositService`، `TransferService` یا `main.cpp`، و هر عملیات Git.

#### Canonicalizer نسخه ۱

- Trim با مجموعه ثابت Unicode White_Space نسخه ۱ پیاده‌سازی شد؛ Whitespace داخلی حفظ می‌شود و U+001C تا U+001F جزو مجموعه نیستند.
- طول فیلدها با `toUtf8().size()` محاسبه می‌شود.
- Amount با حذف صفرهای انتهایی و نقطه اعشاری انتهایی Canonical می‌شود.
- Timestamp به UTC تبدیل و به میلی‌ثانیه Truncate می‌شود. Parser ابتدا کسر ثانیه را به سه رقم Truncate می‌کند تا Round کردن Qt رخ ندهد.
- Timestamp بدون Timezone و Timestamp نامعتبر رد می‌شوند.
- Payload با ترتیب `HOLD_EVENT_FP`، نسخه، HoldId، EventType، AmountProvided/Amount، EffectiveAtProvided/EffectiveAt، ReasonCode، ReferenceType و ReferenceId و Trailing Newline ساخته می‌شود.

#### Test Vectorها

| Vector | Hash |
|---|---|
| A — با `effectiveAt` صریح | `f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1` |
| B — بدون `effectiveAt` | `5e7271fc717f758194af0a0a66247da6a65e34d669359d084876397a3fa5ea8f` |

هر دو Hash در آزمون Unit C++ با Payload دقیقاً هم‌شکل SQL بازتولید شدند.

#### Service و Lookup

- INSERT رویداد اکنون `request_fingerprint`، `fingerprint_version = 1` و `effective_at_provided` را ارسال می‌کند.
- `lookupEventByKey` سه ستون Metadata را می‌خواند.
- هر سه NULL: Legacy Fallback با مقایسه فیلد‌به‌فیلد و heuristic قبلی زمان.
- Metadata ناقص: Fail-Closed با `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID`.
- Version ناشناخته: Fail-Closed با `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED`.
- Version 1: بررسی فرمت و مقایسه Fingerprint؛ Replay فقط با برابری کامل.
- مسیرهای Replay، Conflict، Unique Conflict با Savepoint و Trigger RETURN NULL حفظ شدند.
- تفکیک `IDEMPOTENCY_CONFLICT` و `IDEMPOTENCY_KEY_REUSED` مطابق رفتار قبلی حفظ شد.

#### Validation و نگاشت خطا

- U+0000 در همه ورودی‌های متنی Hold و Event در Controller و Service رد می‌شود.
- Controller کد `INVALID_TEXT_INPUT` و Service کد `HOLD_INVALID_TEXT_INPUT` را با HTTP `400` برمی‌گردانند.
- کدهای اعتبارسنجی جدید Trigger به `422`، Metadata/Fingerprint به `500` و Immutability به `500` نگاشت می‌شوند.
- `triggerErrorInfo` کدهای جدید را می‌شناسد و کدهای Fail-Closed را به `Failed` نگاشت می‌کند.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق.

#### آزمون Unit

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

خروجی:

```text
All hold event fingerprint and validation tests passed
```

پوشش آزمون: Trim نسخه ۱، Amount Canonical، Timestamp با Timezone/بدون Timezone، Truncate میلی‌ثانیه، UTF-8 byte length، هر دو Test Vector، Legacy/Versioned classification، Metadata ناقص، Version ناشناخته، Fingerprint نامعتبر، و U+0000 در Service و Controller.

#### آزمون‌های انجام‌نشده

- Replay و Conflict واقعی از طریق API و دیتابیس پس از Migration 018
- Legacy Retry واقعی بدون و با `effectiveAt` صریح
- هم‌زمانی و Unique Conflict واقعی
- بررسی Read-only ستون‌های Metadata و Fingerprint ردیف‌های جدید
- آزمون Trigger جدید با INSERT مستقیم

#### داده‌های آزمایشی جدید

هیچ داده‌ای از طریق API یا SQL ایجاد یا تغییر نکرد.

#### محدودیت‌ها

- در زمان ثبت این بخش، Migration 018 اجرا نشده بود؛ بنابراین Schema آن زمان ستون‌های جدید را نداشت و Service جدید بدون Migration قابل استقرار نبود. وضعیت به‌روز در بخش 18.14 آمده است.
- Migration 018 بدون Service جدید نیز نوشتن Event را متوقف می‌کند؛ استقرار باید هماهنگ و در Maintenance Window باشد.
- تطابق Hashها در سطح Unit تأیید شد، نه با فراخوانی توابع SQL روی دیتابیس.
- رویه Restore و Rollback باید طبق گزارش آمادگی Migration بررسی شود.

### 18.13 جمع‌بندی نهایی Versioned Idempotency رویداد Hold — 2026-11-11

#### محدوده و فایل‌ها

- تغییر `src/application/holdeventfingerprint.h`
- تغییر `src/application/holdservice.h`
- تغییر `src/application/holdservice.cpp`
- تغییر `tests/hold_event_fingerprint_and_validation_test.cpp`
- ایجاد `docs/opencode-reports/HOLD_EVENT_FINAL_CLOSURE.md`
- ایجاد `docs/opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md`
- به‌روزرسانی کوتاه `docs/PROJECT_CONTEXT.md` و همین سند

انجام نشد: اتصال به دیتابیس، هر نوع DDL/DML یا Migration، تغییر فایل Migration یا `CURRENT_SCHEMA.sql`، تغییر `HoldController`، `CMakeLists.txt`، `DepositService`، `TransferService` یا `main.cpp`، آزمون HTTP/یکپارچه‌سازی، و هر عملیات Git.

#### تعیین تکلیف موارد باز

- تفاوت Legacy مقایسه `amount` برای آزادسازی کامل حفظ شد: Trigger خط ۵۱۸ Migration 018 برای همه انواع مبلغ را مقایسه می‌کند، ولی Lookup سرویس فقط برای `PARTIAL_RELEASE` مبلغ را بررسی می‌کند. مسیر عادی API پیش از INSERT همان رویداد را Replay می‌کند؛ فقط رقابت باریک روی ردیف Legacy می‌تواند `409` بدهد. این رفتار از Migration 015 نیز وجود داشت.
- ریسک Round/Truncate برای Retry Holdهای Legacy تأیید شد: Qt 5.15.8 برای `...06.1239Z` مقدار `...06.124Z` تولید می‌کند، در حالی که Parser جدید `...06.123Z` می‌سازد. سازگاری صرفاً در کد اضافه شد: Fingerprint فعلی و یک Fingerprint کاندید Legacy محاسبه می‌شود و Replay فقط با برابری Fingerprint ذخیره‌شده با یکی از آن دو پذیرفته می‌شود. Timestamp بدون Timezone همچنان طبق قرارداد جدید رد می‌شود.
- پاسخ عمومی چهار کد `ACCOUNT_HOLD_EVENT_FINGERPRINT_*` از متن خام دیتابیس به پیام ثابت تغییر کرد و متن خام فقط با `qWarning` ثبت می‌شود.
- منشأ `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE`، `INVALID_REASON_CODE`، `INVALID_AMOUNT` و `INVALID_EFFECTIVE_AT` فقط شاخه دفاعی Trigger است و از مسیر عادی API قابل‌دسترس نیست. نگاشت `422` به‌عنوان قرارداد API تغییر نکرد.
- تشخیص Metadata به تابع Production `classifyMetadata` منتقل شد و Service از همان تابع استفاده می‌کند.

#### آزمون‌های Unit اضافه‌شده

- همه ۸ ترکیب NULL/غیرNULL برای سه ستون Metadata.
- Versionهای `0`، `-1` و `2` و Fingerprint کوتاه/uppercase.
- یکسانی Fingerprint برای `0.10` و `0.1`.
- نادیده‌گرفتن مبلغ محاسبه‌شده در Fingerprint `FULL_RELEASE`.
- Timestamp با Offset مثبت، Offset فقط‌ساعت، Offset منفی و Truncate بدون Round.
- سازگاری Fingerprint Hold Legacy: Round قبلی Qt در برابر Truncate جدید و پذیرش Fingerprint ذخیره‌شده Legacy فقط با کاندید Legacy.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  7%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  8%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق.

#### آزمون Unit

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

نتیجه: موفق و بدون اتصال به دیتابیس.

#### داده‌های آزمایشی جدید

هیچ داده‌ای از طریق API یا SQL ایجاد یا تغییر نکرد.

#### محدودیت‌ها

- در زمان ثبت این بخش، Migration 018 و آزمون یکپارچه‌سازی هنوز اجرا نشده بودند؛ وضعیت به‌روز در بخش 18.14 آمده است.
- در زمان ثبت این بخش، تطابق Fingerprint با توابع SQL روی دیتابیس آزمون نشده بود؛ این آزمون در بخش 18.14 انجام شد.
- سازگاری Holdهای Legacy با Timestamp بیش از سه رقم اعشار در کد اضافه شد و آزمون Unit دارد؛ در زمان ثبت این بخش آزمون Regression دیتابیس هنوز اجرا نشده بود. آزمون مصنوعی پس از 018 در بخش 18.14 انجام شد.
- در 2026-11-11 کاربر حفظ HTTP `400` برای Timestamp بدون Timezone را پذیرفت؛ این مورد به‌عنوان محدودیت شناخته‌شده ثبت شد و اصلاح جدیدی انجام نشد.
- بررسی فقط‌خواندنی پیش از Migration روی `corebanking`: Schema روی 017 بود و 018 اعمال نشده بود؛ 7 Hold و 13 Event موجود بود. Eventهای 8 و 9 به‌عنوان Fixture Legacy با زمان صریح و غایب قابل استفاده‌اند. Hold 7 با زمان صریح روی مرز `.000` است و مسیر Fallback چهاررقمی را فعال نمی‌کند؛ برای این آزمون از Hold مصنوعی پس از 018 استفاده می‌شود. باینری قدیمی پیش از 018 موجود نبود و سرویس در حال اجرا نبود. هیچ نوشتن، API یا تغییر باینری انجام نشد.
- برنامه آزمون دیتابیس توسعه `corebanking` در `docs/opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md` آماده شد؛ اجرای پس از 018 در بخش 18.14 و بخش ۱۲ همان گزارش ثبت شده است.
- تصمیم درباره نگاشت `ACCOUNT_HOLD_EVENT_INVALID_*` به `422` یا `500` باقی است.
- هیچ رویه Restore/Rollback آزمون یا تأیید نشده است.

### 18.14 استقرار و آزمون یکپارچه‌سازی Hold پس از Migration 018 — 2026-11-11

#### محدوده و فایل‌ها

- تغییر `docs/PROJECT_CONTEXT.md`
- تغییر `docs/PROJECT_HISTORY.md`
- تغییر `docs/opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md` (افزودن بخش ۱۲ نتایج و دستورهای باقی‌مانده)
- کد برنامه، `CMakeLists.txt`، فایل Migration و `CURRENT_SCHEMA.sql` در این مرحله توسط مدل تغییر نکردند.

اقدامات کاربر پیش از این مرحله: تهیه دامپ کامل پیش از 018 در `database/backups`، اجرای موفق Migration 018 با COMMIT، و به‌روزرسانی `database/CURRENT_SCHEMA.sql` با pg_dump از دیتابیس واقعی.

انجام نشد: هیچ SQL نوشتنی مستقیم، Migration، DDL، تغییر Schema، Build مجدد، اجرای Unit Test مجدد، یا عملیات Git توسط مدل انجام نشد. آزمون‌های SQL کنترل‌شده بخش ۸ برنامه آزمون نیز اجرا نشدند.

#### بررسی read-only پس از 018

اتصال فقط با نقش `corebanking_reader` روی `corebanking`:

- ستون‌های `account_hold_events.request_fingerprint` (`character varying`)، `fingerprint_version` (`smallint`) و `effective_at_provided` (`boolean`) موجودند.
- Constraintهای `ck_account_hold_event_fingerprint_format`، `ck_account_hold_event_fingerprint_version` و `ck_account_hold_event_idempotency_metadata` موجودند؛ Constraintهای قبلی `amount`، `idempotency_key`، `reason` و `type` نیز حفظ شده‌اند.
- توابع `hold_event_fp_trim(text)`، `hold_event_canonical_amount(numeric)`، `hold_event_fingerprint_v1(...)` و `hold_event_fingerprint(...)` موجودند.
- Triggerهای `trg_validate_account_hold_event` و `trg_account_hold_event_immutable` فعال‌اند.
- `database/CURRENT_SCHEMA.sql` همین ستون‌ها، Constraintها، توابع و Triggerها را منعکس می‌کند.
- پیش از نوشتن جدید: `total_events=13`، `legacy_events=13`، `non_legacy_events=0`، `hold_count=7`.
- پس از آزمون: `total_events=17`، `legacy_events=13`، `versioned_events=4`، `inconsistent_metadata=0`، `hold_count=9`.

#### اجرای باینری جدید

- باینری: `/home/ahmad/projects/build-CoreBanking-Desktop-Debug/CoreBanking` با زمان `2026-11-11 16:51`.
- فرآیند جدید با PID `5240` اجرا شد و روی پورت `8080` گوش می‌دهد.
- `GET /api/v1/health` پاسخ `200` با `{"data":{"status":"UP"},"success":true}` داد.
- باینری قدیمی اجرا نشد.

#### آزمون تطابق Fingerprint C++ و SQL

یک Probe موقت در `/tmp/fp_probe.cpp` با `HoldEventFingerprintV1::fingerprint` کامپایل و اجرا شد. برای Test Vector A و ورودی با `reasonCode` فارسی `سلام`:

- C++ Vector A: `f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1`
- SQL Vector A: `f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1`
- C++ فارسی: `3ece7c699eb93232f9d0589a92e720d51dc1871eb14eb274a8decf13e7756ee2`
- SQL فارسی: `3ece7c699eb93232f9d0589a92e720d51dc1871eb14eb274a8decf13e7756ee2`

نتیجه: برابر و موفق.

#### آزمون‌های API

- ایجاد Hold جدید (حساب `1`، مبلغ `0.10`، کلید `hold018-api-20261111-h1`) → `201`، `holdId=8`.
- ایجاد Event (`PARTIAL_RELEASE`، `0.01`، `R`، کلید `hold018-api-20261111-e1`) → `201`، `eventId=14`.
- همان درخواست → `200`، همان `eventId=14`، `replayed=true`.
- همان کلید با `effectiveAt` متفاوت → `409 IDEMPOTENCY_CONFLICT`.
- همان کلید با `amount` متفاوت → `409 IDEMPOTENCY_KEY_REUSED`.
- Legacy Event `8` (`effat-review-1793723166-t1`) با `effectiveAt=2026-11-03T16:23:06.000Z` → `200 replayed=true`، همان `eventId=8`.
- Legacy Event `9` (`effat-review-1793723166-t2`) بدون `effectiveAt` → `200 replayed=true`، همان `eventId=9`.
- همان کلید Legacy با زمان متفاوت `2026-11-03T16:28:25.390Z` → `409 IDEMPOTENCY_CONFLICT`.
- Hold مصنوعی پس از 018 با `effectiveAt=2026-11-03T16:23:06.123Z` و کلید `hold018-api-20261111-h2` → `201`، `holdId=9`.
- Retry همان Hold با `effectiveAt=2026-11-03T16:23:06.1229Z` → `200 replayed=true`، همان `holdId=9` (مسیر Fallback Round/Truncate).
- هم‌زمانی دو درخواست با کلید یکسان `hold018-api-20261111-c1` روی Hold `8` → یکی `201 eventId=15` و دیگری `200 replayed=true` با همان `eventId=15`؛ فقط یک ردیف.
- هم‌زمانی دو درخواست با کلیدهای متفاوت `hold018-api-20261111-c2a` و `hold018-api-20261111-c2b` → دو Event مستقل `eventId=16` و `eventId=17` با `201`.
- Timestamp بدون Timezone (`2026-11-03T16:23:06`) → `400 HOLD_INVALID_EFFECTIVE_AT`؛ هیچ ردیفی با کلید `hold018-api-20261111-tz` ایجاد نشد.

#### داده‌های ایجادشده

- Hold `8`، حساب `1`، کلید `hold018-api-20261111-h1`، مبلغ `0.10`، `reasonCode=H018`، وضعیت نهایی `PARTIALLY_RELEASED`، رویدادهای `14` تا `17`، آزادشده `0.04`، باقی‌مانده `0.06`.
- Hold `9`، حساب `1`، کلید `hold018-api-20261111-h2`، مبلغ `0.02`، `reasonCode=H018-ROUND`، `effectiveAt=2026-11-03T16:23:06.123Z` (`.1229Z` به همان Replay شد)، بدون Event، باقی‌مانده `0.02`.
- Eventهای جدید `14` (`hold018-api-20261111-e1`)، `15` (`hold018-api-20261111-c1`)، `16` (`hold018-api-20261111-c2a`) و `17` (`hold018-api-20261111-c2b`) روی Hold `8`؛ همه `PARTIAL_RELEASE` با مبلغ `0.01`، Metadata کامل، `fingerprint_version=1` و `effective_at_provided=false`.
- هیچ رکورد قبلی تغییر یا حذف نشد.

#### کنترل SQL پس از آزمون

- کنترل مجموع برای همه Holdها سازگار است؛ برای Hold `8`: مبلغ اولیه `0.10`، آزادشده `0.04`، باقی‌مانده `0.06`، `event_count=4`.
- Hold `7` بدون تغییر: مبلغ اولیه `0.50`، آزادشده `0.06`، باقی‌مانده `0.44`، `event_count=6`.
- تعداد ردیف‌های کلید هم‌زمانی `hold018-api-20261111-c1` برابر `1` است.
- تعداد کلید Timestamp بدون Timezone برابر `0` است.

#### وضعیت موارد تکمیلی و محدودیت‌ها

- آزمون‌های SQL کنترل‌شده بخش ۸ برنامه آزمون (رد Metadata ناقص، Version نامعتبر، Fingerprint بدفرمت و Trigger RETURN NULL روی ردیف Legacy) بخشی از دامنه آزمون API این مرحله نیستند و اجرای آن‌ها به تصمیم کاربر خارج از این دامنه واگذار شده است. اجرا نشده‌اند و موفق اعلام نمی‌شوند. دستورهای دقیق در بخش ۱۲ گزارش `HOLD_018_COREBANKING_TEST_PLAN.md` آمده است.
- Timestamp بدون Timezone برای Hold/Event همچنان `400` است؛ این محدودیت پذیرفته‌شده باقی می‌ماند.
- آزمون Round/Truncate روی Hold مصنوعی ساخته‌شده پس از 018 انجام شد و ادعای آزمون کامل رکورد ساخته‌شده با باینری قدیمی نیست.
- پوشش Expiry/Cancellation در بخش 18.15 تکمیل شد. آزمون بار، مرزهای اضافه و رویه Restore/Rollback صرفاً آزموده‌نشده‌اند و شرط پایان این مرحله نیستند.
- سرویس جدید در پایان مرحله در حال اجرا روی پورت `8080` باقی ماند.

### 18.15 آزمون API مسیرهای Expiry/Cancellation Hold — 2026-11-11

#### محدوده و فایل‌ها

- تغییر `docs/PROJECT_CONTEXT.md`
- تغییر `docs/PROJECT_HISTORY.md`
- تغییر `docs/opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md` (افزودن بخش ۱۳)
- هیچ تغییر کد، `CMakeLists.txt`، Migration، Schema یا `CURRENT_SCHEMA.sql` انجام نشد؛ چون مسیرهای `CANCELLATION_RELEASE` و `EXPIRY_RELEASE` از قبل در `HoldService::createEvent` و Endpoint عمومی `POST /api/v1/holds/{id}/events` پیاده‌سازی شده بودند.
- Build مجدد اجرا نشد چون کد تغییر نکرد. هیچ SQL نوشتنی مستقیم، DDL/DML یا عملیات Git انجام نشد.

#### آزمون‌های API

- ایجاد Hold `10` (حساب `1`، مبلغ `0.03`، کلید `hold018-cancel-20261111-h1`) → `201`، وضعیت `ACTIVE`.
- `CANCELLATION_RELEASE` روی Hold `10` با کلید `hold018-cancel-20261111-e1` → `201`، `eventId=18`، `holdStatus=CANCELLED`، `remainingAmount=0`، `currentlyBlockedAmount=0`.
- Retry همان Event → `200`، همان `eventId=18`، `replayed=true`.
- `FULL_RELEASE` جدید روی Hold لغوشده با کلید `hold018-cancel-20261111-e2` → `422 ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`.
- ایجاد Hold `11` (حساب `1`، مبلغ `0.03`، `effectiveAt=2026-11-01T00:00:00Z`، `expiresAt=2026-11-02T00:00:00Z`، کلید `hold018-exp-20261111-h1`) → `201`، وضعیت `EXPIRED`، `currentlyBlockedAmount=0`.
- `CANCELLATION_RELEASE` روی Hold `11` با کلید `hold018-exp-20261111-e0` → `422 EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE`؛ هیچ Eventی ایجاد نشد.
- `EXPIRY_RELEASE` روی Hold `8` (غیرمنقضی) با کلید `hold018-exp-20261111-e2` → `422 ACCOUNT_HOLD_NOT_EXPIRED`؛ هیچ Eventی ایجاد نشد.
- `EXPIRY_RELEASE` روی Hold `11` با کلید `hold018-exp-20261111-e1` → `201`، `eventId=19`، `holdStatus=EXPIRED`، `remainingAmount=0`.
- Retry همان Event → `200`، همان `eventId=19`، `replayed=true`.

#### داده‌های ایجادشده

- Hold `10`، حساب `1`، کلید `hold018-cancel-20261111-h1`، مبلغ `0.03`، وضعیت `CANCELLED`، Event `18` با کلید `hold018-cancel-20261111-e1`.
- Hold `11`، حساب `1`، کلید `hold018-exp-20261111-h1`، مبلغ `0.03`، وضعیت `EXPIRED`، Event `19` با کلید `hold018-exp-20261111-e1`.
- کلیدهای `hold018-exp-20261111-e0` و `hold018-exp-20261111-e2` هیچ ردیفی ایجاد نکردند.

#### کنترل read-only

- `hold_count=11`، `event_count=19`، `legacy_events=13`، `versioned_events=6`.
- Hold `10`: `status=CANCELLED`، `remaining_amount=0.00`، `currently_blocked_amount=0.00`، `release_event_count=1`، `last_release_event_type=CANCELLATION_RELEASE`.
- Hold `11`: `status=EXPIRED`، `remaining_amount=0.00`، `currently_blocked_amount=0`، `release_event_count=1`، `last_release_event_type=EXPIRY_RELEASE`.
- Hold `8` بدون تغییر باقی ماند: `PARTIALLY_RELEASED`، باقی‌مانده `0.06`، `event_count=4`.
- `ledger_balance` حساب‌های `1` و `2` بدون تغییر (`1` و `0`)؛ مسدودی وجه روی دفتر کل اثر ندارد.

#### نتیجه

- نتیجه: مسیرهای Expiry/Cancellation از طریق API فعلی بدون نیاز به کدنویسی جدید موفق و با SQL فقط‌خواندنی تأیید شدند.
- آزمون SQL مستقیم بخش ۸ شرط پایان این مرحله نیست و به تصمیم کاربر خارج از دامنه آزمون API واگذار شده است. آزمون بار، مرزهای اضافه و رویه Restore/Rollback صرفاً آزموده‌نشده‌اند و شرط جدید پایان مرحله نیستند.
- سرویس جدید همچنان روی پورت `8080` در حال اجراست.

### 18.16 بستن مرحله Hold API — 2026-11-11

#### محدوده و فایل‌ها

- تغییر `docs/PROJECT_CONTEXT.md`
- تغییر `docs/PROJECT_HISTORY.md`
- تغییر `docs/opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md` (جمع‌بندی پایانی و رفع ابهام وضعیت بخش ۸)
- هیچ تغییر کد، Build، Migration، Schema، `CURRENT_SCHEMA.sql`، SQL جدید، آزمون جدید یا عملیات Git انجام نشد.

#### وضعیت نهایی

- Migration 018 اجرا شده است و `CURRENT_SCHEMA.sql` پس از آن از دیتابیس واقعی به‌روز و تطابق read-only آن بررسی شده است. چون پس از آن تغییر جدید Schema رخ نداده، فایل دوباره تولید نشد.
- نتایج واقعی API شامل Create/Replay/Conflict، Legacy Retry، هم‌زمانی، Cancellation و Expiry و داده‌های آزمایشی باقی‌مانده در بخش‌های 18.14 و 18.15 ثبت شده‌اند.
- آزمون SQL مستقیم بخش ۸ برنامه آزمون، شرط پایان این مرحله نیست و به تصمیم کاربر خارج از دامنه آزمون API واگذار شده است؛ اجرا نشده و موفق اعلام نمی‌شود.
- آزمون بار، مرزهای اضافه و رویه Restore/Rollback صرفاً آزموده‌نشده‌اند و شرط جدید پایان مرحله نیستند.

#### نتیجه

- کار ضروری دیگری برای پایان API مسدودی باقی نمانده است. مرحله Hold API در دامنه مصوب خود بسته است.

### 18.17 زیرمرحله 1A — Unit of Work و Transaction Slice در Deposit — 2026-11-11

#### محدوده و فایل‌ها

- ایجاد `src/infrastructure/unitofwork.h`
- ایجاد `src/infrastructure/unitofwork.cpp`
- بازنویسی `src/domain/transaction.h`
- بازنویسی `src/domain/transaction.cpp`
- بازنویسی `src/infrastructure/repository/itransactionrepository.h`
- بازنویسی `src/infrastructure/repository/transactionrepository.h`
- بازنویسی `src/infrastructure/repository/transactionrepository.cpp`
- ایجاد `src/application/transactionservice.h`
- ایجاد `src/application/transactionservice.cpp`
- تغییر `src/application/depositservice.cpp` فقط برای Transaction و Idempotency
- تغییر `CMakeLists.txt` فقط برای افزودن فایل‌های جدید
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند

انجام نشد: هیچ Migration، DDL/DML مستقیم، تغییر Schema یا `CURRENT_SCHEMA.sql`، تغییر Connection/Pool/Credential، تغییر Transfer/Hold/افتتاح/بستن حساب، Withdrawal، Reversal، تغییر قرارداد HTTP یا عملیات Git انجام نشد. SQL مربوط به Balance، Journal، TransactionEntry و LedgerBalance در Deposit بازنویسی نشد.

#### بررسی مصرف‌کنندگان پیش از بازنویسی

- `Transaction` و `ITransactionRepository` فقط در `TransactionRepository` قدیمی و فایل‌های خودشان استفاده می‌شدند.
- هیچ سرویس فعال، Controller یا `main.cpp` از این مدل/Repository استفاده نمی‌کرد.
- بنابراین بازنویسی آن‌ها نیازمند تغییر Transfer یا مسیرهای خارج از محدوده نبود.

#### تصمیم‌های اجرایی

- `UnitOfWork` مالک `begin/commit/rollback` است و به‌صورت RAII فقط Unit of Work فعال را Rollback می‌کند.
- شکست Commit به وضعیت `CommitOutcomeUnknown` منتقل می‌شود؛ Destructor در این حالت Rollback انجام نمی‌دهد و نتیجه به‌عنوان Rollback قطعی گزارش نمی‌شود.
- `TransactionService` و `TransactionRepository` هیچ Commit/Rollback مستقلی ندارند.
- چرخه داخلی Deposit: `INITIATED -> PROCESSING -> COMPLETED/REJECTED` در همان تراکنش دیتابیس.
- Idempotency Lookup پیش از اعتبارسنجی شرایط متغیر حساب انجام می‌شود.
- الگوریتم Fingerprint، Canonicalization، دامنه Idempotency و قرارداد HTTP فعلی Deposit دقیقاً حفظ شد.

#### Build

دستور اجراشده:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق. تلاش اول به دلیل نبود Default Constructor در مدل `Transaction` خطای کامپایل داد؛ پس از اصلاح `transactionFromQuery` به `std::optional<Transaction>`، Build نهایی با `[100%] Built target CoreBanking` پایان یافت.

#### آزمون‌های API واقعی

سرور از باینری جدید روی `http://localhost:8080` اجرا شد. آزمون‌ها روی دیتابیس `corebanking` انجام شدند.

| سناریو | نتیجه واقعی |
|---|---|
| Deposit موفق، حساب `1`، مبلغ `0.01`، کلید `stage1a-dep-success-20261111-a1` | `201`، `transactionId=2`، `DEP-1794428505308-3415EF73` |
| Retry دقیق همان درخواست | `201`، همان `transactionId=2` و همان `transactionNumber` |
| همان کلید با مبلغ `0.02` | `422 IDEMPOTENCY_KEY_REUSED` |
| Retry کلید پیش از بازنویسی `deposit-main-20261026-a74c91` با Payload اصلی | `201`، همان `transactionId=1`، `DEP-1792961783964-C2534ABA` |
| همان کلید قدیمی با مبلغ `2` | `422 IDEMPOTENCY_KEY_REUSED` |
| Deposit به حساب `2` (CLOSED)، کلید `stage1a-dep-reject-20261111-a1` | `422 ACCOUNT_NOT_ACTIVE` |
| Retry همان درخواست ردشده | `422 ACCOUNT_NOT_ACTIVE` |
| همان کلید ردشده با مبلغ متفاوت | `422 IDEMPOTENCY_KEY_REUSED` |
| مبلغ `"0"`، کلید `stage1a-dep-invalid-20261111-a1` | `422 INVALID_AMOUNT` |
| درخواست بدون `idempotencyKey` | `400 INVALID_DEPOSIT_REQUEST` |

دو درخواست HTTP پشت‌سرهم/هم‌زمان‌شده با کلید `stage1a-dep-concurrent-20261111-a1`:

- هر دو `201` با همان `transactionId=4` و `DEP-1794428619566-4D11C33B` برگشتند.
- فقط یک ردیف Transaction با این کلید ایجاد شد.

محدودیت آزمون هم‌زمانی: سرور تک‌ریسمانی است و Connection مشترک دارد؛ بنابراین این نتیجه Replay منطقی همان کلید را نشان می‌دهد، نه اثبات هم‌پوشانی واقعی دو تراکنش دیتابیس.

#### کنترل SQL فقط‌خواندنی

- Transactionهای ایجادشده/موجود: `1` تا `4`.
- `transaction 2` و `4`: `COMPLETED`، هر کدام یک Journal با وضعیت `POSTED`، دو سطر، Debit=`0.01` و Credit=`0.01`.
- `transaction 3`: `REJECTED` با `result_code=ACCOUNT_NOT_ACTIVE`؛ بدون TransactionEntry و بدون Journal.
- برای کلید `stage1a-dep-invalid-20261111-a1` هیچ ردیفی ایجاد نشد.
- هیچ کلید Idempotency تکراری در `channel='API'` وجود ندارد.
- `account_balance` حساب `1`: `ledger_balance=1.02`، `available_balance=0.50`؛ Active Holds برابر `0.52`، بنابراین `1.02 - 0.52 = 0.50`.
- `account_balance` حساب `2` بدون تغییر: `ledger_balance=0`، `available_balance=0`.
- `ledger_account_balance` سرفصل `1`: `credit_total=1.02`، `balance=-1.02`؛ سرفصل `4`: `debit_total=1.02`، `balance=1.02`.
- تغییر `available_balance` ناشی از SQL فعلی Deposit است که در این مرحله بازنویسی نشد؛ مسئله Cache کهنه‌شونده همچنان برای مرحله Balance باقی است.

#### داده‌های آزمایشی باقی‌مانده

- Transaction `2`، حساب `1`، مبلغ `0.01`، کلید `stage1a-dep-success-20261111-a1`، وضعیت `COMPLETED`.
- Transaction `3`، حساب `2`، مبلغ `0.01`، کلید `stage1a-dep-reject-20261111-a1`، وضعیت `REJECTED`.
- Transaction `4`، حساب `1`، مبلغ `0.01`، کلید `stage1a-dep-concurrent-20261111-a1`، وضعیت `COMPLETED`.
- Transaction `1` و Journal `1` بدون تغییر باقی ماندند.

#### محدودیت‌ها و موارد باقی‌مانده

- Rollback میانی پس از چند نوشتن و پیش از Commit آزمون نشد؛ هیچ Fault Injection یا Test Hook جدیدی اضافه نشد.
- نتیجه نامعلوم Commit آزمون نشد. رفتار UnitOfWork برای این حالت در کد صریح است، اما شاهد اجرایی ندارد.
- آزمون Retry با شرایط متغیر حساب (مثلاً تغییر وضعیت حساب بین درخواست اول و Retry) اجرا نشد؛ ترتیب کد Lookup پیش از اعتبارسنجی حساب حفظ شده است، اما اثبات سناریوی متغیر نیازمند داده/محیط کنترل‌شده است.
- Transfer، Hold، افتتاح حساب و بستن حساب همچنان از مسیرهای قبلی استفاده می‌کنند.
- شکاف قفل قاعده مؤثر ناموجود برای `FOR SHARE` حل نشده و برای پیش از Balance Slice ثبت شده است.
- `available_balance` همچنان Cache کهنه‌شونده است؛ هیچ تصمیم مالی جدیدی در این مرحله بر آن بنا نشد.
- سرویس جدید در پایان مرحله روی پورت `8080` در حال اجرا باقی ماند.

### 18.18 پذیرش محدود زیرمرحله 1A — 2026-11-12

#### دامنه پذیرش

- 1A در دامنه محدود پذیرفته شد: UnitOfWork و مالکیت تراکنش در Deposit، TransactionService و قرارداد Repository، حفظ API/fingerprint/idempotency و Retry قدیمی، عدم تغییر SQL مالی و ترتیب Balance/Journal، و اصلاح محدود F1/F2 با گزینه 1.
- گزارش پذیرش: [1A_ACCEPTANCE_WITH_LIMITATIONS.md](opencode-reports/1A_ACCEPTANCE_WITH_LIMITATIONS.md).

#### شواهد

- آزمون‌های API قبلی 1A (2026-11-11) در بخش 18.17 همین سند خلاصه شده‌اند: Deposit موفق، Retry دقیق، تعارض کلید، Retry کلید قدیمی، رد حساب CLOSED، مبلغ نامعتبر، درخواست بدون کلید و دو درخواست پشت‌سرهم با کلید یکسان. این نتایج در نوبت پذیرش دوباره اجرا نشدند.
- Build پس از اصلاح F1/F2 در نوبت پیاده‌سازی موفق بود: `[100%] Built target CoreBanking`. در نوبت پذیرش، Build تکرار نشد.
- Harness هدفمند F1/F2 با UnitOfWork واقعی و FakeDriver کنترل‌شده (Qt 5.15.8) اجرا شد؛ سناریوهای A/B/C/D/E1/E2/E3 PASS و `TOTAL FAILURES: 0`:
  - A (commit ناموفق): begin=1 commit=1 rollback=0 close=1؛ open true→false.
  - B (rollback صریح ناموفق): begin=1 commit=0 rollback=1 close=1؛ open true→false.
  - C (rollback ناموفق در Destructor): begin=1 commit=0 rollback=1 close=1؛ open true→false.
  - D (BEGIN ناموفق): begin=1 commit=0 rollback=0 close=1؛ open true→false.
  - E1/E2/E3 (مسیرهای موفق): close=0 و open true→true.
- آزمون QSQLITE برای shared-close PASS: close روی یک کپی، `isOpen()` کپی دیگر را false کرد و Query روی آن شکست خورد.
- ثبت State از کدخوانی و رفتار متناظر از Harness تأیید شده است؛ دسترسی مستقیم به private انجام نشده است.
- گزارش‌های بازبینی/پیاده‌سازی/Harness در `/tmp` بودند و پیش از این سند، گزارش دائمی 1A در `docs/opencode-reports/` وجود نداشت.

#### محدودیت‌های پذیرش

- رفتار واقعی QPSQL/PostgreSQL در قطع ارتباط و Retry پس از Commit نامعلوم آزموده نشده است؛ Harness فقط منطق UnitOfWork و قرارداد Qt SQL را می‌سنجد.
- پس از شکست‌های مشخص‌شده، اتصال مشترک بسته می‌ماند و دسترسی دیتابیس تا restart متوقف می‌شود. restart نتیجه Commit قبلی را تعیین نمی‌کند؛ Retry باید با همان idempotency key باشد.
- مسیرهای legacy (Transfer/Hold/AccountOpening/AccountService و چند Repository) خارج از این اصلاح‌اند و ریسک مشترک اتصال دارند.
- مدیریت مرکزی اتصال/recovery (گزینه 2) مرحله مستقل آینده است و اجرا نشد.
- پذیرش محدود 1A معادل آمادگی کامل برای بهره‌برداری بانکی نیست. 1B و Balance Slice شروع نشده‌اند؛ شکاف هم‌زمانی قواعد مؤثر و نبود Override باید پیش از Balance Slice تعیین تکلیف شود.

### 18.19 بازآرایی مستندات — 2026-11-12

### دامنه و فایل‌ها

- تغییر `docs/PROJECT_CONTEXT.md` برای تبدیل آن به مرجع عملی وضعیت و قواعد جاری.
- تغییر همین سند فقط برای افزودن این ورود تاریخی و حفظ سوابق قبلی.
- ایجاد گزارش‌های دائمی زیر `docs/opencode-reports/`:
  - `1A_CODE_REVIEW.md`
  - `1A_F1_F2_IMPLEMENTATION.md`
  - `1A_F1_F2_HARNESS_EVIDENCE.md`
  - `DOCUMENTATION_REORGANIZATION.md`
- هیچ فایل کد، CMake، Schema، Migration، تنظیم اتصال یا Credential تغییر نکرد. Build، HTTP، SQL، Fault Injection و restart اجرا نشد. Git فقط read-only بررسی شد و stage/commit/branch انجام نشد.

### تغییر ساختار Context

- یک «راهنمای سریع عامل» در ابتدای Context اضافه شد: وضعیت جاری 1A/1B/Balance Slice، سیاست fail-closed، ابهام Commit و Retry با همان idempotency key، ریسک legacy، هشدار `available_balance` کهنه و نقاط توقف نیازمند تأیید.
- یک خلاصه «معماری جاری» اضافه شد: لایه‌ها، Connection مشترک و مالکیت آن، مالکیت تراکنش در 1A، Idempotency و ثبت مالی.
- شرح‌های طولانی اجرای گذشته Hold که در Context تکرار شده بودند، با خلاصه قواعد جاری و ارجاع به `PROJECT_HISTORY.md` بخش‌های 18.7 تا 18.16 و گزارش‌های Hold جایگزین شدند.
- جزئیات تاریخی 1A در Context خلاصه شد و به بخش‌های 18.17 و 18.18 همین سند و گزارش‌های `1A_*` ارجاع داده شد.
- شماره بخش‌های اصلی 1 تا 20 حفظ شد. بخش‌های جدید ابتدای Context بدون شماره هستند تا ارجاعات موجود شکسته نشوند.

### حفظ شواهد و انتقال به گزارش‌های دائمی

- اجرای اولیه 1A و آزمون‌های API تاریخ 2026-11-11 از قبل در بخش 18.17 همین سند نگهداری می‌شوند و دوباره اجرا نشدند.
- گزارش بازبینی کد از `/tmp/core-banking-1a-review.md` به `docs/opencode-reports/1A_CODE_REVIEW.md` منتقل شد؛ متن اصلی حفظ شد و به‌عنوان شاهد تاریخی، نه اجرای جدید، علامت‌گذاری شد.
- گزارش پیاده‌سازی F1/F2 از `/tmp/core-banking-1a-f1-f2-implementation.md` به `docs/opencode-reports/1A_F1_F2_IMPLEMENTATION.md` منتقل شد.
- گزارش Harness و خروجی خام `/tmp/core-banking-f1-f2-test/run.log` در `docs/opencode-reports/1A_F1_F2_HARNESS_EVIDENCE.md` نگهداری شد.
- فایل حجیم `/tmp/core-banking-1a-review-code.txt` (شامل متن کامل کد و diff) کپی دائمی نشد؛ محتوای آن با کد جاری و گزارش بازبینی هم‌پوشانی دارد و به‌عنوان منبع موقت در گزارش بازآرایی ثبت شد.
- طرح `/tmp/core-banking-1a-failure-handling-plan.md` کپی دائمی نشد چون یک سند طراحی پیش از پیاده‌سازی است و گزارش پیاده‌سازی F1/F2 تصمیم اجراشده را ثبت می‌کند.
- پاسخ `/tmp/core-banking-1a-acceptance-response.md` کپی نشد چون با گزارش دائمی `1A_ACCEPTANCE_WITH_LIMITATIONS.md` و بخش 18.18 همین سند هم‌پوشانی دارد.
- هیچ اطلاعات حساسی در منابع منتقل‌شده یافت نشد؛ بنابراین موردی برای `[REDACTED]` وجود نداشت.

### تناقض‌ها و لینک‌ها

- تناقض مستندی درباره وضعیت جاری یافت نشد: 1A پذیرفته‌شده محدود، 1B شروع‌نشده و Balance Slice شروع‌نشده در Context و History هم‌راستا هستند.
- یک ابهام مستنداتی درباره جدول انتقال Transaction رفع شد: مسیر Deposit فقط `INITIATED -> PROCESSING -> COMPLETED/REJECTED` را استفاده می‌کند، اما مدل Domain جدول انتقال بخش 10 (شامل `INITIATED -> CANCELLED`) را حفظ می‌کند. این تفکیک در Context بخش 18.5 ثبت شد و رفتار جدیدی ایجاد نکرد.
- لینک Context به گزارش پذیرش و گزارش‌های `1A_*` و لینک History به گزارش پذیرش بررسی شدند و مسیر نسبی آن‌ها معتبر است.
- ارجاعات به بخش‌های 18.7 تا 18.18 و بخش‌های 16.1، 16.4 و 3 History حفظ شدند.

### محدودیت‌ها

- گزارش دائمی مستقل برای «اجرای اولیه 1A» وجود نداشت؛ شاهد اصلی همان بخش 18.17 History است.
- خروجی خام کامل HTTP آزمون‌های 2026-11-11 در مخزن موجود نیست و بازسازی نشد.
- رفتار واقعی QPSQL/PostgreSQL در قطع ارتباط و Retry پس از Commit نامعلوم همچنان اثبات‌نشده است.
- هیچ تصمیم معماری یا رفتاری جدیدی در این بازآرایی ایجاد نشد.

### 18.20 زیرمرحله 1A.1 — انتقال قرارداد Transaction/UoW — 2026-11-12

#### محدوده و فایل‌ها

- ایجاد `src/application/contracts/operationerror.h`
- ایجاد `src/application/contracts/itransactionrepository.h`
- ایجاد `src/application/contracts/itransactionunitofwork.h`
- حذف `src/infrastructure/repository/itransactionrepository.h` (منتقل‌شده به Application)
- تغییر `src/application/transactionservice.h`/`.cpp`
- تغییر `src/application/depositservice.h`/`.cpp`
- تغییر `src/infrastructure/repository/transactionrepository.h`/`.cpp`
- تغییر `src/infrastructure/unitofwork.h`/`.cpp`
- تغییر `src/main.cpp`
- تغییر `CMakeLists.txt` فقط برای مسیر قراردادهای جدید و حذف مسیر قدیمی

انجام نشد: هیچ Schema، Migration، DDL/DML، داده، HTTP، SQL روی دیتابیس واقعی، Integration/Concurrency واقعی، Fault Injection واقعی، restart، نصب ابزار یا عملیات Git انجام نشد. Balance/Journal SQL و `QSqlDatabase` تزریق‌شده در Deposit به‌صورت انتقالی باقی ماندند.

#### تغییرات

- `ITransactionRepository` و `TransactionLookupResult` در `src/application/contracts/` قرار گرفتند؛ `TransactionService` فقط به این قرارداد Application وابسته است.
- `ITransactionUnitOfWork` و `ITransactionUnitOfWorkFactory` در Application تعریف شدند؛ `UnitOfWork` و `UnitOfWorkFactory` در Infrastructure پیاده‌سازی شدند.
- `UnitOfWork` اکنون `transactions()` را برمی‌گرداند و `database()` از API عمومی آن حذف شد. state machine و رفتار F1/F2 بدون تغییر منطقی حفظ شد.
- `DepositService` در هر فراخوانی یک UoW مستقل از factory می‌سازد؛ دیگر `UnitOfWork` یا `TransactionRepository` concrete نمی‌سازد. `TransactionService` با repository همان scope ساخته می‌شود.
- SQL `transaction_entry` با همان INSERT، همان پارامترها و همان جای ترتیب قبلی به `ITransactionRepository::appendDestinationEntry` منتقل شد.
- خطاهای Transaction/UoW با `OperationError` امن منتقل می‌شوند؛ کدهای API (`TRANSACTION_START_FAILED`، `DATABASE_COMMIT_FAILED`، `DATABASE_ROLLBACK_FAILED`، `TRANSACTION_ENTRY_CREATE_FAILED`، `IDEMPOTENCY_*`) حفظ شدند و متن خام SQL از این قراردادها حذف شد.
- `main.cpp` به‌عنوان Composition Root همان `QSqlDatabase` را به `UnitOfWorkFactory` و `DepositService` می‌دهد تا scope و SQLهای انتقالی Balance/Journal از یک Connection/تراکنش استفاده کنند.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق. CMake به‌دلیل تغییر فهرست فایل‌ها یک بار خودکار Reconfigure شد و سپس:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[ 12%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

#### آزمون Harness (بدون دیتابیس واقعی)

Harness قبلی F1/F2 با FakeDriver و UnitOfWork واقعی برای APIهای جدید سازگار شد و اجرا شد. نتیجه: `TOTAL FAILURES: 0` و `EXIT_CODE=0`.

| سناریو | نتیجه | begin | commit | rollback | close | openBefore | openAfter |
|---|---|---|---|---|---|---|---|
| A: commit ناموفق | PASS | 1 | 1 | 0 | 1 | true | false |
| B: rollback صریح ناموفق | PASS | 1 | 0 | 1 | 1 | true | false |
| C: rollback ناموفق در Destructor | PASS | 1 | 0 | 1 | 1 | true | false |
| D: BEGIN ناموفق | PASS | 1 | 0 | 0 | 1 | true | false |
| E1: commit موفق | PASS | 1 | 1 | 0 | 0 | true | true |
| E2: rollback صریح موفق | PASS | 1 | 0 | 1 | 0 | true | true |
| E3: rollback خودکار موفق | PASS | 1 | 0 | 1 | 0 | true | true |

آزمون QSQLITE برای shared-close نیز PASS بود: close روی یک کپی، `isOpen()` کپی دیگر را false کرد و Query روی آن شکست خورد.

#### حفظ رفتار

- API، fingerprint، canonicalization، replay، conflict و retry کلیدهای قدیمی تغییر نکردند.
- SQL و ترتیب Balance/Journal تغییر نکردند.
- commit ناموفق همچنان `CommitOutcomeUnknown` است و هیچ rollback خودکاری پس از آن انجام نمی‌شود.
- شکست rollback و fail-closed فعلی حفظ شدند.
- هیچ اتصال فیزیکی جدید، reconnect، pool یا recovery مرکزی اضافه نشد.

#### بدهی انتقالی و محدودیت‌ها

- `DepositService` هنوز `QSqlDatabase` تزریق‌شده و SQLهای Balance/Journal را تا 1C نگه می‌دارد؛ تکمیل معماری Deposit ادعا نمی‌شود.
- `Balances()`/`Journals()` یا stub مراحل بعد اضافه نشد.
- آزمون HTTP، SQL واقعی، Integration/Concurrency و Fault Injection واقعی اجرا نشد.
- پیام‌های خطای داخلی Transaction/UoW از متن خام دیتابیس به پیام ثابت/ایمن تغییر کردند؛ کدهای API حفظ شدند.
- `UnitOfWork` همچنان با Connection مشترک کار می‌کند؛ هم‌زمانی واقعی با اتصال‌های مستقل همچنان آزموده‌نشده است.

### 18.21 زیرمرحله 1B.1 — استخراج ساختاری Balance از Deposit — 2026-11-13

#### محدوده و فایل‌ها

- ایجاد `src/application/contracts/ibalancerepository.h`
- ایجاد `src/infrastructure/repository/balancerepository.h`
- ایجاد `src/infrastructure/repository/balancerepository.cpp`
- ایجاد `tests/balance_repository_unitofwork_test.cpp`
- تغییر `src/application/contracts/itransactionunitofwork.h`
- تغییر `src/infrastructure/unitofwork.h`/`.cpp`
- تغییر `src/application/depositservice.h`/`.cpp`
- تغییر `CMakeLists.txt` برای افزودن قرارداد و Repository جدید
- تغییر `docs/PROJECT_CONTEXT.md` و همین سند

انجام نشد: هیچ Schema، Migration، DDL/DML، تغییر داده، HTTP، SQL روی PostgreSQL واقعی، Integration/Concurrency واقعی، Fault Injection واقعی، restart، نصب ابزار یا عملیات Git انجام نشد. Journal، Transfer، HoldService و مسیرهای نوشتن Hold تغییر نکردند.

#### تغییرات

- `IBalanceRepository` با سه عملیات موردنیاز واقعی Deposit تعریف شد:
  - قفل ردیف `account_balance`
  - خواندن Snapshot قفل‌شده ورودی‌های محاسبه موجودی
  - اعمال افزایش واریز روی `account_balance`
- قرارداد از `OperationError` امن استفاده می‌کند و هیچ `QSqlDatabase`، `QSqlQuery`، SQL یا متن خام خطای دیتابیس را منتشر نمی‌کند.
- `BalanceRepository` در Infrastructure پیاده‌سازی شد. SQL، bindها، ترتیب Queryها، `FOR UPDATE`، `FOR UPDATE OF a, b`، `CURRENT_TIMESTAMP`/`CURRENT_DATE`، تبدیل `NUMERIC` به رشته و فرمول فعلی `available_balance` دقیقاً حفظ شدند.
- `balances()` به `ITransactionUnitOfWork` اضافه شد. `UnitOfWork` همان `QSqlDatabase` خود را به `BalanceRepository` می‌دهد؛ Connection یا تراکنش مستقل ساخته نشد.
- `DepositService` قفل Balance، خواندن ورودی‌های محاسبه و UPDATE موجودی را از `IBalanceRepository` انجام می‌دهد. منطق تصمیم‌گیری و اعتبارسنجی Use Case در Deposit باقی ماند.
- قفل Account، قفل Hold، SQLهای Journal/Ledger/TransactionEntry و `QSqlDatabase` انتقالی در Deposit باقی ماندند. ترتیب کلی Account Lock → Balance Lock → Hold Lock → Snapshot → ... حفظ شد.
- `available_balance` همچنان همان Cache فعلی است و جایگزین منبع محاسبه نشد.

#### تغییر پیام‌های خطا

کدهای API تغییر نکردند، اما متن خطای این سه مسیر Balance از `QSqlError::text()` خام به پیام امن ثابت تغییر کرد:

| کد API | متن قبلی | متن فعلی |
|---|---|---|
| `ACCOUNT_BALANCE_LOCK_FAILED` | متن خام درایور/دیتابیس | `Account balance row lock failed` |
| `ACCOUNT_LOCK_OR_READ_FAILED` | متن خام درایور/دیتابیس | `Account balance state could not be loaded` |
| `ACCOUNT_BALANCE_UPDATE_FAILED` | متن خام درایور/دیتابیس | `Account balance update failed` |

#### حفظ SQL

مقایسه خودکار رشته‌های SQL با نسخه پیش از تغییر انجام شد و نتیجه برای هر سه بخش `True` بود:

- Snapshot: طول ۲۴۴۷ کاراکتر، یکسان.
- قفل `account_balance`: طول ۸۰ کاراکتر، یکسان.
- `UPDATE account_balance`: طول ۲۹۶ کاراکتر، یکسان.

فایل مقایسه دائمی ایجاد نشد؛ این بررسی بخشی از شواهد همین مرحله است.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق. CMake به‌دلیل تغییر فهرست فایل‌ها یک بار خودکار Reconfigure شد:

```text
-- Configuring done
-- Generating done
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/repository/balancerepository.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[  6%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  8%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  9%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/depositcontroller.cpp.o
[ 10%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

#### آزمون هدفمند بدون PostgreSQL

```bash
g++ -std=c++17 -fPIC -I. tests/balance_repository_unitofwork_test.cpp \
  src/infrastructure/unitofwork.cpp \
  src/infrastructure/repository/balancerepository.cpp \
  src/infrastructure/repository/transactionrepository.cpp \
  src/domain/transaction.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/balance_repository_unitofwork_test

/tmp/balance_repository_unitofwork_test
```

خروجی واقعی:

```text
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
All balance repository and unit-of-work tests passed
```

دو هشدار `database not open` مربوط به آزمون عمدی اتصال بسته برای بررسی پیام امن است؛ assertionها موفق بودند.

پوشش آزمون:

- `UnitOfWorkFactory`/`UnitOfWork` واقعی با `QSQLITE` حافظه‌ای.
- `balances()` از همان scope و همان Connection استفاده می‌کند.
- اعمال افزایش واریز داخل تراکنش، خواندن مقدار، و بازگشت مقدار پس از `rollback`.
- اعمال دوم و `commit`، سپس خواندن مقدار پایدارشده.
- Account ناموجود: شکست امن `Account balance update failed` بدون نشت نام جدول یا SQL.
- اتصال بسته: قفل و Snapshot با پیام‌های امن ثابت شکست می‌خورند و متن خام `not open` نشت نمی‌کند.

این آزمون جایگزین آزمون PostgreSQL نیست. رفتار `FOR UPDATE`/`FOR UPDATE OF`، `NUMERIC` واقعی PostgreSQL، قواعد مؤثر Override/Product و هم‌زمانی واقعی آزموده نشدند.

#### داده‌های آزمایشی

هیچ داده‌ای در دیتابیس `corebanking` ایجاد یا تغییر نکرد. آزمون فقط از دیتابیس حافظه‌ای `QSQLITE` استفاده کرد و هیچ شناسه کسب‌وکاری تولید نشد.

#### بدهی و محدودیت‌های باقی‌مانده

- Journal، Ledger Balance، TransactionEntry و `QSqlDatabase` انتقالی هنوز در `DepositService` هستند.
- شکاف هم‌زمانی قواعد مؤثر و نبود Override حل نشده و برای Balance Slice باز است.
- زمان مرجع و روش محاسبه دقیق Domain، انتخاب `CURRENT_TIMESTAMP`/`CURRENT_DATE` و معنای `available_balance` تغییر نکرده و تکمیل نشده‌اند.
- `available_balance` همچنان Cache کهنه‌شونده است.
- HoldService و مسیرهای نوشتن Hold تغییر نکردند.
- آزمون HTTP، PostgreSQL واقعی، Integration/Concurrency واقعی و Fault Injection واقعی اجرا نشد.
- این مرحله تکمیل 1B، رفع race یا تکمیل معماری Deposit نیست.

### 18.22 زیرمرحله 1C.1 — استخراج ساختاری Journal/Ledger از Deposit — 2026-11-13

#### محدوده و فایل‌ها

- ایجاد `src/application/contracts/ijournalrepository.h`
- حذف `src/infrastructure/repository/ijournalrepository.h` (قرارداد به Application منتقل شد)
- تغییر `src/infrastructure/repository/journalrepository.h`/`.cpp` (پیاده‌سازی قرارداد جدید Application)
- ایجاد `tests/journal_repository_unitofwork_test.cpp`
- تغییر `src/application/contracts/itransactionunitofwork.h`
- تغییر `src/infrastructure/unitofwork.h`/`.cpp`
- تغییر `src/application/depositservice.h`/`.cpp`
- تغییر `CMakeLists.txt` برای مسیر قرارداد جدید و حذف مسیر قدیمی
- تغییر `docs/PROJECT_CONTEXT.md` و همین سند

انجام نشد: هیچ Schema، Migration، DDL/DML، تغییر داده، HTTP، SQL روی PostgreSQL واقعی، Integration/Concurrency واقعی، Fault Injection واقعی، restart، نصب ابزار یا عملیات Git انجام نشد. Balance، Transfer، HoldService و مسیرهای نوشتن Hold تغییر نکردند.

#### تغییرات

- `IJournalRepository` در Application با عملیات واقعی Deposit تعریف شد:
  - lookup سرفصل تسویه با `FOR SHARE`
  - مقداردهی اولیه `ledger_account_balance`
  - قفل ردیف‌های `ledger_account_balance`
  - ایجاد Journal پیش‌نویس
  - درج دو سطر Journal واریز
  - خواندن تعداد و توازن سطرها
  - به‌روزرسانی تجمعی Ledger
  - posting مشروط Journal
- قرارداد از `OperationError` امن استفاده می‌کند و هیچ `QSqlDatabase`، `QSqlQuery`، SQL یا متن خام خطای دیتابیس را منتشر نمی‌کند.
- `JournalRepository` موجود Infrastructure برای پیاده‌سازی قرارداد جدید استفاده شد. متدهای CRUD عمومی قبلی آن هیچ مصرف‌کننده‌ای نداشتند و با عملیات واقعی Deposit جایگزین شدند؛ رفتار آن‌ها در مسیر فعال پروژه استفاده نمی‌شد.
- `journals()` به `ITransactionUnitOfWork` اضافه شد. `UnitOfWork` همان `QSqlDatabase` خود را به `JournalRepository` می‌دهد؛ Connection یا تراکنش مستقل ساخته نشد.
- `DepositService` Journal/Ledger SQL را از `IJournalRepository` انجام می‌دهد. تصمیم‌ها و اعتبارسنجی‌های Use Case در Deposit باقی ماندند.
- ترتیب کلی حفظ شد: Transaction create → Account/Balance/Hold locks → Snapshot → Settlement lookup → Ledger init/lock → TransactionEntry → Journal create/entries/check → Balance update → Ledger update → Journal post → Transaction complete.
- `QSqlDatabase` انتقالی در Deposit حذف نشد، چون سه SQL دیگر باقی‌اند: Account currency lookup، Account row lock و Hold row lock.

#### SQLهای باقی‌مانده در Deposit

- `SELECT currency_id FROM account WHERE id = :id`
- `SELECT id FROM account WHERE id = :account_id FOR UPDATE`
- `SELECT id FROM account_hold WHERE account_id = :account_id ORDER BY id FOR UPDATE`

این SQLها عمداً پنهان یا به Repository نامرتبط منتقل نشدند.

#### حفظ SQL و bindها

مقایسه خودکار SQL و ترتیب bindها با نسخه پیش از تغییر انجام شد. نتیجه برای هر هشت بخش `sql=True` و `bind_order=True` بود:

| بخش | طول SQL | bindها |
|---|---|---|
| Settlement ledger lookup | ۱۶۹ | `:code`, `:currency_id` |
| Ledger balance init | ۳۶۱ | `:customer_ledger`, `:settlement_ledger` |
| Ledger balance lock | ۱۹۰ | `:customer_ledger`, `:settlement_ledger` |
| Journal create | ۱۷۴ | `:transaction_id`, `:description` |
| Journal entries | ۳۵۲ | `:journal_id`, `:settlement_ledger`, `:customer_ledger`, `:currency_id`, `:debit_amount`, `:credit_amount`, `:account_id`, `:cash_description`, `:customer_description` |
| Journal balance read | ۱۴۵ | `:journal_id` |
| Ledger update | ۹۷۷ | `:settlement_ledger`, `:customer_ledger`, `:cash_debit`, `:customer_credit` |
| Journal post | ۴۰۲ | `:journal_id` |

فایل مقایسه دائمی ایجاد نشد؛ این بررسی بخشی از شواهد همین مرحله است.

#### تغییر پیام‌های خطا

کدهای API و شاخه‌های کنترل حفظ شدند. متن خام جایگزین‌شده:

| کد API | متن قبلی | متن فعلی |
|---|---|---|
| `SETTLEMENT_LEDGER_LOOKUP_FAILED` | متن خام `QSqlError::text()` | `Settlement ledger lookup failed` |
| `LEDGER_BALANCE_INIT_FAILED` | متن خام `QSqlError::text()` | `Ledger balance initialization failed` |
| `LEDGER_BALANCE_LOCK_FAILED` | متن خام `QSqlError::text()` | `Ledger balance rows could not be locked` |
| `LEDGER_BALANCE_LOCK_FAILED` (تعداد ناکافی) | `Could not lock all ledger balance rows` | بدون تغییر |
| `JOURNAL_CREATE_FAILED` | متن خام `QSqlError::text()` | `Journal could not be created` |
| `JOURNAL_ENTRY_CREATE_FAILED` | متن خام `QSqlError::text()` | `Journal entries could not be created` |
| `JOURNAL_BALANCE_CHECK_FAILED` | متن خام `QSqlError::text()` | `Journal balance check failed` |
| `JOURNAL_NOT_BALANCED` | `Journal debit and credit totals do not match` | بدون تغییر |
| `LEDGER_BALANCE_PREPARE_FAILED` | متن خام `QSqlError::text()` | `Ledger balance update could not be prepared` |
| `LEDGER_BALANCE_UPDATE_FAILED` | متن خام `QSqlError::text()` | `Ledger balance update failed` |
| `JOURNAL_POST_FAILED` (خطای اجرا) | متن خام `QSqlError::text()` | `Journal could not be posted` |
| `JOURNAL_POST_FAILED` (رد شدن) | متن خام یا fallback | `Journal is unbalanced or could not be posted` |

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق.

```text
-- Configuring done
-- Generating done
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/repository/journalrepository.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[  6%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  8%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  9%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/depositcontroller.cpp.o
[ 10%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

CMake به‌دلیل تغییر فهرست فایل‌ها یک بار خودکار Reconfigure شد. دستور `cmake -S` جداگانه اجرا نشد.

#### آزمون هدفمند بدون PostgreSQL

```bash
g++ -std=c++17 -fPIC -I. tests/journal_repository_unitofwork_test.cpp \
  src/infrastructure/unitofwork.cpp \
  src/infrastructure/repository/journalrepository.cpp \
  src/infrastructure/repository/balancerepository.cpp \
  src/infrastructure/repository/transactionrepository.cpp \
  src/domain/transaction.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/journal_repository_unitofwork_test

/tmp/journal_repository_unitofwork_test
```

خروجی واقعی:

```text
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
All journal repository and unit-of-work tests passed
```

هشدارهای `database not open` مربوط به آزمون عمدی اتصال بسته برای بررسی پیام امن هستند؛ assertionها موفق بودند.

#### پوشش و محدودیت آزمون

- `UnitOfWorkFactory`/`UnitOfWork` واقعی با `QSQLITE` حافظه‌ای.
- `journals()` از همان scope و Connection است و از `transactions()`/`balances()` جداست.
- ایجاد Journal پیش‌نویس، درج دو سطر، خواندن تعداد/توازن و rollback بدون باقی‌ماندن ردیف.
- ایجاد و commit دومین Journal و دو سطر آن.
- اتصال بسته و بررسی پیام‌های امن برای lookup، init، lock، create، entries، balance read، ledger update و post.
- موفقیت `postIfBalanced` و موفقیت `ensureLedgerBalanceRows`/`applyLedgerDeltas` روی `QSQLITE` اجرا نشد، چون SQL آن‌ها PostgreSQL-specific است (`UPDATE ... alias`، `unnest(ARRAY[...])`، CTE/ON CONFLICT). فقط مسیرهای خطای امن آن‌ها آزموده شد.
- آزمون PostgreSQL واقعی، HTTP، Integration/Concurrency واقعی و Fault Injection واقعی اجرا نشد. `QSQLITE` اثبات سازگاری PostgreSQL نیست.

#### داده‌ها

هیچ داده‌ای در دیتابیس `corebanking` ایجاد یا تغییر نکرد و هیچ شناسه کسب‌وکاری تولید نشد. آزمون فقط از دیتابیس حافظه‌ای `QSQLITE` استفاده کرد.

#### بدهی و محدودیت‌های باقی‌مانده

- Account currency lookup، Account row lock و Hold row lock همچنان در `DepositService` و روی `QSqlDatabase` انتقالی هستند.
- Journal invariants جدید، بازطراحی قواعد Journal و رفع race قواعد مؤثر Balance انجام نشد.
- زمان مرجع، isolation، advisory locks و روش محاسبات پولی تغییر نکردند.
- Transfer، HoldService و مسیرهای نوشتن Hold تغییر نکردند.
- آزمون PostgreSQL واقعی، HTTP، Integration/Concurrency واقعی و Fault Injection واقعی اجرا نشد.
- این مرحله تکمیل کل 1B/1C، رفع race یا آمادگی عملیاتی بانکی نیست.

### 18.23 زیرمرحله 1C.2 — بسته نهایی استخراج persistence از Deposit — 2026-11-13

#### محدوده و فایل‌ها

- ایجاد `src/application/contracts/iaccountstaterepository.h`
- ایجاد `src/application/contracts/iholdrepository.h`
- ایجاد `src/infrastructure/repository/accountstaterepository.h`/`.cpp`
- ایجاد `src/infrastructure/repository/holdrepository.h`/`.cpp`
- ایجاد `tests/deposit_orchestration_test.cpp`
- ایجاد `tests/account_hold_persistence_test.cpp`
- تغییر `src/application/contracts/itransactionunitofwork.h`
- تغییر `src/infrastructure/unitofwork.h`/`.cpp`
- تغییر `src/application/depositservice.h`/`.cpp`
- تغییر `src/main.cpp` برای wiring جدید Composition Root
- تغییر `CMakeLists.txt` برای قراردادها و Repositoryهای جدید
- تغییر `docs/PROJECT_CONTEXT.md` و همین سند

انجام نشد: هیچ Schema، Migration، DDL/DML، تغییر داده، HTTP، SQL روی PostgreSQL واقعی، Integration/Concurrency واقعی، Fault Injection واقعی، restart، نصب ابزار یا عملیات Git انجام نشد.

#### تغییرات

- Account currency lookup و Account row lock در قرارداد `IAccountStateRepository` Application و پیاده‌سازی `AccountStateRepository` Infrastructure قرار گرفتند.
- Hold row lock در قرارداد `IHoldRepository` Application و پیاده‌سازی `HoldRepository` Infrastructure قرار گرفت.
- `accounts()` و `holds()` به `ITransactionUnitOfWork` اضافه شدند. `UnitOfWork` هر دو Repository را با همان `QSqlDatabase` و همان scope می‌سازد؛ Connection یا transaction مستقل اضافه نشد.
- `DepositService` تمام persistence را از قراردادهای Application و UoW انجام می‌دهد. SQL اجرایی، `QSqlDatabase`، `QSqlQuery`، `QSqlError` و include مستقیم Infrastructure از آن حذف شدند.
- برای حفظ `DATABASE_UNAVAILABLE` و ترتیب کنترل قبلی، `ITransactionUnitOfWorkFactory::isConnectionAvailable()` اضافه شد. این متد فقط وضعیت Connection مشترک را گزارش می‌کند و در Deposit وابستگی Qt SQL ایجاد نمی‌کند.
- `main.cpp` فقط factory و کد سرفصل تسویه را به `DepositService` می‌دهد؛ `QSqlDatabase` دیگر به constructor آن پاس نمی‌شود.
- ترتیب کامل Account/Balance/Hold lock و تمام مراحل Transaction/Balance/Journal حفظ شد. طول عمر قفل‌ها تا پایان همان UoW/تراکنش تغییر نکرد.
- HoldService و مسیرهای نویسنده Hold تغییر نکردند.

#### حفظ SQL و bindها

سه SQL منتقل‌شده با نسخه قبل مقایسه شدند:

| بخش | SQL | bind | طول |
|---|---|---|---|
| Account currency lookup | یکسان | `:id` | ۴۶ |
| Account row lock | یکسان | `:account_id` | ۵۶ |
| Hold row lock | یکسان | `:account_id` | ۸۱ |

ترتیب فراخوانی و ترتیب قفل‌ها نیز در آزمون orchestration بررسی شد: Account lock → Balance lock → Hold lock و سپس Snapshot و ادامه Journal/Balance.

#### تغییر پیام‌های خطا

کدهای API و شاخه‌های کنترل حفظ شدند. متن خام جایگزین‌شده:

| کد API | متن قبلی | متن فعلی |
|---|---|---|
| `ACCOUNT_LOOKUP_FAILED` | متن خام `QSqlError::text()` | `Account currency lookup failed` |
| `ACCOUNT_LOCK_FAILED` | متن خام `QSqlError::text()` | `Account row lock failed` |
| `HOLD_LOCK_FAILED` | متن خام `QSqlError::text()` | `Account hold rows could not be locked` |

وضعیت «ردیف پیدا نشد» در Account currency lookup با `Found`/`NotFound`/`Failed` از خطای persistence جدا ماند و مسیر `ACCOUNT_NOT_FOUND` بدون تغییر حفظ شد.

#### Build

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

#### آزمون orchestration Deposit بدون Qt SQL

این آزمون فقط با Qt5Core کامپایل شد و هیچ فایل Infrastructure یا Qt5Sql را لینک نکرد؛ بنابراین fakeها و خود `DepositService` به اتصال Qt SQL نیاز ندارند.

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

پوشش:

- موفقیت کامل و ترتیب حساس فراخوانی‌ها از `uow.begin` تا `uow.commit`
- استفاده از `transactions()`، `accounts()`، `balances()`، `holds()` و `journals()` همان UoW
- Replay تراکنش Completed و عدم ورود به Account/Balance/Journal
- Account not found و rollback بدون ایجاد Transaction
- خطای persistence امن در Account currency lookup
- شکست Hold lock و توقف پیش از Journal
- شکست Journal post پس از نوشتن Balance و rollback پیش از commit
- `DATABASE_UNAVAILABLE` و توقف پیش از ساخت UoW

#### آزمون persistence Account/Hold

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

پوشش:

- خواندن موفق `currency_id` از `account` با `QSQLITE`
- تشخیص `NotFound` برای Account ناموجود، جدا از خطای persistence
- خطای امن برای currency lookup، account row lock و hold lock روی Connection بسته
- عدم نشت متن خام `not open`

محدودیت: موفقیت `FOR UPDATE` روی `QSQLITE` آزموده نشد؛ این آزمون جایگزین PostgreSQL نیست.

#### بررسی نهایی وابستگی‌های Deposit

- جست‌وجوی `QSqlDatabase`، `QSqlQuery`، `QSqlError`، `lastError` و `infrastructure/repository` در `depositservice.h`/`.cpp`: بدون نتیجه.
- includeهای Deposit فقط شامل قراردادهای Application، `transactionservice.h` و Qt Core هستند.
- وابستگی Qt Core (`QDateTime`، `QCryptographicHash`، `QRegularExpression`، `QUuid`، `QDebug`) خارج از دامنه این مرحله باقی ماند.
- `DepositService` هیچ تصمیم مالی جدیدی دریافت نکرد و فقط محل اجرای SQL تغییر کرد.

#### موارد باز که حل نشده‌اند

- هم‌زمانی قواعد مؤثر Balance، همه نویسندگان آن‌ها و شکاف نبود Override باز است.
- زمان مرجع پس از قفل‌ها و انتخاب `CURRENT_TIMESTAMP`/`CURRENT_DATE` تعیین تکلیف نشده است.
- محاسبات دقیق monetary و قواعد Domain تکمیل نشده‌اند.
- آزمون PostgreSQL واقعی، رفتار commit نامعلوم در QPSQL، Integration/Concurrency و Fault Injection واقعی اجرا نشد.
- محدودیت Connection مشترک و fail-closed فعلی باقی است.
- این مرحله تکمیل 1B/1C، رفع race یا آمادگی عملیاتی بانکی نیست.

## 19. قاعده پایانی تاریخچه

History شاهد اتفاقات گذشته است. برای هر نتیجه، محیط و محدوده آزمون مشخص شود. اگر کاربر وضعیت جدیدی از تست تأیید کرد، وضعیت خلاصه باید به‌روز شود، ولی نباید جزئیات نامشخص مانند نام دیتابیس، پاسخ HTTP یا خروجی SQL حدس زده شود.

