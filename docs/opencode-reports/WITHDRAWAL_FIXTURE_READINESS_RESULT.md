# آماده‌سازی fixture Withdrawal — گزارش شواهد — 2026-11-18

هدف: بررسی پیش‌نیازهای برداشت و ساخت دادهٔ تست فقط از API مجاز. هیچ Withdrawal/Transfer ارسال نشد، هیچ SQL نوشتنی/DDL/تغییر کد/عملیات Git انجام نشد.

## ۱. محیط

- API محلی: `http://localhost:8080/api/v1/health` → `200` و `{"data":{"status":"UP"},"success":true}`.
- اتصال برنامه از `src/infrastructure/database.cpp`: `localhost:5432`، دیتابیس `corebanking`، کاربر `corebanking`.
- بررسی SQL فقط با `corebanking_reader` روی همان دیتابیس انجام شد.
- نسخهٔ Build موجود اجرا و در پایان متوقف شد (`port 8080 free`).

## ۲. وضعیت پیش از این مرحله (از قبل موجود)

| مورد | شناسه | وضعیت |
|---|---|---|
| Currency IRR | 1 | ACTIVE، minorUnit 2 |
| Branch `0000001` | 1 | ACTIVE |
| AccountType `10` | 1 | ACTIVE، IRR |
| Product `SAV-T5` | 1 | ACTIVE، IRR، effective از 2026-01-01 |
| Mapping محصول | 1 | ACTIVE، product 1 → ledger 2 |
| Ledger تسویه `10100101` | 1 | ASSET، postable، ACTIVE (منطبق با کد ثابت `DepositService`/`WithdrawalService`) |
| Ledger بدهی مشتری `20100101` | 2 | LIABILITY، postable، ACTIVE |
| Customers | 1، 2 | ACTIVE (مرحلهٔ Transfer؛ دست‌نخورده) |
| Accounts | 1، 2 | ACTIVE با مانده‌های `984.87654322` و `115.12345678` (مرحلهٔ Transfer؛ دست‌نخورده) |
| Product balance rule | — | ۰ ردیف |
| Account override | — | ۰ ردیف |

حساب‌های ۱ و ۲ به‌دلیل ماندهٔ غیرصفر و نامرتبط با هدف ۱۵۰/۲۵۰ این تست استفاده نشدند و هیچ تغییری روی آن‌ها انجام نشد.

## ۳. fixture ساخته‌شده از API

| مورد | مسیر API | شناسهٔ واقعی | نتیجه |
|---|---|---|---|
| Customer تست | `POST /api/v1/customers` | 3 (`WDF-000001`) | «از API ساخته شد»، ACTIVE |
| Account A | `POST /api/v1/accounts` | 3 (`1000000000034`) | «از API ساخته شد»، ACTIVE |
| Account B | `POST /api/v1/accounts` | 4 (`1000000000055`) | «از API ساخته شد»، ACTIVE |

بلافاصله پس از افتتاح حساب، رکورد `account_balance` برای هر دو وجود داشت و مقدار آن صفر بود (نه «ردیف ناموجود»):

```text
account 3: ACTIVE, ledger_balance=0, available_balance=0, effective_holds=0
account 4: ACTIVE, ledger_balance=0, available_balance=0, effective_holds=0
```

## ۴. واریزها و شواهد

| حساب | مبلغ | کلید idempotency | پاسخ | شناسهٔ تراکنش | شماره |
|---|---|---|---|---|---|
| A (3) | `150.00000000` | `wd-fixture-deposit-a-20261118` | `201 DEPOSIT_COMPLETED` | 6 | `DEP-1794995338630-2A2F4604` |
| B (4) | `250.00000000` | `wd-fixture-deposit-b-20261118` | `201 DEPOSIT_COMPLETED` | 7 | `DEP-1794995338645-BEB5EFB8` |

پس از واریز:

```text
account 3: ledger_balance=150, available_balance=150, effective_holds=0
account 4: ledger_balance=250, available_balance=250, effective_holds=0
```

تراکنش‌ها: دو ردیف DEPOSIT با وضعیت COMPLETED و `DEPOSIT_COMPLETED`، هر کلید فقط یک ردیف (بدون تکرار).

Journalها:

| Journal | Transaction | وضعیت | Entries | Debit | Credit |
|---|---|---|---|---|---|
| 5 | 6 | POSTED | 2 | 150 | 150 |
| 6 | 7 | POSTED | 2 | 250 | 250 |

ثبت‌ها: بدهکار Ledger ASSET 1 / بستانکار Ledger LIABILITY 2 روی حساب مربوطه.

## ۵. وضعیت Rule/Override (آزمون کف مانده)

```text
product_balance_rule (product 1) = 0
account_balance_rule_override (accounts 3,4) = 0

account 3: product_min=0, override_min=0, effective_min=0, overdraft_limit=0
account 4: product_min=0, override_min=0, effective_min=0, overdraft_limit=0
```

هیچ Route/Controller برای ساخت `product_balance_rule` یا `account_balance_rule_override` در این checkout وجود ندارد (فهرست کامل Routeها بازبینی شد) و Product/Account Opening هم فیلد Rule نمی‌پذیرند. بنابراین مقادیر هدف کف محصول ۱۰۰، Override حساب A=۵۰ و حساب B=۲۰۰ **قابل ساخت از مسیر مجاز نیستند**.

**نتیجه:** حساب‌ها و مانده‌های تست آماده‌اند؛ آزمون کف مانده به‌دلیل نبود Rule/Override فعالِ مجاز مسدود است.

## ۶. تفکیک آمادگی

- **آماده برای برداشت عادی:** حساب ۳ با ماندهٔ ۱۵۰ و حساب ۴ با ماندهٔ ۲۵۰، هر دو ACTIVE، دارای رکورد مانده، بدون Hold مؤثر.
- **آماده برای آزمون کف مانده:** خیر؛ Rule/Override فعال وجود ندارد و مسیر مجاز ساخت آن موجود نیست.

## ۷. محدودیت‌ها

- هیچ برداشت یا انتقالی اجرا نشد؛ بنابراین Journal برداشت ایجاد نشده است.
- Ruleهای زمان‌مند، Overdraft و هم‌زمانی همچنان آزمون‌نشده‌اند.
