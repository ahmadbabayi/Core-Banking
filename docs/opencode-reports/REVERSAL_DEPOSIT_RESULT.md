# برگشت واریز (Reversal) — گزارش پیاده‌سازی و آزمون — 2026-11-21

هدف این مرحله: پیاده‌سازی و آزمون فقط **برگشت کاملِ یک واریز (DEPOSIT)** که وضعیت `COMPLETED` دارد و **کارمزد ندارد**، از مسیر جدید `POST /api/v1/transactions/{id}/reversals`، و مستندسازی شواهد واقعی Build، API و SELECT.

دامنهٔ این مرحله **فقط** همین است. **برگشت برداشت (Withdrawal)، برگشت انتقال (Transfer)، برگشت جزئی، و برگشت تراکنشی که دارای کارمزد است، خارج از محدوده‌اند** و در این مرحله هیچ تغییری در Withdrawal یا Transfer انجام نشده است.

## ۱. مبنا و فایل‌های تغییرکرده

- SHA مبدا: `08b8e37a1aea10998b54e3de1a49096ea806dc93` (قبل از شروع این مرحله درخت تمیز بود؛ HEAD تغییر نکرد).
- هیچ DDL/Migration، SQL نوشتنی، دستور staging/commit/push یا تغییر در Transfer/Withdrawal انجام نشد.

فایل‌های جدید:

| فایل | نقش |
|---|---|
| `src/application/reversalservice.h` / `.cpp` | `ReversalService::reverse(originalTransactionId, idempotencyKey)` → `ReversalResult` |
| `src/web/reversalcontroller.h` / `.cpp` | `ReversalController` و نگاشت نتیجه به HTTP |

فایل‌های ویرایش‌شده:

| فایل | تغییر |
|---|---|
| `src/application/contracts/itransactionrepository.h` | افزودن `TransactionFeeLookupResult` و متدهای `findByIdForUpdate`، `findSuccessfulReversalForTransaction`، `findFeeForTransaction` |
| `src/application/contracts/ijournalrepository.h` | افزودن `JournalEntryLine`/`JournalLinesResult` و متدهای `readPostedJournalLinesForTransaction` و `createReversalEntries` |
| `src/infrastructure/repository/transactionrepository.{h,cpp}` | پیاده‌سازی متدهای جدید (قفل `FOR UPDATE` سطر اصلی، یافتن برگشت موفق، جست‌وجوی کارمزد) |
| `src/infrastructure/repository/journalrepository.{h,cpp}` | خواندن سطرهای Journal `POSTED` تراکنش اصلی و ساخت سطرهای معکوس |
| `src/web/httprouter.{h,cpp}` | `setReversalController` + مسیر `POST /api/v1/transactions/{id}/reversals` با کد 503 `REVERSAL_SERVICE_UNAVAILABLE` |
| `src/main.cpp` | ساخت/اتصال `ReversalService`/`ReversalController` |
| `CMakeLists.txt` | افزودن ۴ فایل جدید |

## ۲. Build

```
cmake --build /home/it4850/Projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه: موفق؛ خروجی با `[100%] Built target CoreBanking` پایان یافت. هیچ reconfigure یا تغییر Kit/نوع Build انجام نشد.

## ۳. محیط

- API محلی: `http://localhost:8080`؛ `GET /api/v1/health` → `200` و `{"data":{"status":"UP"},"success":true}`.
- بررسی SQL فقط با `corebanking_reader` روی دیتابیس `corebanking` (localhost:5432).
- مقادیر ثابت محیط: Currency 1 (IRR)، Branch 1، AccountType 1، Product 1، Mapping 1 (product 1 → ledger 2)، Ledger ASSET 1 (`10100101`) و Ledger LIABILITY 2 (`20100101`).

## ۴. Fixture اختصاصی Reversal (فقط از API)

| مورد | مسیر | شناسه |
|---|---|---|
| Customer | `POST /api/v1/customers` | 4 (`3293306570`) |
| Account | `POST /api/v1/accounts` | 5 (`1000000000060`) |

واریزهای fixture (هر کدام با یک Idempotency key):

| تراکنش | مبلغ | کلید Idempotency | Journal POSTED |
|---|---|---|---|
| 8 (`DEP-1795237593744-3ADB5EE6`) | 120 | `rev-fixture-deposit-20261121` | 7 (سطر ۱: دارایی 1 بدهکار 120؛ سطر ۲: بدهی 2/حساب 5 بستانکار 120) |
| 10 (`DEP-1795237669582-3532A267`) | 50 | `rev-fixture-deposit-concurrency-20261121` | 9 |
| 11 (`DEP-1795237669600-BBD2726E`) | 80 | `rev-fixture-deposit-insufficient-20261121` | 10 |

حساب‌های ۱ و ۲ (fixture‌های Transfer/Withdrawal) دست‌نخورده ماندند.

## ۵. آزمون‌های API اجراشده

| # | سناریو | درخواست | نتیجه واقعی |
|---|---|---|---|
| 1 | برگشت موفق واریز بدون کارمزد | `POST /api/v1/transactions/8/reversals` با `{"idempotencyKey":"rev-key-1-20261121"}` | `201`؛ `{"data":{"transactionId":9,"transactionNumber":"REV-1795237616001-30B8A449","reversesTransactionId":8,"status":"COMPLETED","code":"REVERSAL_COMPLETED","message":"Reversal completed"}}` |
| 2 | Replay همان کلید (برگشتِ از قبل انجام‌شده) | همان کلید `rev-key-1-20261121` روی تراکنش 8 | `201` با همان نتیجهٔ ثبت‌شده (تراکنش 9) — بدون اثر تکراری |
| 3 | همان کلید با تراکنش دیگر (Conflict) | کلید `rev-key-1-20261121` روی تراکنش 1 | `422`، `{"error":{"code":"IDEMPOTENCY_KEY_REUSED",...}}` |
| 4 | برگشت دوگانه با کلید متفاوت | کلید `rev-key-4-double-20261121` روی تراکنش 8 | `422`، `{"error":{"code":"TRANSACTION_ALREADY_REVERSED",...}}`؛ بدون ایجاد برگشت جدید |
| 5 | هم‌زمانی: دو کلید متفاوت روی یک واریز برگشت‌نشده (تراکنش 10) | `rev-conc-key-A-20261121` و `rev-conc-key-B-20261121` | فقط یک درخواست موفق شد: `201` تراکنش 12 (`REV-1795238982157-720D3D3A`)؛ درخواست دیگر `422` `TRANSACTION_ALREADY_REVERSED` |
| 6 | موجودی ناکافی: واریز 80 + Hold مؤثر 1 (تراکنش 11) | کلید `rev-key-insufficient-20261121` | `422`، `{"error":{"code":"INSUFFICIENT_AVAILABLE_FUNDS",...}}`؛ تراکنش 11 همچنان `COMPLETED`؛ تراکنش 13٬ REJECTED شد |
| 7 | تراکنش اصلی ناموجود | `POST /api/v1/transactions/999999/reversals` با `rev-key-notfound-20261121` | `422`، `{"error":{"code":"ORIGINAL_TRANSACTION_NOT_FOUND",...}}` |
| 8 | برگشت تراکنش غیر واریز (تراکنش 3 که بیرون از دامنهٔ انتقال است) | `rev-key-transfer-20261121` | `422`، `{"error":{"code":"TRANSACTION_NOT_REVERSIBLE",...}}` |

## ۶. تأیید SELECT (فقط `corebanking_reader`)

وضعیت نهایی تراکنش‌ها:

| id | transaction_number | type | status | amount | reverses |
|---|---|---|---|---|---|
| 8 | `DEP-1795237593744-3ADB5EE6` | DEPOSIT | REVERSED | 120 | — |
| 9 | `REV-1795237616001-30B8A449` | REVERSAL | COMPLETED | 120 | 8 |
| 10 | `DEP-1795237669582-3532A267` | DEPOSIT | REVERSED | 50 | — |
| 11 | `DEP-1795237669600-BBD2726E` | DEPOSIT | COMPLETED | 80 | — |
| 12 | `REV-1795238982157-720D3D3A` | REVERSAL | COMPLETED | 50 | 10 |
| 13 | `REV-1795239016548-7AC13C8B` | REVERSAL | REJECTED (`INSUFFICIENT_AVAILABLE_FUNDS`) | 80 | 11 |

- **حداکثر یک برگشت موفق برای هر تراکنش اصلی**: تراکنش 8 → 1 برگشت COMPLETED، تراکنش 10 → 1، تراکنش 11 → 0 (فقط REJECTED). جمع با `FILTER (WHERE status='COMPLETED')` تأیید شد: `{8:1, 10:1, 11:0}`.
- **Journal معکوس POSTED** (دو سطر، بدهکار↔بستانکار جابه‌جا نسبت به Journal اصلی):
  - Journal 8 (تراکنش 9، برگشت واریز 120): سطر ۱: دارایی 1 **بستانکار** 120؛ سطر ۲: بدهی 2/حساب 5 **بدهکار** 120 (معکوس Journal 7).
  - Journal 11 (تراکنش 12، برگشت واریز 50): سطر ۱: دارایی 1 **بستانکار** 50؛ سطر ۲: بدهی 2/حساب 5 **بدهکار** 50.
  - برای تراکنش 13 (REJECTED) هیچ Journal برگشت ساخته نشد.
  - Journal اصلی 7 دست‌نخورده ماند.
- **ماندهٔ حساب 5**: پس از همهٔ آزمون‌ها `ledger_balance = 80` و `available_balance = 80` (فقط واریز 80 تراکنش 11 باقی است). Hold با شناسهٔ 1 (`rev-test`، مبلغ 1) همچنان `ACTIVE` و `currently_blocked_amount = 1` است.
- **`ledger_account_balance`** نهایی:
  - Ledger 1 (دارایی): `debit_total=1750`, `credit_total=170`, `balance=1580`.
  - Ledger 2 (بدهی): `debit_total=185.12345678`, `credit_total=1765.12345678`, `balance=-1580.00000000`.
  - توازن دارایی/بدهی حفظ شده است (`1580` در برابر `-1580`).

رفتار حسابداری تأییدشده: برگشت واریز، سمت بدهی مشتری (LIABILITY) را **بدهکار** و سرفصل تسویه (ASSET) را **بستانکار** می‌کند؛ یعنی دقیقاً معکوس Journal اصلی، و وضعیت تراکنش اصلی پس از اتمام ثبت برگشت به `REVERSED` می‌رود.

## ۷. موارد اجرانشده — با تفکیک از موارد اجرا‌شده

موارد زیر **اجرا نشدند** و به‌عنوان موفق گزارش نمی‌شوند:

- **کفِ ماندهٔ فعال (حداقل مانده):** هیچ قاعدهٔ مؤثر حداقل مانده‌ای در `product_balance_rule`/`account_balance_rule_override` وجود ندارد (مانند گزارش Withdrawal؛ API مجاز ساخت Rule نیز وجود ندارد). بنابراین رفتار برگشت در حضور قاعدهٔ کف ماندهٔ فعال آزمون نشده است. بررسی موجودی فقط با `ledger_balance − active_holds − minimum ≥ amount` و با `minimum` واقعیِ صفر اجرا شد.
- **مسیرهای شکست زیرساختی، Rollback و Commit نامشخص:** هیچ Fault Injection انجام نشد؛ شکست واقعی `commit` (`CommitOutcomeUnknown`)، شکست `rollback`، شکست BEGIN روی اتصال باز، و مسیرهای بازگشتی پس از آنها در **runtime** آزمون نشدند. این مسیرها فقط در کد پیاده‌سازی شده‌اند و خروجی واقعی QPSQL/PostgreSQL برایشان ثبت نشده است.
- **وضعیت fakeهای تست پس از تغییر interfaceها:** افزودن متدهای pure virtual به `ITransactionRepository` و `IJournalRepository` ایجاب می‌کرد که fakeهای موجود در `tests/deposit_orchestration_test.cpp` و `tests/withdrawal_orchestration_test.cpp` به stub برای متدهای جدید مجهز شوند. این تطبیق انجام شد: stubهای `findByIdForUpdate`، `findSuccessfulReversalForTransaction`، `findFeeForTransaction` و `readPostedJournalLinesForTransaction`، `createReversalEntries` اضافه شدند. این فایل‌ها بخشی از `CMakeLists.txt` نیستند (فقط یک `add_executable(CoreBanking)` و بدون `add_test`)؛ بنابراین در Build رسمی کامپایل نمی‌شوند. هر دو فایل با پرچم‌های موجود Build (بدون reconfigure) جداگانه تا مرحلهٔ object کامپایل شدند و خروجی `0` گرفتند؛ **لینک و اجرای باینری تست انجام نشد**، چون هدف تستی در CMake موجود وجود ندارد.

## ۸. محدودیت‌ها

- دامنه فقط برگشت کامل واریزِ `COMPLETED` و بدون کارمزد است؛ برگشت واریزِ دارای کارمزد با `TRANSACTION_HAS_FEES` رد می‌شود و هیچ سناریویی برای آن آزمون نشده است.
- برگشت برداشت، برگشت انتقال و برگشت جزئی پیاده‌سازی و آزمون نشده‌اند.
- اضافه‌برداشت (Overdraft) فعال نیست و در صورت نیازِ برگشت به تسویه، با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود.
- `available_balance` همچنان Cache کهنه‌شونده است و تصمیم مالی برگشت بر آن بنا نشده؛ تصمیم بر پایهٔ `ledger_balance − holds − minimum` گرفته شده است.
- هم‌زمانی: فقط یک درخواست موفق شد؛ اما رقابت واقعیِ دو تراکنش مستقل PostgreSQL روی قفل ردیف (`FOR UPDATE`) آزمون نشد. سرور تک‌ریسمانی با Connection مشترک است و این آزمون هم‌پوشانی تراکنشی واقعی دو تراکنش دیتابیس را اثبات نمی‌کند.