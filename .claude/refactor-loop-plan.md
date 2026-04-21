# Refactor Loop Plan

This document is a complete, self-contained runbook for iteratively verifying and fixing the ESP-IDF safemode firmware rewrite. A fresh Claude instance should be able to read this file and execute the loop without any other context.

## Project Context

ESP32 safemode recovery firmware. Rewritten from PlatformIO/Arduino to ESP-IDF 6.0 component architecture.

- **Source of truth for functionality**: The design spec at `docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md`
- **Build command**: `python scripts/build.py`
- **Namespace**: `safemode::`
- **Language**: C++20, pure ESP-IDF (no Arduino, no PlatformIO)
- **Hardware**: ESP32-WROOM-32E-N8R2 (dual-core Xtensa LX6 @ 240MHz, 8MB flash, 2MB PSRAM)
- **Code style**: PascalCase classes, camelCase methods, `kConstantName` constants, Allman braces

## Component Layout

| Component | Purpose |
|-----------|---------|
| `components/ota/` | OtaUpdater: esp_ota_begin/write/end streaming OTA |
| `components/wifi/` | WiFi AP, DNS captive portal, HTTP server, web assets |
| `main/main.cpp` | Wires NVS, WiFi, DNS, OTA, HTTP together |
| `frontend/` | React 19 + Tailwind v4 SPA |

## The Loop

### Loop Structure

Repeat until Final Verification confirms REFACTOR_TRULY_COMPLETE:

```
ITERATION N:
  Step 1: PLAN          — Opus agent audits current state vs spec, writes .claude/refactor-plan.md
  Step 1b: VERIFY       — (only on REFACTOR_COMPLETE) Fresh Opus agent independently verifies
  Step 2: EXECUTE       — Opus agent implements the plan
  Step 3: BUILD         — python scripts/build.py, fix if needed
  Step 4: COMMIT        — Auto-commit
  Step 5: UPDATE DOCS   — Update CLAUDE.md, .claude/memory/ if needed
```

### Step 1: PLAN

Spawn an agent with `subagent_type: "general-purpose"` and `model: "opus"`. Research only — writes `.claude/refactor-plan.md`, no code changes.

**Agent prompt:**

```
You are auditing an ESP32 safemode firmware rewrite. Compare the current implementation against the design spec to find any missing functionality.

Read the design spec at docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md completely.

Then audit the current implementation:
1. Read every file in firmware/components/ota/
2. Read every file in firmware/components/wifi/
3. Read firmware/main/main.cpp
4. Read every file in firmware/frontend/src/
5. Check that scripts/ contains all required scripts and they match the spec
6. Verify partitions.csv and sdkconfig.defaults match the spec

For each spec requirement, verify the implementation exists and is correct.

## Output

### If you find gaps:
Write a plan to .claude/refactor-plan.md listing each gap with:
- What's missing (cite spec section)
- Which files to modify/create
- Implementation steps

### If everything matches the spec:
Write REFACTOR_COMPLETE to .claude/refactor-plan.md

## Rules
- Actually READ the code and the spec
- Only write .claude/refactor-plan.md
- Be specific — cite file paths and function names
- Cosmetic differences are NOT gaps
```

**After:** Read `.claude/refactor-plan.md`. If `REFACTOR_COMPLETE`, proceed to Step 1b. Otherwise proceed to Step 2.

### Step 1b: FINAL VERIFICATION

Spawn a fresh agent with `subagent_type: "general-purpose"` and `model: "opus"`.

**Agent prompt:**

```
You are a FINAL VERIFICATION agent for a firmware rewrite. A previous agent declared the rewrite complete. Independently verify this.

Read the design spec at docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md.
Read ALL implementation files in firmware/components/, firmware/main/, firmware/frontend/src/, and scripts/.

Verify every spec requirement is implemented. Check:
- All API endpoints exist with correct methods and response formats
- WiFi AP config matches spec (SSID, password, IP)
- DNS server resolves to correct IP
- OTA updater uses esp_ota_begin/write/end (not raw flash ops)
- Frontend has all UI elements described in spec
- Build scripts match spec (no encryption, correct addresses)
- sdkconfig.defaults has all required settings

### If gaps found:
Write plan to .claude/refactor-plan.md with GAPS_FOUND header.

### If everything verified:
Write REFACTOR_TRULY_COMPLETE to .claude/refactor-plan.md.
```

### Step 2: EXECUTE

Spawn agent with `subagent_type: "general-purpose"` and `model: "opus"`.

**Agent prompt:**

```
Read .claude/refactor-plan.md and implement the changes described.

Context:
- ESP-IDF 6.0, C++20, safemode:: namespace
- Allman braces, PascalCase classes, camelCase methods, kConstant names
- No Arduino, no PlatformIO
- Do NOT build, commit, or modify docs

Follow the plan precisely.
```

### Step 3: BUILD

```bash
cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py 2>&1 | tail -100
```

If build fails, spawn a fix agent with the errors. Max 3 retries.

### Step 4: COMMIT

```bash
git add firmware/ scripts/
git commit -m "refactor: [description from plan]

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

### Step 5: UPDATE DOCS

Update CLAUDE.md and .claude/memory/ if changes affect documented architecture.

## Completion Checklist

**Infrastructure:**
- [x] PlatformIO files removed
- [x] Arduino dependencies removed
- [x] Python scripts/ created and functional
- [x] CMakeLists.txt files created
- [x] sdkconfig.defaults created
- [x] partitions.csv preserved

**Components:**
- [x] WiFi AP (SSID "SAFEMODE", pass "safemode", IP 4.3.2.1)
- [x] DNS captive portal (all queries -> 4.3.2.1)
- [x] HTTP server with static file serving + SPA fallback
- [x] API routes (ping, restart, app, update, info)
- [x] CORS middleware
- [x] web_assets.h / web_assets.cmake integration
- [x] OtaUpdater with begin/write/finish/abort
- [x] main.cpp wires everything together

**Frontend:**
- [x] React 19 + Tailwind v4 + Vite 8
- [x] OTA upload with progress
- [x] Leave Safemode with confirmation
- [x] Restart button
- [x] Connectivity indicator
- [x] Device info section
- [x] Builds and embeds via build_frontend.py

**Integration:**
- [x] python scripts/build.py succeeds
- [x] Binary fits in safemode partition (1.7MB)
- [x] Old code fully removed

## Completion Criteria

1. Planner declares REFACTOR_COMPLETE
2. Final Verification confirms REFACTOR_TRULY_COMPLETE
3. Functional parity with old firmware
4. Firmware builds and fits in partition
5. No TODO stubs remaining
