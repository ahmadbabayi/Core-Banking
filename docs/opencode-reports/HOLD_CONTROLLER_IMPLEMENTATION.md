# HoldController — Implementation Report

> تاریخ: 2026-11-02
>
> وضعیت: **HoldController پیاده‌سازی و به Router متصل شد؛ Build موفق؛ آزمون پایه API محلی موفق؛ آزمون هم‌زمانی/بار انجام نشد.**
>
> مبنا: `docs/opencode-reports/HOLD_CONTROLLER_DESIGN.md` و `src/application/holdservice.*`

## 1. خلاصه

پنج Endpoint Hold پیاده‌سازی و در HTTP Router متصل شدند:

- `POST /api/v1/holds`
- `GET /api/v1/holds/{id}`
- `GET /api/v1/holds?accountId={id}`
- `POST /api/v1/holds/{id}/events`
- `GET /api/v1/holds/{id}/events`

Controller فقط اعتبارسنجی ورودی HTTP، ساخت پاسخ JSON و نگاشت خطا به HTTP را انجام می‌دهد. منطق کسب‌وکار، Idempotency، قفل‌ها، تراکنش و Triggerها در `HoldService` باقی مانده‌اند. هیچ دسترسی مستقیم به دیتابیس در Controller وجود ندارد.

## 2. فایل‌های تغییرکرده

| فایل | عملیات |
|---|---|
| `src/web/holdcontroller.h` | ایجاد |
| `src/web/holdcontroller.cpp` | ایجاد |
| `src/web/httprouter.h` | افزودن include، pointer و setter |
| `src/web/httprouter.cpp` | افزودن مسیرهای Hold |
| `src/main.cpp` | ساخت `HoldService`/`HoldController` و اتصال به Router |
| `CMakeLists.txt` | افزودن فایل‌های Controller |
| `docs/PROJECT_CONTEXT.md` | به‌روزرسانی وضعیت و قرارداد Hold API |
| `docs/PROJECT_HISTORY.md` | ثبت Build، آزمون‌ها و داده‌های آزمایشی |
| `docs/opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md` | همین گزارش |

`DepositService`، `TransferService`، `HoldService`، Migrationها و Schema تغییر نکردند. هیچ عملیات Git انجام نشد.

## 3. قرارداد Endpointها

### 3.1 `POST /api/v1/holds`

فیلدهای الزامی: `accountId` (عدد صحیح مثبت)، `amount` (JSON string)، `reasonCode`، `idempotencyKey`. ورودی عددی JSON برای مبلغ رد می‌شود و مقدار باید حداکثر ۸ رقم اعشار داشته باشد.

فیلدهای اختیاری: `reasonDescription`، `referenceType`، `referenceId`، `effectiveAt`، `expiresAt`.

پاسخ موفق از `HoldResult.snapshot` ساخته می‌شود:

- ایجاد جدید: `201 Created`
- Retry دقیق: `200 OK`
- بدنه شامل Snapshot Hold، `replayed`، `code` و `message` است.

### 3.2 `GET /api/v1/holds/{id}`

- موفق: `200 OK` با Snapshot Hold.
- ناموجود: `404 HOLD_NOT_FOUND`.
- شناسه نامعتبر: `400 INVALID_HOLD_ID`.

### 3.3 `GET /api/v1/holds?accountId={id}`

- `accountId` اجباری است.
- موفق: `200 OK` با `accountId`، آرایه `holds` و `count`.
- نبود `accountId`: `400 MISSING_ACCOUNT_ID`.
- مقدار نامعتبر: `400 HOLD_INVALID_ACCOUNT_ID`.
- حساب بدون Hold: `200` با لیست خالی.

### 3.4 `POST /api/v1/holds/{id}/events`

فیلدهای الزامی: `eventType`، `reasonCode`، `idempotencyKey`.

فیلدهای اختیاری: `amount` (برای `PARTIAL_RELEASE`)، `effectiveAt`، `referenceType`، `referenceId`.

پاسخ موفق از `HoldEventResult.snapshot` و فیلدهای واقعی Event شامل `eventId`، `holdId`، `eventType`، `amount`، `effectiveAt` و `reasonCode` ساخته می‌شود:

- ایجاد جدید: `201 Created`
- Retry دقیق: `200 OK`
- بدنه شامل `eventId`، `holdId`، `eventType`، `amount`، `effectiveAt`، `reasonCode`، وضعیت جاری Hold، `replayed`، `code` و `message` است.

### 3.5 `GET /api/v1/holds/{id}/events`

- موفق: `200 OK` با `holdId`، آرایه `events` و `count`.
- Hold ناموجود: `404 HOLD_NOT_FOUND`.
- شناسه نامعتبر: `400 INVALID_HOLD_ID`.

## 4. نگاشت خطاها

| نوع خطا | HTTP |
|---|---|
| JSON نامعتبر، فیلد الزامی نامعتبر، شناسه نامعتبر، خطاهای اعتبارسنجی ورودی/سرویس | `400 Bad Request` |
| Hold یا Account ناموجود | `404 Not Found` |
| استفاده مجدد از Idempotency Key با Payload متفاوت | `409 Conflict` |
| رد کسب‌وکاری Hold/Trigger، موجودی ناکافی، وضعیت حساب/محصول، قواعد زمانی/مبلغی | `422 Unprocessable Entity` |
| خطای دیتابیس، قفل، Commit یا نبود Snapshot | `500 Internal Server Error` |
| نبود HoldController در Router | `503 HOLD_SERVICE_UNAVAILABLE` |

نگاشت با کدهای `HoldResult`/`HoldEventResult`/`HoldQueryResult`/`HoldListResult`/`HoldEventsResult` انجام می‌شود و منطق Trigger تکرار نشده است.

## 5. Snapshot و Idempotency

- پاسخ‌های `POST` فقط از `snapshot` سرویس استفاده می‌کنند.
- وجود Snapshot با `hasSnapshot` بررسی می‌شود؛ نبود آن `500 HOLD_STATE_READ_FAILED` می‌دهد.
- Controller پس از Commit هیچ Query جدیدی اجرا نمی‌کند.
- `replayed == false` → `201`؛ `replayed == true` → `200`.
- تعارض کلید با Payload متفاوت از سرویس به `409` نگاشت می‌شود.

## 6. Build

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

در تلاش اول یک خطای نوع `QString` برای `HttpResponse::status` رخ داد و اصلاح شد؛ Build نهایی موفق بود.

## 7. آزمون‌های انجام‌شده

سرور از Build فعلی روی `http://localhost:8080` اجرا شد و پس از آزمون متوقف شد. تمام نوشتن‌ها از API انجام شدند.

| سناریو | نتیجه |
|---|---|
| ایجاد Hold جدید | `201`، `holdId=1`، Snapshot کامل |
| Retry همان Hold | `200`، همان `holdId`، `replayed=true` |
| همان کلید با Payload متفاوت | `409 IDEMPOTENCY_KEY_REUSED` |
| دریافت Hold موجود / ناموجود | `200` / `404 HOLD_NOT_FOUND` |
| فهرست Holdهای حساب | `200`، `count=1` |
| نبود/نامعتبر بودن `accountId` در فهرست | `400 MISSING_ACCOUNT_ID` / `400 HOLD_INVALID_ACCOUNT_ID` |
| شناسه نامعتبر در مسیر | `400 INVALID_HOLD_ID` |
| PARTIAL_RELEASE معتبر | `201`، `eventId=1`، remaining=`0.06`، `PARTIALLY_RELEASED` |
| Retry همان Event | `200`، همان `eventId`، `replayed=true` |
| FULL_RELEASE | `201`، `eventId=2`، remaining=`0`، `RELEASED` |
| Retry FULL_RELEASE | `200`، همان `eventId`، `replayed=true` |
| همان کلید Event با Payload متفاوت | `409 IDEMPOTENCY_KEY_REUSED` |
| تاریخچه Eventها | `200`، `count=2` |
| Hold ناموجود برای Event | `404 HOLD_NOT_FOUND` |
| JSON نامعتبر / Event Type نامعتبر | `400 INVALID_JSON` / `400 HOLD_INVALID_EVENT_TYPE` |
| PARTIAL برابر remaining | `422 PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT` |
| Event با زمان آینده | `422 ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED` |
| موجودی ناکافی | `422 INSUFFICIENT_AVAILABLE_FUNDS` |
| حساب ناموجود | `404 ACCOUNT_NOT_FOUND` |

### 7.1 داده‌های آزمایشی

- Hold `1`: کلید `stage3-hold-001-20261102`، مبلغ `0.10`، وضعیت نهایی `RELEASED`
- Event `1`: `PARTIAL_RELEASE` مبلغ `0.04` روی Hold `1`
- Event `2`: `FULL_RELEASE` مبلغ `0.06` روی Hold `1`
- Hold `2`: کلید `stage3-hold-002-20261102`، مبلغ `0.05`، وضعیت نهایی `RELEASED`
- Event `3`: `FULL_RELEASE` مبلغ `0.05` روی Hold `2`

بررسی read-only با `corebanking_reader`:

- `account_balance` حساب `1` قبل و بعد: `ledger_balance=1`، `available_balance=1`
- `account_hold_status`: هر دو Hold با `remaining_amount=0`، `currently_blocked_amount=0`، `status=RELEASED`
- `account_hold_events`: سه رویداد مطابق بالا

## 8. آزمون‌های انجام‌نشده

- آزمون هم‌زمانی و بار برای Hold/Event.
- Expiry Release واقعی پس از رسیدن `expires_at`.
- CANCELLATION_RELEASE و پوشش کامل همه حالت‌های Trigger.
- آزمون Rollback در نقاط شکست دیتابیس.
- بررسی هم‌زمانی Hold با Deposit/Transfer.

## 9. محدودیت و انحراف

این محدودیت در مرحله بعد و در `docs/opencode-reports/HOLD_EVENT_RESULT_FIX.md` رفع شد: `HoldEventResult` اکنون فیلدهای واقعی Event را از دیتابیس داخل همان تراکنش برمی‌گرداند و Controller از ورودی درخواست برای ساخت پاسخ استفاده نمی‌کند.

## 10. وضعیت Git

هیچ Stage، Commit یا Push انجام نشد. فایل‌های جدید و تغییرات موجود برای بررسی توسط کاربر باقی مانده‌اند.
