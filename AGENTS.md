# AGENTS.md — Core Banking

## Project shape
- C++17 / Qt 5.15 (Core, Sql, Network) / PostgreSQL 15 backend.
- Layered: src/domain → src/application → src/infrastructure → src/web.
- Entrypoint: src/main.cpp. HTTP server + router in src/web/httpserver.*, src/web/httprouter.*.
- Full context, decisions, and rules: read docs/PROJECT_CONTEXT.md first. Historical test results: docs/PROJECT_HISTORY.md.
- Schema migrations: database/schema/NNN_*.sql (numbered, additive; never rewrite an applied migration).

## Build (exact commands)
Out-of-source build, separate from repo. Run:
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
If the build dir is missing or CMake cache is stale, run:
cmake -S /home/ahmad/projects/CoreBanking -B /home/ahmad/projects/build-CoreBanking-Desktop-Debug
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
Do not create a build/ directory inside the repo.

## Run / test
- Server runs on port 8080. Health check: curl -i http://localhost:8080/api/v1/health → {"status":"UP"}.
- There is no unit-test framework. Integration tests are done by calling the real HTTP API with curl and asserting JSON responses. Do not add temp test code to main.cpp.
- For financial changes, verify DB state too (psql), not just the HTTP response:
  psql -U corebanking -h localhost -d corebanking -W

## Database
- DB name: corebanking. User: corebanking. Always pass -U corebanking; the default OS role will fail.
- Migrations are applied manually in numeric order. Before adding one, check that it is additive and compatible with already-applied schemas.
- Known conflict: schema 007_balances.sql enforces non-negative balances; overdraft (schema 014) may need an additive migration, not a rewrite.

## Conventions that an agent would otherwise get wrong
- When proposing a file change, output the full file content, not a fragment or diff-only. The user applies files manually.
- Read the current file from GitHub before proposing a replacement. The user does not grant local-FS access; use raw GitHub URLs under https://raw.githubusercontent.com/ahmadbabayi/Core-Banking/refs/heads/main/
- Do not re-open settled design decisions listed in docs/PROJECT_CONTEXT.md section 4 unless you find a real contradiction in code.
- Keep commits separated by concern: code / schema / docs in separate commits.

## Current known gaps (do not assume these are done)
- Schema 015_account_holds.sql is untracked, unapplied, and unapproved. Open issues: idempotency, concurrent release serialization, reliable last-event ordering, view/trigger validation on real PostgreSQL.
- idempotency and reconciliation are specified but not fully implemented/tested.
- Financial end-to-end integration test (transfer → journal → balances) is still pending.

## Model / tool caveat (OpenCode)
- deepseek-reasoner currently breaks tool-calling in OpenCode (missing reasoning_content). Prefer deepseek-chat or another non-reasoning model for agent loops.
- Some gateways (e.g. GapGPT free tier) may not expose Tool Calling; if the agent only chats but cannot edit files, that is the cause.
