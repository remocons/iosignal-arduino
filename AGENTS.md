# Public Arduino distribution

- Develop product changes in ../_iosignal-arduino, then use its promotion commands.
- Never copy private directories, examples, npm files, agent-work or Git history here.
- scripts/public-files.json is an exact allowlist. New public files need explicit review; no wildcards.
- Maintain README, LICENSE and library.properties/version independently here.
- Before release run python3 scripts/verify.py and, in ../_iosignal-arduino, npm run public:verify.
- Review the full public diff for confidential content; the scope audit does not detect arbitrary secrets in approved files.
- Build representative Arduino boards and test device communication before release.
- Do not push, tag or publish without an explicit user request.
- Detailed agent-assisted work records belong in ../_iosignal-arduino/agent-work only.
