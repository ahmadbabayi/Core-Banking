BEGIN;

-- Refuse the migration if city codes are already duplicated.
DO $$
BEGIN
    IF EXISTS (
        SELECT 1
        FROM city
        GROUP BY code
        HAVING COUNT(*) > 1
    ) THEN
        RAISE EXCEPTION
            'Cannot enforce global city-code uniqueness: duplicate codes exist';
    END IF;
END
$$;

-- Remove the old province-scoped uniqueness constraint.
ALTER TABLE city
    DROP CONSTRAINT IF EXISTS uq_city_province_code;

-- Enforce uniqueness across all provinces.
ALTER TABLE city
    ADD CONSTRAINT uq_city_code UNIQUE (code);

COMMIT;
