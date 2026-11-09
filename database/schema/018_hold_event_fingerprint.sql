BEGIN;

-- ============================================================
-- 018: Versioned request fingerprint for account_hold_events
--
-- Scope:
--   - Add nullable idempotency metadata to hold events.
--   - Keep all existing rows untouched as Legacy rows.
--   - Store a versioned canonical fingerprint for every new event.
--   - Preserve the existing unique index and immutability trigger.
--
-- Legacy compatibility:
--   Existing rows keep request_fingerprint, fingerprint_version and
--   effective_at_provided as NULL. No UPDATE and no backfill is performed.
--   The validation trigger detects that all three columns are NULL and
--   falls back to the legacy field-by-field comparison, including the
--   legacy effective_at heuristic:
--       effective_at = date_trunc('milliseconds', created_at)
--
-- Deployment note:
--   This migration is not compatible with the old C++ service as-is: the
--   new validation trigger requires metadata on every new insert. Apply
--   this migration together with the matching service deployment, or in a
--   controlled maintenance window. The old service must not continue
--   inserting events after this migration is applied.
--
-- Restore note:
--   Because every new insert must carry metadata, restoring a pre-018 dump
--   directly into the post-018 schema will fail for Legacy rows whose new
--   columns are NULL. Use one of these approved procedures:
--     1. Restore the pre-018 dump into a pre-018 schema, then apply 018.
--     2. Temporarily disable trg_validate_account_hold_event during the
--        restore, then re-enable it immediately after the restore.
--     3. Prepare valid metadata before the restore.
--   Do not disable trg_account_hold_event_immutable in normal operation.
--
-- Versioning:
--   fingerprint_version = 1 is the only supported version in this
--   migration. Unknown versions are rejected by the trigger and by the
--   dispatcher below. A future algorithm version requires a new migration
--   that adds its canonicalizer and updates the allowed-version checks.
-- ============================================================

ALTER TABLE account_hold_events
  ADD COLUMN request_fingerprint VARCHAR(64),
  ADD COLUMN fingerprint_version SMALLINT,
  ADD COLUMN effective_at_provided BOOLEAN;

-- All three metadata columns must be NULL together (Legacy) or NOT NULL
-- together (new versioned event).
ALTER TABLE account_hold_events
  ADD CONSTRAINT ck_account_hold_event_idempotency_metadata
    CHECK (
      (request_fingerprint IS NULL
       AND fingerprint_version IS NULL
       AND effective_at_provided IS NULL)
      OR
      (request_fingerprint IS NOT NULL
       AND fingerprint_version IS NOT NULL
       AND effective_at_provided IS NOT NULL)
    );

-- Fingerprints are lowercase SHA-256 hex strings.
ALTER TABLE account_hold_events
  ADD CONSTRAINT ck_account_hold_event_fingerprint_format
    CHECK (
      request_fingerprint IS NULL
      OR request_fingerprint ~ '^[0-9a-f]{64}$'
    );

-- Only version 1 is supported by this migration.
ALTER TABLE account_hold_events
  ADD CONSTRAINT ck_account_hold_event_fingerprint_version
    CHECK (
      fingerprint_version IS NULL
      OR fingerprint_version = 1
    );


-- ============================================================
-- V1 canonical text normalization.
--
-- The character set is the Unicode White_Space set frozen by the V1
-- contract. It is intentionally explicit so the result does not depend on
-- the database locale or on a Qt version.
--
-- U+0009 U+000A U+000B U+000C U+000D
-- U+0020 U+0085 U+00A0 U+1680
-- U+2000..U+200A
-- U+2028 U+2029 U+202F U+205F U+3000
-- ============================================================

CREATE OR REPLACE FUNCTION hold_event_fp_trim(p_value TEXT)
RETURNS TEXT
LANGUAGE sql
IMMUTABLE
STRICT
AS $function$
    SELECT btrim(
        p_value,
        U&'\0009\000A\000B\000C\000D\0020\0085\00A0\1680\2000\2001\2002\2003\2004\2005\2006\2007\2008\2009\200A\2028\2029\202F\205F\3000'
    );
$function$;


-- ============================================================
-- V1 canonical amount.
--
-- Converts a finite numeric value to a canonical decimal representation:
--   - no unnecessary trailing fractional zeros
--   - no trailing decimal point
--   - no leading plus sign
--   - no exponent notation
--
-- NaN, Infinity and -Infinity are rejected. Positivity and the maximum of
-- eight fractional digits are enforced by the validation trigger because
-- this helper only canonicalizes the representation.
-- ============================================================

CREATE OR REPLACE FUNCTION hold_event_canonical_amount(p_amount NUMERIC)
RETURNS NUMERIC
LANGUAGE plpgsql
IMMUTABLE
STRICT
AS $function$
DECLARE
    v_text TEXT;
BEGIN
    v_text := p_amount::text;

    IF v_text IN ('NaN', 'Infinity', '-Infinity') THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    IF position('.' IN v_text) > 0 THEN
        v_text := regexp_replace(v_text, '0+$', '');
        v_text := regexp_replace(v_text, '[.]$', '');
    END IF;

    RETURN v_text::numeric;
END;
$function$;


-- ============================================================
-- V1 canonical fingerprint payload.
--
-- Field order:
--   HOLD_EVENT_FP
--   1
--   holdId=<decimal>
--   eventType=<byteLength>:<value>
--   amountProvided=<0|1>
--   amount=<byteLength>:<value>
--   effectiveAtProvided=<0|1>
--   effectiveAt=<byteLength>:<value>
--   reasonCode=<byteLength>:<value>
--   referenceType=<byteLength>:<value>
--   referenceId=<byteLength>:<value>
--
-- Every line is terminated with a newline, including the last one.
-- Lengths are UTF-8 byte lengths. The function is STABLE, not IMMUTABLE,
-- because convert_to() and to_char() are STABLE in PostgreSQL. It is used
-- by the trigger and is not intended as an index expression.
-- ============================================================

CREATE OR REPLACE FUNCTION hold_event_fingerprint_v1(
    p_hold_id BIGINT,
    p_event_type TEXT,
    p_amount NUMERIC,
    p_effective_at_provided BOOLEAN,
    p_effective_at TIMESTAMPTZ,
    p_reason_code TEXT,
    p_reference_type TEXT,
    p_reference_id TEXT
) RETURNS TEXT
LANGUAGE plpgsql
STABLE
AS $function$
DECLARE
    v_event_type TEXT;
    v_reason_code TEXT;
    v_reference_type TEXT;
    v_reference_id TEXT;
    v_amount_text TEXT;
    v_amount_provided TEXT;
    v_effective_text TEXT;
    v_payload TEXT;
BEGIN
    IF p_effective_at_provided IS NULL THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID'
            USING ERRCODE = '23514';
    END IF;

    v_event_type := hold_event_fp_trim(p_event_type);
    v_reason_code := hold_event_fp_trim(p_reason_code);
    v_reference_type := NULLIF(hold_event_fp_trim(p_reference_type), '');
    v_reference_id := NULLIF(hold_event_fp_trim(p_reference_id), '');

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
        v_amount_text := hold_event_canonical_amount(p_amount)::text;
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
        'eventType=' ||
            octet_length(convert_to(v_event_type, 'UTF8'))::text ||
            ':' || v_event_type || chr(10) ||
        'amountProvided=' || v_amount_provided || chr(10) ||
        'amount=' ||
            octet_length(convert_to(v_amount_text, 'UTF8'))::text ||
            ':' || v_amount_text || chr(10) ||
        'effectiveAtProvided=' ||
            CASE WHEN p_effective_at_provided THEN '1' ELSE '0' END ||
            chr(10) ||
        'effectiveAt=' ||
            octet_length(convert_to(v_effective_text, 'UTF8'))::text ||
            ':' || v_effective_text || chr(10) ||
        'reasonCode=' ||
            octet_length(convert_to(v_reason_code, 'UTF8'))::text ||
            ':' || v_reason_code || chr(10) ||
        'referenceType=' ||
            octet_length(convert_to(COALESCE(v_reference_type, ''), 'UTF8'))::text ||
            ':' || COALESCE(v_reference_type, '') || chr(10) ||
        'referenceId=' ||
            octet_length(convert_to(COALESCE(v_reference_id, ''), 'UTF8'))::text ||
            ':' || COALESCE(v_reference_id, '');

    -- Trailing newline after the last field.
    v_payload := v_payload || chr(10);

    RETURN encode(sha256(convert_to(v_payload, 'UTF8')), 'hex');
END;
$function$;


-- ============================================================
-- Version dispatcher.
--
-- Unknown versions fail closed. Future versions must add a branch here
-- and a corresponding canonicalizer in a new migration.
-- ============================================================

CREATE OR REPLACE FUNCTION hold_event_fingerprint(
    p_version SMALLINT,
    p_hold_id BIGINT,
    p_event_type TEXT,
    p_amount NUMERIC,
    p_effective_at_provided BOOLEAN,
    p_effective_at TIMESTAMPTZ,
    p_reason_code TEXT,
    p_reference_type TEXT,
    p_reference_id TEXT
) RETURNS TEXT
LANGUAGE plpgsql
STABLE
AS $function$
BEGIN
    IF p_version = 1 THEN
        RETURN hold_event_fingerprint_v1(
            p_hold_id,
            p_event_type,
            p_amount,
            p_effective_at_provided,
            p_effective_at,
            p_reason_code,
            p_reference_type,
            p_reference_id
        );
    END IF;

    RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED'
        USING ERRCODE = '0A000';
END;
$function$;


-- ============================================================
-- Validate release events and handle idempotency.
--
-- This replaces the existing function body while preserving all current
-- business validations. The immutability trigger is not touched.
--
-- Order:
--   1. Require complete metadata on every new insert.
--   2. Validate version/format.
--   3. Normalize text, effective_at and PARTIAL amount.
--   4. Recompute the fingerprint from normalized values.
--   5. Lock the hold.
--   5a. Capture v_now with clock_timestamp() after the lock is held.
--   6. Check idempotency before business validation.
--      6a. Complete versioned row: compare version + fingerprint.
--      6b. Legacy row (all three metadata columns NULL): compare fields
--          and use the legacy effective_at heuristic.
--      6c. Partially populated metadata: fail closed.
--   7. Run the existing business validation.
-- ============================================================

CREATE OR REPLACE FUNCTION validate_account_hold_event()
RETURNS TRIGGER
LANGUAGE plpgsql
AS $function$
DECLARE
    v_hold account_hold%ROWTYPE;
    v_existing account_hold_events%ROWTYPE;
    v_released NUMERIC;
    v_remaining NUMERIC;
    v_now TIMESTAMPTZ;
    v_amount_text TEXT;
    v_expected_fingerprint TEXT;
    v_existing_is_legacy BOOLEAN;
BEGIN
    -- --------------------------------------------------------
    -- 1. New events must carry complete idempotency metadata.
    -- --------------------------------------------------------
    IF NEW.request_fingerprint IS NULL
       OR NEW.fingerprint_version IS NULL
       OR NEW.effective_at_provided IS NULL THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.request_fingerprint !~ '^[0-9a-f]{64}$' THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.fingerprint_version <> 1 THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED'
            USING ERRCODE = '0A000';
    END IF;

    -- --------------------------------------------------------
    -- 2. Normalize textual fields before fingerprint/business checks.
    -- --------------------------------------------------------
    NEW.event_type := hold_event_fp_trim(NEW.event_type);
    NEW.reason_code := hold_event_fp_trim(NEW.reason_code);
    NEW.reference_type := NULLIF(hold_event_fp_trim(NEW.reference_type), '');
    NEW.reference_id := NULLIF(hold_event_fp_trim(NEW.reference_id), '');

    IF NEW.event_type IS NULL
       OR NEW.event_type NOT IN (
            'PARTIAL_RELEASE',
            'FULL_RELEASE',
            'EXPIRY_RELEASE',
            'CANCELLATION_RELEASE'
       ) THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.reason_code IS NULL OR NEW.reason_code = '' THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_REASON_CODE'
            USING ERRCODE = '23514';
    END IF;

    -- --------------------------------------------------------
    -- 3. Normalize effective_at.
    --    false: use the transaction default, not created_at and not
    --           clock_timestamp.
    --    true:  preserve the client timestamp, truncated to milliseconds.
    -- --------------------------------------------------------
    IF NEW.effective_at_provided THEN
        IF NEW.effective_at IS NULL THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT'
                USING ERRCODE = '23514';
        END IF;

        NEW.effective_at := date_trunc('milliseconds', NEW.effective_at);
    ELSE
        NEW.effective_at := date_trunc('milliseconds', CURRENT_TIMESTAMP);
    END IF;

    -- --------------------------------------------------------
    -- 4. Normalize and validate the amount.
    --    Only PARTIAL_RELEASE contributes amount to the fingerprint.
    --
    --    account_hold_events.amount is unconstrained NUMERIC
    --    (atttypmod = -1), so PostgreSQL parses the input exactly and
    --    does not round it before this trigger. Keep it unconstrained.
    --    If a future migration introduces NUMERIC(p,s), PostgreSQL can
    --    round the input before this trigger and the original precision
    --    would no longer be detectable here.
    -- --------------------------------------------------------
    IF NEW.event_type = 'PARTIAL_RELEASE' THEN
        IF NEW.amount IS NULL THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
                USING ERRCODE = '23514';
        END IF;

        v_amount_text := hold_event_canonical_amount(NEW.amount)::text;

        IF v_amount_text !~ '^(0|[1-9][0-9]*)([.][0-9]{1,8})?$'
           OR v_amount_text !~ '[1-9]' THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
                USING ERRCODE = '23514';
        END IF;

        NEW.amount := v_amount_text::numeric;
    ELSE
        IF NEW.amount IS NULL
           OR NEW.amount::text IN ('NaN', 'Infinity', '-Infinity') THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
                USING ERRCODE = '23514';
        END IF;

        IF NEW.amount <= 0 THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_AMOUNT'
                USING ERRCODE = '23514';
        END IF;
    END IF;

    -- --------------------------------------------------------
    -- 5. Recompute the fingerprint from the normalized values.
    -- --------------------------------------------------------
    v_expected_fingerprint := hold_event_fingerprint(
        NEW.fingerprint_version,
        NEW.hold_id,
        NEW.event_type,
        NEW.amount,
        NEW.effective_at_provided,
        NEW.effective_at,
        NEW.reason_code,
        NEW.reference_type,
        NEW.reference_id
    );

    IF NEW.request_fingerprint IS DISTINCT FROM v_expected_fingerprint THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_MISMATCH'
            USING ERRCODE = '23514';
    END IF;

    -- --------------------------------------------------------
    -- 6. Lock the hold.
    -- --------------------------------------------------------
    SELECT *
      INTO v_hold
      FROM account_hold
     WHERE id = NEW.hold_id
     FOR UPDATE;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_FOUND'
            USING ERRCODE = '23503';
    END IF;

    -- Capture business-validation time only after the hold lock is held.
    -- This keeps expiry/effective-time checks based on the post-lock clock.
    v_now := clock_timestamp();

    -- --------------------------------------------------------
    -- 7. Idempotency before release validation.
    -- --------------------------------------------------------
    SELECT *
      INTO v_existing
      FROM account_hold_events
     WHERE hold_id = NEW.hold_id
       AND idempotency_key = NEW.idempotency_key;

    IF FOUND THEN
        v_existing_is_legacy := (
            v_existing.request_fingerprint IS NULL
            AND v_existing.fingerprint_version IS NULL
            AND v_existing.effective_at_provided IS NULL
        );

        -- Partially populated metadata is invalid data; fail closed.
        IF NOT v_existing_is_legacy
           AND (
               v_existing.request_fingerprint IS NULL
               OR v_existing.fingerprint_version IS NULL
               OR v_existing.effective_at_provided IS NULL
           ) THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID'
                USING ERRCODE = '23514';
        END IF;

        IF v_existing_is_legacy THEN
            -- Legacy rows have no fingerprint. Preserve the previous
            -- field-by-field comparison and the legacy time heuristic.
            IF v_existing.event_type = NEW.event_type
               AND v_existing.amount = NEW.amount
               AND v_existing.reference_type
                   IS NOT DISTINCT FROM NEW.reference_type
               AND v_existing.reference_id
                   IS NOT DISTINCT FROM NEW.reference_id
               AND v_existing.reason_code = NEW.reason_code
               AND (
                    (
                        NEW.effective_at_provided = FALSE
                        AND v_existing.effective_at =
                            date_trunc('milliseconds', v_existing.created_at)
                    )
                    OR
                    (
                        NEW.effective_at_provided = TRUE
                        AND v_existing.effective_at = NEW.effective_at
                        AND v_existing.effective_at <>
                            date_trunc('milliseconds', v_existing.created_at)
                    )
               ) THEN
                -- Exact Legacy retry: do not insert a duplicate event.
                RETURN NULL;
            END IF;
        ELSE
            -- Versioned rows compare only the stored version and the
            -- fingerprint. Never compare fingerprints across versions.
            IF v_existing.fingerprint_version = NEW.fingerprint_version
               AND v_existing.request_fingerprint
                   IS NOT DISTINCT FROM NEW.request_fingerprint THEN
                -- Exact retry: do not insert a duplicate event.
                RETURN NULL;
            END IF;
        END IF;

        RAISE EXCEPTION 'ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED'
            USING ERRCODE = '23505';
    END IF;

    -- --------------------------------------------------------
    -- 8. Existing business validation (unchanged semantics).
    -- --------------------------------------------------------
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
$function$;

-- The existing trigger bindings are intentionally left unchanged:
--   trg_validate_account_hold_event  BEFORE INSERT
--   trg_account_hold_event_immutable BEFORE DELETE OR UPDATE

COMMIT;
