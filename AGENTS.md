# AGENTS.md — CoreBanking

## 1. Mandatory project context
- Before starting any task, read `docs/PROJECT_CONTEXT.md` and `docs/PROJECT_HISTORY.md` completely enough to understand the current architecture, settled decisions, implemented services, and outstanding work.
- Treat those two files as the project's source of truth. Also inspect the relevant current source files and schema snapshot before proposing or implementing changes.
- Do not revive settled decisions or mark completed work as pending. When sources conflict, stop and explain the discrepancy instead of guessing.
- Work from the local repository. Do not use GitHub raw files as a substitute for the checked-out project files.

## 2. Development workflow — one service per stage
- Work on exactly one service or one explicitly approved, narrowly scoped task at a time.
- Before editing, summarize the intended scope, relevant files, dependencies, and test plan. Do not begin another service while the current stage is unfinished.
- Complete implementation, build, API tests, database read-back checks, and documentation updates for the current stage.
- At the end of the stage, report changed files, behavior changed, build result, API requests made, created business identifiers, database verification, and any remaining issues.
- Then STOP. Do not start the next service until the user explicitly approves it.

## 3. Database permissions and schema-change boundary
- The development database is `corebanking`. It is a development/test database and is not used by a real bank or for live customer operations.
- For direct SQL inspection, use only the PostgreSQL role `corebanking_reader`, which is intended to have read-only access. Connect with `psql -h localhost -p 5432 -U corebanking_reader -d corebanking` and rely on the user's `~/.pgpass`; never ask to display, read, copy, or print that file or any password.
- Direct SQL is for reading schema and data only. Use `SELECT` and read-only catalog/information-schema queries. Never attempt direct `INSERT`, `UPDATE`, `DELETE`, `TRUNCATE`, DML through functions, or other database writes.
- Never connect directly as `corebanking`, `postgres`, or another privileged database role. Never use `sudo -u postgres`, `su`, or another privilege escalation route.
- Never execute or apply migrations. Never run DDL such as `CREATE`, `ALTER`, `DROP`, `COMMENT ON`, `GRANT`, `REVOKE`, `CREATE INDEX`, or schema-changing commands. Never run migration scripts through `psql`, the application, or another tool.
- If a service requires a table, column, relation, index, constraint, trigger, function, view, extension, or migration change, STOP before implementing code that depends on it. Explain the required schema change and provide the proposed SQL or migration plan for the user to review and apply manually. Resume only after the user confirms the schema change is applied.
- The model may inspect the schema and verify database state using `corebanking_reader`; schema changes and migration execution are always performed by the user.

## 4. API integration tests and development data
- The application API is the authorized write path for development/testing. It is explicitly permitted to create customers, open accounts, make deposits, execute internal transfers, and perform other business operations through the real local API when needed for the approved stage.
- Use the real API workflows, validation rules, identifier generators, account-number generators, transaction logic, and business rules. Do not invent fixed IDs or bypass business logic to make tests easier.
- Use synthetic/fictitious customer information, but otherwise realistic banking data and operation sequences. The project is not in production; realistic development records and transactions are expected in the development database.
- Target only the local development server, normally `http://localhost:8080`. Never send test writes to a remote or production host. Verify the base URL before any write request.
- Record the returned customer/account/transaction identifiers and use read-only SQL queries to verify persisted results and financial consistency.
- Do not directly modify or delete test records with SQL. Do not delete or reverse API-created records unless the approved test plan explicitly requires it and the relevant operation is performed through the API.
- Keep a clear record of test data created during each stage so the user knows what remains in the development database.

## 5. Build and test environment
- Use the existing Qt Creator build directory only: `/home/ahmad/projects/build-CoreBanking-Desktop-Debug`.
- The existing build uses CMake, `Unix Makefiles`, and `Debug`. Do not create a second build directory or an in-repository `build/` directory.
- Normal build command:
  `cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2`
- Do not run `cmake -S`, reconfigure CMake, change the Qt kit, change build type, or alter the build directory without first explaining why and obtaining user approval.
- Run the application and API integration tests against the local development instance. Do not add temporary test code to `src/main.cpp`.
- A successful compile is not a substitute for API tests. Verify relevant API responses and use `corebanking_reader` to check persisted state.

## 6. Git boundaries
- Inspect Git status and diffs as needed, but do not run `git add`, `git commit`, `git push`, or commands that change branches, reset, clean, merge, or rebase.
- Do not stage files. The user reviews the final diff and performs all staging, commits, and pushes.
- At the end of each stage, provide a concise list of changed files and a suggested commit message, but do not execute it.

## 7. Documentation is part of every completed stage
- After each completed service/stage, update both `docs/PROJECT_CONTEXT.md` and `docs/PROJECT_HISTORY.md`.
- `PROJECT_CONTEXT.md` should reflect the current architecture, implementation status, API behavior, important decisions, and remaining work.
- `PROJECT_HISTORY.md` should record the completed stage, files changed, build/test results, API operations performed, generated business identifiers, verified database results, and known limitations.
- Do not claim a test passed unless its command was actually run and its output checked. Distinguish implemented, built, API-tested, and database-verified status.
- Include the two documentation files in the final diff report for the stage.

## 8. User confirmation and reporting
- The user performs all database schema changes and all Git staging/commits/pushes.
- Ask for approval before moving to a new service, expanding the approved scope, applying a schema-dependent change, reconfiguring the build, or running an operation outside the approved local test plan.
- Do not interpret permission to test through the API as permission to change the database schema directly.
- Never run OpenCode with `--auto` for this project. Keep interactive approvals enabled.
