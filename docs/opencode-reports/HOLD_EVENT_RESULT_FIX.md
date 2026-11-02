# HoldEventResult Event Data Fix

> تاریخ: 2026-11-02
>
> وضعیت: **اصلاح شد؛ Build موفق؛ آزمون ایجاد Event بدون `effectiveAt` و Retry موفق.**

## 1. خلاصه

`HoldEventResult` توسعه یافت تا اطلاعات واقعی Event ثبت‌شده را از دیتابیس برگرداند:

- `eventId`
- `holdId`
- `eventType`
- `amount`
- `effectiveAt`
- `reasonCode`

این مقادیر با یک Loader جدید از `account_hold_events` و با `eventId` خوانده می‌شوند. Loader در همه مسیرهای موفق `createEvent`، شامل ایجاد جدید، Trigger RETURN NULL، بازیابی Unique Conflict و Idempotency Retry، داخل همان تراکنش و پیش از Commit اجرا می‌شود. هیچ Query پس از Commit اضافه نشد.

## 2. فایل‌های تغییرکرده

- `src/application/holdservice.h`
- `src/application/holdservice.cpp`
- `src/web/holdcontroller.h`
- `src/web/holdcontroller.cpp`
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`
- `docs/opencode-reports/HOLD_EVENT_RESULT_FIX.md`

## 3. تغییرات کلیدی

- افزودن `holdId`، `eventType`، `amount`، `effectiveAt` و `reasonCode` به `HoldEventResult`.
- افزودن `loadHoldEventData` برای خواندن رکورد واقعی Event و `applyHoldEventData` برای انتقال آن به Result.
- فراخوانی Loader در مسیر Idempotency Retry پیش از Commit.
- فراخوانی Loader در مسیر مشترک ایجاد/بازیابی Event پیش از Commit.
- حذف استفاده Controller از `eventType`، `amount`، `effectiveAt` و `reasonCode` ورودی برای ساخت پاسخ Create.
- پاسخ Event اکنون `holdId` واقعی را از رکورد Event می‌گیرد.

## 4. Build

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

## 5. آزمون

- Hold `3` از API ایجاد شد: مبلغ `0.01`، کلید `stage4-hold-001-20261102`.
- Event بدون `effectiveAt` ایجاد شد: `FULL_RELEASE`، کلید `stage4-evt-full-001`.
- پاسخ ایجاد: `201`، `eventId=4`، `amount="0.01"`، `effectiveAt="2026-11-02T18:41:31.810Z"`، `holdId=3`، `holdStatus="RELEASED"`.
- Retry همان درخواست: `200`، همان `eventId=4`، همان `amount` و همان `effectiveAt`، `replayed=true`.
- بررسی read-only دیتابیس:

```text
id | hold_id | event_type   | amount | effective_at                  | reason_code
4  | 3       | FULL_RELEASE | 0.01   | 2026-11-02 22:11:31.81+03:30 | API_TEST_EVENT_DEFAULT_TIME
```

زمان پاسخ `18:41:31.810Z` با مقدار ذخیره‌شده در دیتابیس منطبق است. بنابراین هر دو پاسخ Create و Retry زمان واقعی ذخیره‌شده را برمی‌گردانند.

- `account_balance` حساب `1` پس از آزمون: `ledger_balance=1`، `available_balance=1`.

## 6. محدودیت‌ها

- آزمون هم‌زمانی و مسیرهای Expiry/Cancellation در این اصلاح انجام نشد.
- هیچ تغییر Schema، Migration، منطق کسب‌وکار یا سرویس دیگر انجام نشد.
- هیچ دستور Git اجرا نشد.
