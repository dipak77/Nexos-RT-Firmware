# OTA

Two slots ota_0, ota_1 + factory.

Flow:
Current -> Download update -> ota_1 -> Verify signature -> Restart -> Self-test -> PASS Accept / FAIL Rollback

Display shows:
FIRMWARE UPDATE 67% progress bar DO NOT POWER OFF

Use HTTPS OTA with certificate validation for production.
