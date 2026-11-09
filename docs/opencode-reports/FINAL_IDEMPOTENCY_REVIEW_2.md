# FINAL IDEMPOTENCY REVIEW 2 — Trigger Order، توابع Canonical، ترتیب اعتبارسنجی و Legacy

تاریخ: 2026-11-03

این گزارش فقط بررسی و طراحی است. هیچ فایل کد، Migration، DDL/DML یا عملیات Git تغییر/اجرا نشد. تمام بررسی‌های دیتابیس با نقش Read-only `corebanking_reader` انجام شد.

---

## 0. خلاصه نتایج

| موضوع | نتیجه |
|---|---|
| ترتیب Triggerها | تنها یک Trigger کاربری BEFORE INSERT وجود دارد. Triggerهای AFTER INSERT فقط داخلی FK هستند و فیلدها را تغییر نمی‌دهند. |
| نرمال‌سازی قبل از اعتبارسنجی | اگر داخل `trg_validate_account_hold_event` و در ابتدای تابع انجام شود، قبل از Idempotency، Business Validation، CHECK/NOT NULL و FK بعدی اجرا می‌شود. |
| توابع SQL | سه تابع با تعریف کامل پیشنهاد شد. Fingerprint با Test Vectorهای نسخه ۱ سازگار است. |
| ترتیب Constraintها | BEFORE INSERT Trigger قبل از CHECK/NOT NULL اجرا می‌شود؛ بنابراین نرمال‌سازی `NEW.amount` و `NEW.effective_at` روی مقادیر نهایی اثر دارد. |
| `NaN` و `Infinity` | CHECK فعلی `amount > 0` آن‌ها را رد نمی‌کند؛ `NaN > 0` و `Infinity > 0` در PostgreSQL برابر true هستند. تابع Canonical باید غیرمتناهی‌ها را رد کند. |
| تبدیل Numeric | برای اعداد متناهی، تبدیل متن Canonical و برگشت به `numeric` دقیق است؛ اما محدودیت ۸ رقم اعشار باید در Trigger جداگانه enforce شود. |
| INSERT مستقیم | با Metadata ناقص، نسخه ناشناخته، Fingerprint نامعتبر یا Canonical-form ناسازگار رد می‌شود. |
| Legacy | Trigger جدید روی ردیف‌های قدیمی اثری ندارد؛ Fallback فعلی حفظ می‌شود؛ Backfill/UPDATE لازم نیست. |

---

## 1. ترتیب واقعی Triggerها

### 1.1 خروجی واقعی `pg_trigger`

با نقش `corebanking_reader`:

| Trigger | tgtype | internal | تعریف |
|---|---:|---|---|
| `trg_validate_account_hold_event` | 7 | false | `BEFORE INSERT ... FOR EACH ROW EXECUTE FUNCTION validate_account_hold_event()` |
| `trg_account_hold_event_immutable` | 27 | false | `BEFORE DELETE OR UPDATE ... FOR EACH ROW EXECUTE FUNCTION prevent_account_hold_event_mutation()` |
| `RI_ConstraintTrigger_c_19493` | 5 | true | `AFTER INSERT ON account_hold_events FROM account_hold ... EXECUTE FUNCTION RI_FKey_check_ins()` |
| `RI_ConstraintTrigger_c_19494` | 17 | true | `AFTER UPDATE ... EXECUTE FUNCTION RI_FKey_check_upd()` |

رمزگشایی `tgtype`:

- `7 = 1 + 2 + 4` → ROW + BEFORE + INSERT
- `27 = 1 + 2 + 8 + 16` → ROW + BEFORE + DELETE + UPDATE
- `5 = 1 + 4` → ROW + AFTER + INSERT
- `17 = 1 + 16` → ROW + AFTER + UPDATE

### 1.2 Triggerهای BEFORE INSERT

فقط یک Trigger کاربری BEFORE INSERT وجود دارد:

```sql
CREATE TRIGGER trg_validate_account_hold_event
    BEFORE INSERT ON public.account_hold_events
    FOR EACH ROW
    EXECUTE FUNCTION public.validate_account_hold_event();
```

تعریف کامل تابع فعلی در `database/CURRENT_SCHEMA.sql:67-182` و در Appendix A آمده است.

### 1.3 Triggerهای AFTER INSERT

تنها AFTER INSERT موجود، Trigger داخلی FK است:

```sql
CREATE CONSTRAINT TRIGGER "RI_ConstraintTrigger_c_19493"
    AFTER INSERT ON public.account_hold_events
    FROM account_hold
    NOT DEFERRABLE INITIALLY IMMEDIATE
    FOR EACH ROW
    EXECUTE FUNCTION "RI_FKey_check_ins"();
```

این Trigger:

- فقط وجود ردیف والد `account_hold` را بررسی می‌کند.
- هیچ فیلدی از `NEW` را تغییر نمی‌دهد.
- بعد از CHECK/NOT NULL و بعد از Triggerهای BEFORE اجرا می‌شود.

هیچ Trigger کاربری AFTER INSERT روی `account_hold_events` وجود ندارد.

### 1.4 ترتیب اجرا

ترتیب منطقی برای INSERT جدید:

1. اعمال Defaultهای ستون‌ها، از جمله `effective_at` و `created_at`.
2. اجرای Triggerهای BEFORE INSERT کاربری؛ فعلاً فقط `trg_validate_account_hold_event`.
   - چند Trigger BEFORE INSERT در صورت وجود، به ترتیب نام اجرا می‌شوند.
   - فقط یک Trigger BEFORE INSERT وجود دارد؛ بنابراین ترتیب نام در حال حاضر موضوعیت ندارد.
3. اجرای CHECK و NOT NULL Constraintهای جدول.
4. اجرای Triggerهای AFTER INSERT؛ فعلاً فقط FK داخلی.

نتیجه: اگر نرمال‌سازی در ابتدای `validate_account_hold_event` انجام شود، قبل از تمام موارد زیر اجرا می‌شود:

- Idempotency Lookup داخل همان Trigger
- اعتبارسنجی‌های کسب‌وکاری زمان/انقضا/مبلغ
- CHECK/NOT NULL جدول
- FK بعدی

### 1.5 آیا Trigger دیگری می‌تواند بعد از Fingerprint فیلدها را تغییر دهد؟

خیر، در وضعیت فعلی:

- هیچ Trigger کاربری AFTER INSERT وجود ندارد.
- Trigger داخلی FK فقط بررسی می‌کند و فیلدها را تغییر نمی‌دهد.
- تغییر ردیف پس از INSERT با `trg_account_hold_event_immutable` مسدود است.
- `trg_account_hold_event_immutable` روی UPDATE/DELETE اجرا می‌شود و روی INSERT اجرا نمی‌شود.

بنابراین Fingerprint محاسبه‌شده در BEFORE INSERT نماینده مقادیر نهایی ذخیره‌شده است.

### 1.6 ایراد احتمالی آینده

اگر بعداً یک Trigger جدید BEFORE INSERT با نامی که پیش از `trg_validate_account_hold_event` مرتب می‌شود اضافه شود، ممکن است قبل از نرمال‌سازی/بازمحاسبه Fingerprint اجرا شود. راهکار امن:

- نرمال‌سازی و بازمحاسبه Fingerprint را داخل همان `trg_validate_account_hold_event` نگه داریم؛ یا
- اگر Trigger جداگانه اضافه شد، نام و ترتیب آن صریحاً کنترل و مستند شود.

---

## 2. تعریف کامل توابع پیشنهادی

توابع زیر در این مرحله ایجاد نشده‌اند؛ فقط تعریف پیشنهادی برای Migration آینده است.

### 2.1 `hold_event_fp_trim`

مجموعه Trim همان Unicode White_Space نسخه ۱ است:

```text
U+0009 U+000A U+000B U+000C U+000D
U+0020 U+0085 U+00A0 U+1680
U+2000..U+200A
U+2028 U+2029 U+202F U+205F U+3000
```

تعریف:

```sql
CREATE OR REPLACE FUNCTION public.hold_event_fp_trim(p_value text)
RETURNS text
LANGUAGE sql
IMMUTABLE
STRICT
AS $function$
    SELECT btrim(
        p_value,
        U&'\0009\000A\000B\000C\000D\0020\0085\00A0\1680\2000\2001\2002\2003\2004\2005\2006\2007\2008\2009\200A\2028\2029\202F\205F\3000'
    );
$function$;
```

رفتار:

- NULL → NULL به‌دلیل `STRICT`.
- رشته خالی → رشته خالی.
- فقط Whitespaceهای مجموعه V1 از ابتدا و انتها حذف می‌شوند.
- Whitespace داخلی حفظ می‌شود.
- U+001C تا U+001F جزو مجموعه نیستند.
- هیچ Unicode Normalization انجام نمی‌شود.

### 2.2 `hold_event_canonical_amount`

تعریف:

```sql
CREATE OR REPLACE FUNCTION public.hold_event_canonical_amount(p_amount numeric)
RETURNS numeric
LANGUAGE plpgsql
IMMUTABLE
STRICT
AS $function$
DECLARE
    v_text text;
BEGIN
    v_text := p_amount::text;

    IF v_text IN ('NaN', 'Infinity', '-Infinity') THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    IF position('.' in v_text) > 0 THEN
        v_text := regexp_replace(v_text, '0+$', '');
        v_text := regexp_replace(v_text, '\.$', '');
    END IF;

    RETURN v_text::numeric;
END;
$function$;
```

رفتار:

- `0.10` → `0.1`
- `1.23000000` → `1.23`
- `10.0` → `10`
- `10` → `10` (بدون حذف صفر عدد صحیح)
- `0.000000010` → `0.00000001`
- `NaN`، `Infinity` و `-Infinity` → Exception
- NULL → NULL به‌دلیل `STRICT`

محدودیت مهم:

- این تابع فقط Canonical Representation را نرمال می‌کند.
- محدودیت ۸ رقم اعشار و مثبت بودن را جداگانه باید Trigger enforce کند.
- تابع به‌تنهایی `0.000000001` (۹ رقم اعشار) را رد نمی‌کند؛ این مقدار باید با Regex در Trigger رد شود.

### 2.3 `hold_event_fingerprint_v1`

تعریف:

```sql
CREATE OR REPLACE FUNCTION public.hold_event_fingerprint_v1(
    p_hold_id bigint,
    p_event_type text,
    p_amount numeric,
    p_effective_at_provided boolean,
    p_effective_at timestamptz,
    p_reason_code text,
    p_reference_type text,
    p_reference_id text
) RETURNS text
LANGUAGE plpgsql
STABLE
AS $function$
DECLARE
    v_event_type text;
    v_reason_code text;
    v_reference_type text;
    v_reference_id text;
    v_amount_text text;
    v_amount_provided text;
    v_effective_text text;
    v_payload text;
BEGIN
    IF p_effective_at_provided IS NULL THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID'
            USING ERRCODE = '23514';
    END IF;

    v_event_type := public.hold_event_fp_trim(p_event_type);
    v_reason_code := public.hold_event_fp_trim(p_reason_code);
    v_reference_type := NULLIF(public.hold_event_fp_trim(p_reference_type), '');
    v_reference_id := NULLIF(public.hold_event_fp_trim(p_reference_id), '');

    IF v_event_type IS NULL OR v_event_type = '' THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID'
            USING ERRCODE = '23514';
    END IF;

    IF v_reason_code IS NULL OR v_reason_code = '' THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID'
            USING ERRCODE = '23514';
    END IF;

    IF v_event_type = 'PARTIAL_RELEASE' THEN
        IF p_amount IS NULL THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
                USING ERRCODE = '23514';
        END IF;
        v_amount_provided := '1';
        v_amount_text := public.hold_event_canonical_amount(p_amount)::text;
    ELSE
        v_amount_provided := '0';
        v_amount_text := '';
    END IF;

    IF p_effective_at_provided THEN
        IF p_effective_at IS NULL THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT'
                USING ERRCODE = '23514';
        END IF;

        v_effective_text := to_char(
            date_trunc('milliseconds', p_effective_at) AT TIME ZONE 'UTC',
            'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'
        );
    ELSE
        v_effective_text := '';
    END IF;

    v_payload :=
        'HOLD_EVENT_FP' || chr(10) ||
        '1' || chr(10) ||
        'holdId=' || p_hold_id::text || chr(10) ||
        'eventType=' || octet_length(convert_to(v_event_type, 'UTF8'))::text || ':' || v_event_type || chr(10) ||
        'amountProvided=' || v_amount_provided || chr(10) ||
        'amount=' || octet_length(convert_to(v_amount_text, 'UTF8'))::text || ':' || v_amount_text || chr(10) ||
        'effectiveAtProvided=' || CASE WHEN p_effective_at_provided THEN '1' ELSE '0' END || chr(10) ||
        'effectiveAt=' || octet_length(convert_to(v_effective_text, 'UTF8'))::text || ':' || v_effective_text || chr(10) ||
        'reasonCode=' || octet_length(convert_to(v_reason_code, 'UTF8'))::text || ':' || v_reason_code || chr(10) ||
        'referenceType=' || octet_length(convert_to(COALESCE(v_reference_type, ''), 'UTF8'))::text || ':' || COALESCE(v_reference_type, '') || chr(10) ||
        'referenceId=' || octet_length(convert_to(COALESCE(v_reference_id, ''), 'UTF8'))::text || ':' || COALESCE(v_reference_id, '') || chr(10);

    RETURN encode(sha256(convert_to(v_payload, 'UTF8')), 'hex');
END;
$function$;
```

نکات:

- خروجی Hex lowercase با طول ۶۴ است.
- برای `effective_at_provided=false`، مقدار `effective_at` در Payload رشته خالی است.
- برای `true`، زمان به میلی‌ثانیه Truncate و به UTC تبدیل می‌شود.
- برای انواع غیر `PARTIAL_RELEASE`، مبلغ در Payload خالی است.
- `STABLE` انتخاب شده است، چون `convert_to` و `to_char` در PostgreSQL از نوع STABLE هستند. اگر تابع را `IMMUTABLE` اعلام کنیم، برخلاف طبقه‌بندی توابع داخلی است و در آینده می‌تواند در Index/Generated Column مشکل ایجاد کند.
- این تابع برای استفاده در Trigger کافی است؛ برای Index Expression یا Generated Column باید نسخه‌ای با توابع کاملاً Immutable طراحی شود.

### 2.4 تطابق با Test Vectorهای نسخه ۱

با ساخت همان Payload در یک Query Read-only، دو Hash زیر تأیید شدند:

```text
f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1
5e7271fc717f758194af0a0a66247da6a65e34d669359d084876397a3fa5ea8f
```

این مقادیر با Test Vectorهای قبلی یکسان هستند. بنابراین ساختار Length-Prefix و Trailing Newline با C++ سازگار است.

### 2.5 تطابق با Service

| موضوع | Service | PostgreSQL | وضعیت |
|---|---|---|---|
| Trim | باید Helper صریح V1 داشته باشد | `hold_event_fp_trim` | نیازمند پیاده‌سازی یکسان |
| NULL/Empty | Fingerprint از رشته خالی استفاده می‌کند | `NULLIF` و `COALESCE` | سازگار |
| Unicode | بدون Normalization؛ طول UTF-8 | `octet_length(convert_to(...))` | سازگار |
| Amount | Regex و حذف صفر انتهایی | `hold_event_canonical_amount` | سازگار در محدوده مجاز |
| Timestamp | UTC و میلی‌ثانیه | `to_char(... AT TIME ZONE 'UTC', ...MS...)` | سازگار |
| Presence | `effectiveAtProvided` | `p_effective_at_provided` | سازگار |

نکته مهم: C++ نباید از `QString::size()` برای طول استفاده کند؛ باید `toUtf8().size()` استفاده شود. همچنین C++ نباید به `QString::trimmed()` وابسته بماند؛ باید Helper صریح V1 با همان مجموعه Codepoint پیاده شود.

---

## 3. محدودیت‌ها و ترتیب اعتبارسنجی

### 3.1 نرمال‌سازی `NEW.amount` قبل از Constraintها

بله، اگر نرمال‌سازی در ابتدای `trg_validate_account_hold_event` انجام شود:

- Defaultهای ستون قبل از Trigger اعمال می‌شوند.
- Trigger BEFORE INSERT قبل از CHECK/NOT NULL اجرا می‌شود.
- مقدار نرمال‌شده `NEW.amount` همان مقداری است که Constraintهای جدول و Triggerهای AFTER می‌بینند.

ترتیب پیشنهادی داخل تابع:

1. بررسی Metadata/Version/Format.
2. Trim فیلدهای متنی.
3. نرمال‌سازی `effective_at`.
4. نرمال‌سازی و اعتبارسنجی Canonical مبلغ Partial.
5. بازمحاسبه و مقایسه Fingerprint.
6. Idempotency.
7. Business Validation.
8. `RETURN NEW`.

### 3.2 دقت تبدیل Numeric

- `numeric` در PostgreSQL دقت دلخواه دارد؛ برای اعداد متناهی، تبدیل `numeric → text → canonical text → numeric` بدون افت دقت است.
- نمونه Read-only:
  - `0.10` → `0.1`
  - `1.23000000` → `1.23`
  - `10.0` → `10`
  - `12345678901234567890.12345678` بدون تغییر
  - `0.000000010` → `0.00000001`
- ایراد موجود: `ck_account_hold_event_amount` فقط `amount > 0` را بررسی می‌کند. در PostgreSQL:
  - `'NaN'::numeric > 0` → `true`
  - `'Infinity'::numeric > 0` → `true`
  - `'-Infinity'::numeric > 0` → `false`
- بنابراین تابع Canonical و Trigger باید `NaN`، `Infinity` و `-Infinity` را صریحاً رد کنند.
- محدودیت ۸ رقم اعشار در Service وجود دارد، اما تابع Canonical به‌تنهایی آن را enforce نمی‌کند. Trigger باید برای `PARTIAL_RELEASE` متن Canonical را با Regex زیر بررسی کند:

```text
^(?:0|[1-9][0-9]*)(?:\.[0-9]{1,8})?$
```

و علاوه بر آن وجود حداقل یک رقم غیرصفر را بررسی کند.

### 3.3 INSERT مستقیم با Metadata ناقص یا Fingerprint نامعتبر

قطعاً رد می‌شود، مشروط به اینکه Trigger به‌درستی اعمال شود:

- هر سه ستون Metadata NULL یا ناقص → `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED`
- `fingerprint_version` خارج از `{1}` → `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED`
- `request_fingerprint` با فرمت نامعتبر → `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID`
- Fingerprint بازمحاسبه‌شده متفاوت → `ACCOUNT_HOLD_EVENT_FINGERPRINT_MISMATCH`
- Metadata کامل ولی Fingerprint محاسبه‌شده از مقادیر خام → پس از نرمال‌سازی Mismatch و رد
- Metadata کامل و Fingerprint Canonical → نرمال و پذیرفته می‌شود

تنها راه دور زدن، غیرفعال کردن Trigger توسط Roleهای ممتاز است که خارج از قرارداد برنامه است.

### 3.4 حالت‌های `effective_at_provided`

#### `false`

- Trigger مقدار `NEW.effective_at` را برابر `date_trunc('milliseconds', CURRENT_TIMESTAMP)` قرار می‌دهد.
- Fingerprint از `effectiveAt=""` استفاده می‌کند.
- Business Validation با مقدار پیش‌فرض انجام می‌شود.
- INSERT مستقیم با زمان دلخواه ولی Presence=false → زمان با Default جایگزین می‌شود.
- تناقضی با CHECK/NOT NULL ندارد، چون مقدار پیش‌فرض غیر NULL است.

#### `true`

- Trigger مقدار `NEW.effective_at := date_trunc('milliseconds', NEW.effective_at)` را انجام می‌دهد.
- اگر `NEW.effective_at` NULL باشد، رد می‌شود.
- Fingerprint از UTC میلی‌ثانیه استفاده می‌کند.
- Business Validation روی همان مقدار انجام می‌شود.
- مقدار صریح حفظ می‌شود؛ فقط دقت به میلی‌ثانیه یکسان می‌شود.

نتیجه: هر دو حالت بدون تناقض کار می‌کنند و Presence مرجع اصلی است.

### 3.5 ایرادهای احتمالی دیگر

1. **Volatility:** `hold_event_fingerprint_v1` نباید `IMMUTABLE` اعلام شود، چون `convert_to` و `to_char` در PostgreSQL STABLE هستند. برای Trigger مشکلی نیست، ولی برای Index/Generated Column آینده باید نسخه Immutable جداگانه طراحی شود.
2. **Varchar Length:** طول `varchar(n)` در زمان ورود نوع بررسی می‌شود؛ اگر مقدار خام از حد مجاز طولانی‌تر باشد، ممکن است قبل از نرمال‌سازی در Trigger رد شود. این با قرارداد Service سازگار است، چون Service هم طول را قبل از ذخیره بررسی می‌کند.
3. **Trigger Order آینده:** هر Trigger BEFORE INSERT جدید باید بعد از نرمال‌سازی اجرا شود یا نرمال‌سازی داخل همان Trigger موجود بماند.
4. **AFTER Trigger آینده:** اگر بعداً Trigger AFTER INSERT اضافه شود، نمی‌تواند مستقیماً `NEW` را تغییر دهد؛ ولی اگر UPDATE انجام دهد، `trg_account_hold_event_immutable` باید آن را مسدود کند.

---

## 4. سازگاری Legacy

### 4.1 اثر Trigger روی ردیف‌های قدیمی

- Trigger پیشنهادی فقط `BEFORE INSERT` است؛ روی ردیف‌های موجود اجرا نمی‌شود.
- افزودن ستون‌های Nullable و CHECKهای All-or-None/Format/Version، ردیف‌های موجود با NULL را تغییر نمی‌دهد و آن‌ها Constraint را پاس می‌کنند.
- هیچ UPDATE یا Backfill لازم نیست.
- `trg_account_hold_event_immutable` هر UPDATE را مسدود می‌کند؛ بنابراین Backfill حتی اگر خواسته شود، نیازمند رویه کنترل‌شده جداگانه است.

### 4.2 مسیر Fallback Legacy

- Detection: هر سه ستون `request_fingerprint`، `fingerprint_version` و `effective_at_provided` برابر NULL.
- Lookup: مقایسه فیلدبه‌فیلد فعلی با heuristic `effective_at = date_trunc('milliseconds', created_at)`.
- هیچ محاسبه/مقایسه Fingerprint برای Legacy انجام نمی‌شود.
- Retryهای Legacy بدون تغییر رفتار فعلی کار می‌کنند.
- اگر ردیف Legacy در آینده Backfill شود، می‌تواند به مسیر نسخه‌دار منتقل شود؛ اما در این مرحله لازم نیست.

### 4.3 محدودیت عملیاتی Restore

Trigger الزام‌کننده Metadata، Restore دامپ‌های قدیمی را که ردیف‌های Legacy با NULL دارند مسدود می‌کند. این موضوع ردیف‌های موجود را تغییر نمی‌دهد، ولی رویه Restore باید یکی از این‌ها باشد:

- ابتدا دامپ را در Schema قبل از Migration 018 Restore کنیم و سپس Migration را اعمال کنیم.
- یا Trigger را با تأیید و رویه کنترل‌شده موقتاً غیرفعال کنیم.
- یا Metadata را قبل از Restore آماده کنیم.

### 4.4 نتیجه Legacy

- هیچ تغییری در ردیف‌های قدیمی ایجاد نمی‌شود.
- هیچ Backfill یا UPDATE لازم نیست.
- Fallback فعلی حفظ می‌شود.
- Restore/Migration نیازمند رویه عملیاتی مستند است.

---

## 5. نتیجه نهایی هر چهار موضوع

1. **Trigger Order:** تنها یک Trigger کاربری BEFORE INSERT وجود دارد؛ AFTER INSERT فقط FK داخلی است و فیلدها را تغییر نمی‌دهد. نرمال‌سازی داخل همان Trigger باید در ابتدای تابع باشد.
2. **توابع:** سه تابع کامل پیشنهاد شدند. Trim و Amount و Fingerprint با Service قابل تطبیق‌اند؛ Test Vectorها تأیید شدند.
3. **ترتیب اعتبارسنجی:** نرمال‌سازی قبل از CHECK/NOT NULL و Business Validation انجام می‌شود. `NaN/Infinity` و محدودیت ۸ رقم اعشار باید صریحاً در Trigger مدیریت شوند.
4. **Legacy:** بدون تغییر، بدون Backfill، با Fallback فعلی. تنها محدودیت، رویه Restore دامپ‌های قدیمی است.

---

## Appendix A — تعریف کامل تابع فعلی `validate_account_hold_event`

منبع: `database/CURRENT_SCHEMA.sql:67-182`

```sql
CREATE FUNCTION public.validate_account_hold_event() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_hold account_hold%ROWTYPE;
    v_existing account_hold_events%ROWTYPE;
    v_released NUMERIC;
    v_remaining NUMERIC;
    v_now TIMESTAMPTZ;
BEGIN
    v_now := clock_timestamp();

    SELECT *
      INTO v_hold
      FROM account_hold
     WHERE id = NEW.hold_id
     FOR UPDATE;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_FOUND'
            USING ERRCODE = '23503';
    END IF;

    SELECT *
      INTO v_existing
      FROM account_hold_events
     WHERE hold_id = NEW.hold_id
       AND idempotency_key = NEW.idempotency_key;

    IF FOUND THEN
        IF v_existing.event_type = NEW.event_type
           AND v_existing.amount = NEW.amount
           AND v_existing.effective_at = NEW.effective_at
           AND v_existing.reference_type
               IS NOT DISTINCT FROM NEW.reference_type
           AND v_existing.reference_id
               IS NOT DISTINCT FROM NEW.reference_id
           AND v_existing.reason_code = NEW.reason_code
        THEN
            RETURN NULL;
        END IF;

        RAISE EXCEPTION 'ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED'
            USING ERRCODE = '23505';
    END IF;

    IF NEW.effective_at > v_now THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED'
            USING ERRCODE = '23514';
    END IF;

    IF v_now < v_hold.effective_at THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_YET_EFFECTIVE'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.effective_at < v_hold.effective_at THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.event_type = 'EXPIRY_RELEASE' THEN
        IF v_hold.expires_at IS NULL
           OR v_now < v_hold.expires_at THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_EXPIRED'
                USING ERRCODE = '23514';
        END IF;
    ELSE
        IF v_hold.expires_at IS NOT NULL
           AND v_now >= v_hold.expires_at THEN
            RAISE EXCEPTION 'EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE'
                USING ERRCODE = '23514';
        END IF;
    END IF;

    SELECT COALESCE(SUM(amount), 0)
      INTO v_released
      FROM account_hold_events
     WHERE hold_id = NEW.hold_id;

    v_remaining := v_hold.amount - v_released;

    IF v_remaining <= 0 THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_ALREADY_FULLY_RELEASED'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.amount > v_remaining THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.event_type = 'PARTIAL_RELEASE'
       AND NEW.amount >= v_remaining THEN
        RAISE EXCEPTION 'PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.event_type IN (
        'FULL_RELEASE',
        'EXPIRY_RELEASE',
        'CANCELLATION_RELEASE'
    ) AND NEW.amount <> v_remaining THEN
        RAISE EXCEPTION 'FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    RETURN NEW;
END;
$$;
```

---

## Appendix B — تعریف Triggerهای فعلی

منبع: `database/CURRENT_SCHEMA.sql:2190-2207`

```sql
CREATE TRIGGER trg_account_hold_event_immutable
    BEFORE DELETE OR UPDATE ON public.account_hold_events
    FOR EACH ROW
    EXECUTE FUNCTION public.prevent_account_hold_event_mutation();

CREATE TRIGGER trg_validate_account_hold_event
    BEFORE INSERT ON public.account_hold_events
    FOR EACH ROW
    EXECUTE FUNCTION public.validate_account_hold_event();
```

Trigger داخلی FK:

```sql
CREATE CONSTRAINT TRIGGER "RI_ConstraintTrigger_c_19493"
    AFTER INSERT ON public.account_hold_events
    FROM account_hold
    NOT DEFERRABLE INITIALLY IMMEDIATE
    FOR EACH ROW
    EXECUTE FUNCTION "RI_FKey_check_ins"();
```

---

## Appendix C — شواهد Read-only کلیدی

- `information_schema.columns`: `effective_at` و `created_at` هر دو `timestamp with time zone`، `NOT NULL`، `DEFAULT CURRENT_TIMESTAMP`.
- `pg_trigger`: چهار Trigger؛ فقط یک Trigger کاربری BEFORE INSERT.
- `account_hold_events`: ۱۳ ردیف؛ ۱۰ ردیف رابطه `effective_at = date_trunc('milliseconds', created_at)` را دارند و ۳ ردیف صریح (`8`، `10`، `12`) ندارند.
- `'NaN'::numeric > 0` → `true`
- `'Infinity'::numeric > 0` → `true`
- Test Vectorهای Fingerprint نسخه ۱ با Payload SQL یکسان تولید شدند.

هیچ داده‌ای تغییر نکرد و هیچ DDL/DML اجرا نشد.
