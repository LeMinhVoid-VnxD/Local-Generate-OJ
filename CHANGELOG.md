# Changelog

## v2.2.2 (2026-07-11)
- Added `\lfloor`, `\rfloor`, `\lceil`, `\rceil` support in math rendering

## v2.2.1 (2026-07-11)
- Math transformations (superscript `^`, subscript `_`, LaTeX symbols) now apply globally
- Fixed broken formulas in AI hint text when `$...$` wrapping is missing
- Clean brace `{}` removal everywhere

## v2.2.0 (2026-07-11)
- AI Configuration section in Settings (provider, model, API key)
- Auto-save generate modal fields to localStorage
- Auto-restore generate modal from saved config
- Solution hint/code buttons read from saved config

## v2.1.0 (2026-07-11)
- `POST /api/solution/hint` — AI returns approach, algorithm, steps, key_insight
- `POST /api/solution/code` — AI returns C++ code, explanation, complexity
- "Hướng dẫn giải" and "Code giải" buttons in problem detail view
- Result display box with loading/error/success states

## v2.0.0 (2026-07-11)
- Restored AI generation feature (`POST /api/generate`)
- Support OpenAI, Claude, Gemini, DeepSeek providers
- `cf_rating` field added to Problem struct
- `$...$` math notation support in AI prompts
- `formatRichText` renders `$...$` with styled spans

## v1.4.0
- Compiler path configurable via `--g++` flag and Settings
- Frontend API URL configurable for GitHub Pages deployment

## v1.3.0
- GitHub sync: `--repo owner/repo` downloads problems at startup
- `--force` flag to re-download

## v1.2.0
- CF rating displayed in problem list and detail view
- Rating ranges: Easy (<1000) → Only AI (>3000)
- AI rating dropdown in Generate modal

## v1.1.0
- Installer downloaders (MSYS2, Dev-C++, CodeBlocks)
- WinHTTP GET for both GitHub sync and downloaders

## v1.0.0
- Initial release: local OJ server on port 8080
- C++ compilation and subtask scoring
- Codeforces-style dark theme frontend
- Random problem generator (15 types)
- Basic AI generation with Gemini
