# Hold API Amount Precision Fix

> تاریخ: 2026-11-02
>
> وضعیت: **ورودی عددی مبلغ حذف و اعتبارسنجی رشته‌ای سخت‌گیرانه شد؛ Build موفق؛ آزمون‌های HTTP موفق.**

## 1. مشکل

`amountToText` در `HoldController` ورودی JSON عددی را با `QString::number(value, 'f', 8)` به رشته تبدیل می‌کرد. این کار می‌توانست ورودی با دقت بیشتر از ۸ رقم اعشار را بی‌صدا گرد کند.

## 2. اصلاح

- `amountToText` حذف شد.
- `parseAmountString` اضافه شد که فقط JSON string را می‌پذیرد.
- رشته خالی، فاصله ابتدا/انتها و ورودی عددی JSON با `400 INVALID_AMOUNT` رد می‌شوند.
- اعتبارسنجی تفصیلی مقدار (مثبت بودن، حداکثر ۸ رقم اعشار، رد قالب علمی، جداکننده نامعتبر، صفر و منفی) در `HoldService` انجام می‌شود و با `400 HOLD_INVALID_AMOUNT` برمی‌گردد.
- `HoldService` مبلغ معتبر را قبل از ساخت Fingerprint نرمال می‌کند (حذف صفرهای انتهایی)؛ بنابراین `"0.10"` و `"0.1"` یک مقدار canonical دارند.
- ستون‌های `account_hold.amount` و `account_hold_events.amount` در Schema فعلی `numeric` بدون precision/scale ثابت هستند. محدودیت ۸ رقم اعشار همان قاعده موجود سرویس است و فرض جدیدی درباره دقت ارز ایجاد نشد.
- هیچ Migration، تغییر Schema، DDL/DML مستقیم یا تغییر در سرویس‌های دیگر انجام نشد.

## 3. فایل‌های تغییرکرده

- `src/web/holdcontroller.cpp`
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`
- `docs/opencode-reports/HOLD_CONTROLLER_DESIGN.md`
- `docs/opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md`
- `docs/opencode-reports/HOLD_AMOUNT_VALIDATION_FIX.md`

## 4. Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

## 5. نتایج آزمون HTTP

| ورودی | نتیجه |
|---|---|
| `"0.12"` | `201`، ذخیره دقیق `0.12` |
| `"0.12345678"` | `201`، ذخیره دقیق `0.12345678` |
| `"0.123456789"` | `400 HOLD_INVALID_AMOUNT` |
| عدد JSON `0.1` | `400 INVALID_AMOUNT` |
| `"0"` | `400 HOLD_INVALID_AMOUNT` |
| `"-0.1"` | `400 HOLD_INVALID_AMOUNT` |
| `""` | `400 INVALID_AMOUNT` |
| `" 0.1"` / `"0.1 "` / `"   "` | `400 INVALID_AMOUNT` |
| `"1e-3"` | `400 HOLD_INVALID_AMOUNT` |
| `"1,000.00"` | `400 HOLD_INVALID_AMOUNT` |
| `"+0.1"` | `400 HOLD_INVALID_AMOUNT` |
| `"01.0"` | `400 HOLD_INVALID_AMOUNT` |
| Event با `amount` عددی | `400 INVALID_AMOUNT` |
| Event با `"0.123456789"` | `400 HOLD_INVALID_AMOUNT` |

### Idempotency canonical

- ایجاد Hold با `"0.10"` و کلید `amount-canonical-20261102` → `201`، `holdId=6`
- Retry همان کلید با `"0.1"` → `200`، همان `holdId=6`، `replayed=true`
- مقدار ذخیره‌شده: `0.1`

## 6. داده‌های آزمایشی

- Hold `4`: مبلغ `0.12`، کلید `amount-valid-2dp-20261102`، وضعیت نهایی `RELEASED`
- Hold `5`: مبلغ `0.12345678`، کلید `amount-valid-8dp-20261102`، وضعیت نهایی `RELEASED`
- Hold `6`: مبلغ `0.1`، کلید `amount-canonical-20261102`، وضعیت نهایی `RELEASED`
- Eventهای `5`، `6` و `7` برای آزادسازی کامل

`ledger_balance` حساب `1` قبل و بعد: `1`، بدون تغییر.

## 7. محدودیت‌ها

- آزمون هم‌زمانی انجام نشد.
- آزمون‌های این مرحله از API محلی `http://localhost:8080` انجام شدند و هیچ نوشتن مستقیم در دیتابیس انجام نشد.
- هیچ دستور Git اجرا نشد.
