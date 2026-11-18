# نتیجه آزمون Withdrawal با Rule کف مانده — 2026-11-18

نوع مرحله: آزمون مسیر واقعی Withdrawal با Rule فعال در PostgreSQL. هیچ کد تولیدی تغییر نکرد. سه سناریوی درخواستی **اجرا نشدند**، چون fixture مجاز Rule در دسترس نبود و SQL نوشتنی ممنوع است.

## ۱. مبنا

- HEAD: `279416c350930fc16edce5d602d39819b2ba4ffc`.
- تغییرات مرحله Transfer (گذار وضعیت) در working tree حاضر و دست‌نخورده است؛ این مرحله هیچ فایل تولیدی را تغییر نداد.
- `git diff --check`: exit 0.

## ۲. سیاست (بدون تغییر)

سیاست مصوب و پیاده‌شده قبلی: `effective_minimum = GREATEST(product_minimum, account_override_minimum)`؛ Override فقط افزایش‌دهنده است. مسیر Withdrawal همین سیاست را از `BalanceRepository::loadLockedAccountSnapshot` می‌خواند و در `checkWithdrawalFunds` اعمال می‌کند. هیچ بازنویسی انجام نشد.

## ۳. مانع اجرا

- API موجود هیچ مسیری برای ساخت `product_balance_rule` یا `account_balance_rule_override` ندارد. فهرست کامل Routeها فقط شامل health، transfers، deposits، withdrawals، holds، accounts، customers، currencies، account-types، products، product-ledger-mappings، ledger-accounts، countries، provinces، cities و branches است.
- محصول و Account Opening هیچ فیلد حداقل مانده یا Override نمی‌پذیرند.
- پس از ساخت محصول ۱ و حساب‌های ۱ و ۲ در مرحله قبل **فقط از API**، جدول‌های Rule همچنان خالی ماندند؛ یعنی Trigger یا مسیر جانبی برای seed وجود ندارد.

وضعیت read-only پیش از آزمون:

```text
product_balance_rule = 0
account_balance_rule_override = 0
WITHDRAWAL transactions = 0
```

Rule مؤثر فعلی:

| account_id | ledger_balance | product_minimum | override_minimum | effective_minimum |
|---|---|---|---|---|
| 1 | 984.87654322 | 0 | 0 | 0 |
| 2 | 115.12345678 | 0 | 0 | 0 |

بنابراین هر برداشت فعلی بدون کف مانده اجرا می‌شود و نتیجه‌اش به قاعدهٔ کف وابسته نیست.

## ۴. سناریوها — اجرا نشده

| سناریو | وضعیت | دلیل |
|---|---|---|
| Override پایین‌تر کف محصول را کاهش ندهد | اجرا نشد | Rule و Override فعال وجود ندارد و API ساخت آن نیست |
| Override بالاتر کف مؤثر را افزایش دهد | اجرا نشد | همان مانع |
| مرز دقیق کف پذیرفته و ۰٫۰۰۰۰۰۰۰۱ بیشتر رد شود | اجرا نشد | همان مانع |

هیچ درخواست Withdrawal در این مرحله ارسال نشد؛ بنابراین هیچ مانده، تراکنش یا Journal جدیدی ایجاد نشد. Journalهای موجود همان ۴ ردیف مراحل Deposit/Transfer هستند.

## ۵. پیش‌نیاز دقیق برای اجرای بعدی

کاربر باید سه ردیف Rule را (پس از بازبینی) اعمال کند؛ این SQL اجرا نشد:

```sql
INSERT INTO product_balance_rule
    (product_id, minimum_balance, overdraft_enabled, overdraft_limit,
     effective_from, effective_to)
VALUES
    (1, 100.00000000, FALSE, 0,
     TIMESTAMPTZ '2026-11-01 00:00:00+00', NULL);

INSERT INTO account_balance_rule_override
    (account_id, minimum_balance, effective_from, reason_code)
VALUES
    (1, 50.00000000,
     TIMESTAMPTZ '2026-11-01 00:00:00+00', 'TEST_LOWER_OVERRIDE');

INSERT INTO account_balance_rule_override
    (account_id, minimum_balance, effective_from, reason_code)
VALUES
    (2, 200.00000000,
     TIMESTAMPTZ '2026-11-01 00:00:00+00', 'TEST_HIGHER_OVERRIDE');
```

برنامهٔ آزمون پس از اعمال:

1. **Override پایین‌تر:** حساب ۱ (کف مؤثر ۱۰۰، Override ۵۰). با ماندهٔ `984.87654322`، برداشت `900.00000000` باید با `INSUFFICIENT_AVAILABLE_FUNDS` رد شود (ماندهٔ باقی‌مانده `84.87654322` < ۱۰۰؛ اگر Override جای کف محصول را می‌گرفت، ۸۴٫۸۷ ≥ ۵۰ بود و پذیرفته می‌شد). سپس برداشت `884.87654322` باید پذیرفته شود و مانده را دقیقاً ۱۰۰ کند.
2. **Override بالاتر:** حساب ۲ (کف مؤثر ۲۰۰). ابتدا با Deposit مجاز، مانده به `250.00000000` می‌رسد. برداشت `50.00000001` باید رد شود (باقی‌مانده `199.99999999` < ۲۰۰؛ با کف محصول ۱۰۰ پذیرفته می‌شد) و برداشت `50.00000000` باید پذیرفته شود و مانده را دقیقاً ۲۰۰ کند.
3. **مرز دقیق:** دو حساب جدید C و D زیر محصول ۱ بدون Override (کف ۱۰۰)، هرکدام با Deposit به ماندهٔ `150.00000000`. برداشت `50.00000000` از C باید پذیرفته شود (باقی‌مانده ۱۰۰) و برداشت `50.00000001` از D باید رد شود (باقی‌مانده `99.99999999`) در حالی که مانده و Journal D تغییر نکند.

پیش‌نیازها: Rule محصول با `product_id=1`، دو Override فعال روی حساب‌های ۱ و ۲، و اجازهٔ ساخت دو حساب جدید و Deposit با API محلی. بازهٔ `effective_from` باید گذشته باشد و `effective_to` تهی یا بعد از حالا.

## ۶. محدودیت‌ها

- بدون Rule فعال، هیچ ادعایی دربارهٔ درستی رفتار کف مانده در این مرحله مطرح نمی‌شود؛ شواهد قبلی Withdrawal (مسیر موفق بدون Rule) به‌جای خود باقی است.
- آزمون Ruleهای زمان‌مند، هم‌زمانی و Overdraft همچنان اجرا نشده‌اند.
- هیچ DDL/Migration، SQL نوشتنی، Fault Injection یا عملیات Git انجام نشد.
