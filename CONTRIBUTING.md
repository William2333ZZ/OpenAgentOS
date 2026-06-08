# Contributing to OpenAgentOS

Thank you for your interest in OpenAgentOS!

## Getting started

1. Fork the repository and clone your fork.
2. Install dependencies (see [README.md](README.md#前置依赖)).
3. Run the fast acceptance suite before opening a PR:

```bash
make check-v0.1-beta
make check-console-x86
```

## Pull requests

- Keep changes focused; one logical change per PR.
- Ensure relevant `make check-*` targets pass.
- Update documentation when behavior or ABI changes.
- Do not commit secrets (`.env`, API keys, `generated/`).

## Developer Certificate of Origin

By contributing, you agree that your contributions are made under the
Apache License 2.0 and you certify the DCO:

```
Developer Certificate of Origin
Version 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I have the right
    to submit it under the open source license indicated in the file; or
(b) The contribution is based upon previous work that, to the best of my knowledge,
    is covered under an appropriate open source license and I have the right
    under that license to submit that work with modifications; or
(c) The contribution was provided directly to me by some other person who
    certified (a), (b) or (c) and I have not modified it.
(d) I understand and agree that this project and the contribution are public
    and that a record of the contribution is maintained indefinitely.
```

## Code style

- Match surrounding C code style in `kernel/` and `user/`.
- Prefer minimal, focused diffs over large refactors.
- Add `make check-*` scripts for new user-visible features when practical.

## Questions

Open a [GitHub Discussion](https://github.com/William2333ZZ/OpenAgentOS/discussions) or file an issue.
