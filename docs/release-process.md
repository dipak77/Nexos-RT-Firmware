# Release

Semantic versioning MAJOR.MINOR.PATCH

Build metadata: FW, HW, SDK, Git commit, Build timestamp, ESP-IDF version.

Command: system version

Process: clang-format -> static analysis -> unit tests -> build -> size check -> artifact -> signed binary -> bootloader + partition + fw + OTA + checksums + release notes

Git: main + feature/* + fix/*, tags v0.1.0 ... v1.0.0
