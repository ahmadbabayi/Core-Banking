# اصلاح Idempotency مربوط به `effectiveAt` در رویدادهای Hold — 2026-11-03

## 1. محدوده

بررسی و اصلاح رفتار Idempotency رویداد Hold در `HoldService::createEvent` و `lookupEventByKey`.

فایل‌های تغییرکرده:

- `src/application/holdservice.cpp`
- `src/web/holdcontroller.cpp`
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`
- `docs/opencode-reports/HOLD_EVENT_EFFECTIVE_AT_IDEMPOTENCY_FIX.md` (همین فایل)

هیچ Migration، تغییر Schema، DDL/DML مستقیم، تغییر `DepositService`/`TransferService` یا عملیات Git انجام نشد.

## 2. قرارداد مورد انتظار

حضور یا عدم حضور `effectiveAt` بخشی از هویت معنایی درخواست Event است:

| درخواست اولیه | Retry | نتیجه |
|---|---|---|
| `effectiveAt` صریح | همان مقدار صریح | `200` و `replayed=true` |
| بدون `effectiveAt` | بدون `effectiveAt` | `200` و `replayed=true` |
| `effectiveAt` صریح | بدون `effectiveAt` | `409 IDEMPOTENCY_CONFLICT` |
| بدون `effectiveAt` | `effectiveAt` صریح | `409 IDEMPOTENCY_CONFLICT` |
| `effectiveAt` صریح A | `effectiveAt` صریح B ≠ A | `409 IDEMPOTENCY_CONFLICT` |
| سایر فیلدها متفاوت | همان کلید | `409 IDEMPOTENCY_KEY_REUSED` |

## 3. بررسی Fingerprint درخواست Event

`fingerprintForEvent` در `src/application/holdservice.cpp` این فیلدها را به ترتیب شامل می‌شود:

1. `holdId`
2. `eventType`
3. `amount` فقط برای `PARTIAL_RELEASE` (برای سایر انواع رشته خالی)
4. `effectiveAt` نرمال‌شده؛ اگر ارسال نشده باشد رشته خالی
5. `reasonCode`
6. `referenceType`
7. `referenceId`

نتیجه بررسی:

- Fingerprint از نظر مفهومی «غایب» را با رشته خالی و «صریح» را با مقدار نرمال‌شده نمایش می‌دهد؛ بنابراین این دو حالت در ورودی Fingerprint قابل تفکیک‌اند.
- اما نتیجه Fingerprint رویداد در دیتابیس ذخیره نمی‌شود؛ جدول `account_hold_events` ستون `request_fingerprint` ندارد. Idempotency رویداد با مقایسه فیلدبه‌فیلد ردیف موجود انجام می‌شود.
- نقص واقعی در `lookupEventByKey` بود: وقتی Retry بدون `effectiveAt` می‌آمد، شرط مقایسه زمان به‌صورت Wildcard عمل می‌کرد و زمان ذخیره‌شده را نادیده می‌گرفت. در نتیجه Retry بدون زمان برای رویداد اولیه صریح، به‌اشتباه Replay می‌شد.
- حالت معکوس (اولیه بدون زمان، Retry صریح) در عمل به‌دلیل مقایسه مقدار صریح با زمان پیش‌فرض ذخیره‌شده معمولاً Conflict می‌شد، اما این رفتار تضمین‌شده نبود و به تصادف مقدار زمان وابسته بود.

نتیجه: تغییر خود `fingerprintForEvent` لازم نبود؛ چون مقدار خالی/صریح را از قبل تفکیک می‌کند. اصلاح در منطق Lookup انجام شد تا حضور/عدم حضور ذخیره‌شده را نیز تشخیص دهد.

## 4. منطق اصلاح

### 4.1 تشخیص حضور `effectiveAt` بدون تغییر Schema

Service برای درخواست بدون `effectiveAt` مقدار پیش‌فرض سرور را این‌گونه ذخیره می‌کند:

```text
effective_at = date_trunc('milliseconds', CURRENT_TIMESTAMP)
```

در حالی که `created_at` همان ردیف مقدار کامل `CURRENT_TIMESTAMP` تراکنش را دارد. بنابراین برای ردیف‌هایی که با این Service ساخته شده‌اند:

- درخواست بدون `effectiveAt`: `effective_at = date_trunc('milliseconds', created_at)`
- درخواست با `effectiveAt` صریح: مقدار ذخیره‌شده برابر زمان نرمال‌شده کلاینت است و عموماً با `date_trunc('milliseconds', created_at)` برابر نیست.

این قاعدе در `lookupEventByKey` اعمال شد:

```sql
(CASE
   WHEN CAST(:effective_provided AS VARCHAR) = 'false'
   THEN effective_at = date_trunc('milliseconds', created_at)
   ELSE effective_at = CAST(:client_effective AS TIMESTAMPTZ)
        AND effective_at <> date_trunc('milliseconds', created_at)
 END) AS effective_matches
```

### 4.2 تفکیک کد خطا

- اگر فقط/همچنین `effectiveAt` مطابقت نداشته باشد: `IDEMPOTENCY_CONFLICT` با HTTP `409`.
- اگر `effectiveAt` مطابق باشد ولی فیلد دیگری متفاوت باشد: همان `IDEMPOTENCY_KEY_REUSED` با HTTP `409`.
- نگاشت `IDEMPOTENCY_CONFLICT` در `HoldController` به `409 Conflict` اضافه شد.

بنابراین رفتار Idempotency سایر فیلدها تغییر نکرده است.

## 5. Build

دستور استاندارد پروژه:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

## 6. آزمون‌های HTTP

محیط: سرور محلی `http://localhost:8080`، دیتابیس `corebanking`.

Hold آزمایشی: `holdId=7`، حساب `1`، مبلغ `0.50`، `effectiveAt` صریح در گذشته، کلید `effat-review-1793723166-hold`.

زمان‌های صریح استفاده‌شده:

- `T1 = 2026-11-03T16:23:06.000Z`
- `T2 = 2026-11-03T16:23:07.000Z`

| سناریو | کلید | درخواست | نتیجه |
|---|---|---|---|
| Retry یکسان با `effectiveAt` صریح | `...-t1` | ایجاد با T1، سپس همان T1 | `201` سپس `200` و `replayed=true`، همان `eventId=8` |
| Retry یکسان بدون `effectiveAt` | `...-t2` | ایجاد بدون زمان، سپس بدون زمان | `201` سپس `200` و `replayed=true`، همان `eventId=9` |
| اولیه صریح، Retry بدون زمان | `...-t3` | ایجاد با T1، سپس بدون زمان | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| اولیه بدون زمان، Retry صریح | `...-t4` | ایجاد بدون زمان، سپس با T1 | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| دو `effectiveAt` صریح متفاوت | `...-t5` | ایجاد با T1، سپس T2 | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| سایر فیلدها متفاوت | `...-t6` | ایجاد بدون زمان با `reasonCode=R6A`، سپس `R6B` | `201` سپس `409 IDEMPOTENCY_KEY_REUSED` |

همه شش سناریو مطابق قرارداد بودند. Retryهای موفق رکورد جدید ایجاد نکردند و `eventId` تغییر نکرد.

## 7. بررسی دیتابیس (Read-only)

با نقش `corebanking_reader`:

- برای `hold_id=7` دقیقاً ۶ رویداد با ۶ کلید Idempotency مجزا ثبت شده است؛ هیچ رکورد تکراری از Retryها ایجاد نشده است.
- مجموع مبلغ آزادشده: `0.06` از مبلغ اولیه `0.50`.
- ردیف‌های بدون `effectiveAt` دارای `effective_at = date_trunc('milliseconds', created_at)` هستند.
- ردیف‌های با `effectiveAt` صریح، مقدار T1 را ذخیره کرده‌اند و این شرط را ندارند.

Eventهای ایجادشده:

| eventId | key | effectiveAt |
|---|---|---|
| 8 | `effat-review-1793723166-t1` | `2026-11-03T16:23:06.000Z` (صریح) |
| 9 | `effat-review-1793723166-t2` | پیش‌فرض سرور |
| 10 | `effat-review-1793723166-t3` | `2026-11-03T16:23:06.000Z` (صریح) |
| 11 | `effat-review-1793723166-t4` | پیش‌فرض سرور |
| 12 | `effat-review-1793723166-t5` | `2026-11-03T16:23:06.000Z` (صریح) |
| 13 | `effat-review-1793723166-t6` | پیش‌فرض سرور |

## 8. محدودیت شناخته‌شده

جدول `account_hold_events` ستون ذخیره Fingerprint یا پرچم Presence ندارد. بنابراین تشخیص «زمان ارسال‌نشده» از روی رابطه `effective_at` با `created_at` انجام می‌شود.

در یک حالت مرزی نادر، اگر کلاینت در درخواست اولیه مقدار صریحی ارسال کند که دقیقاً برابر `date_trunc('milliseconds', created_at)` همان ردیف باشد، آن ردیف ممکن است به‌اشتباه «بدون زمان صریح» تلقی شود. رفع کامل و قطعی این ابهام نیازمند ذخیره Presence/Fingerprint در `account_hold_events` و یک Migration است که در این مرحله انجام نشد.

## 9. Git

هیچ دستور Git برای stage، commit، push، reset، checkout یا تغییر branch اجرا نشد.
