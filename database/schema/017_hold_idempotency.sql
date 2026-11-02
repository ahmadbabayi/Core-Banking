
BEGIN;

-- ============================================================
-- 017: Add hold creation idempotency
-- ============================================================

ALTER TABLE account_hold
  ADD COLUMN idempotency_key VARCHAR(100),
  ADD COLUMN request_fingerprint VARCHAR(64);

ALTER TABLE account_hold
  ADD CONSTRAINT ck_hold_idempotency_pair
    CHECK (
      (idempotency_key IS NULL AND request_fingerprint IS NULL)
      OR
      (idempotency_key IS NOT NULL AND request_fingerprint IS NOT NULL)
    );

ALTER TABLE account_hold
  ADD CONSTRAINT ck_hold_idempotency_key_length
    CHECK (
      idempotency_key IS NULL
      OR LENGTH(BTRIM(idempotency_key)) BETWEEN 1 AND 100
    );

ALTER TABLE account_hold
  ADD CONSTRAINT ck_hold_fingerprint_length
    CHECK (
      request_fingerprint IS NULL
      OR LENGTH(request_fingerprint) = 64
    );

CREATE UNIQUE INDEX uq_hold_api_idempotency
  ON account_hold (idempotency_key)
  WHERE idempotency_key IS NOT NULL;

COMMIT;

