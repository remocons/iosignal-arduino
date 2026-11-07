# Public Arduino distribution

- Develop product changes in ../_iosignal-remocon-arduino (iosignal/), then use its promotion commands.
- Never copy private directories, examples, npm files, private docs or Git history here.
- scripts/public-files.json is an exact allowlist. New public files need explicit review; no wildcards.
- Maintain README, LICENSE and library.properties/version independently here. The version matches the public iosignal release (from 9.0.0), together with iosignal-cli, remocon and remocon-arduino.
- Before release run python3 scripts/verify.py and, in ../_iosignal-remocon-arduino, npm run iosignal:verify.
- Review the full public diff for confidential content; the scope audit does not detect arbitrary secrets in approved files.
- Build representative Arduino boards and test device communication before release.
- Do not push, tag or publish without an explicit user request.
- Detailed agent-assisted work records belong in ../_iosignal-remocon-arduino/docs/logs only.

- Keep user documentation only in the public repository: README.md (Korean) and README.en.md (English), with reciprocal language links. Both are public-local policy entries, never synced from private. Do not recreate a private root README.
