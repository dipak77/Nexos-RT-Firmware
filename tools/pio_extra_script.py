# PlatformIO pre-script: register LVGL + esp_lcd_gc9a01 as managed IDF components
# so the esp32s3_idf environment links the same libraries the ESP-IDF build uses.
Import("env")

# PlatformIO's espidf framework supports `idf_component.yml` dependency resolution
# automatically. We point the build at main/idf_component.yml by copying it to the
# project root so the component manager (run on first build) can fetch lvgl + gc9a01.
import os
from SCons.Script import DefaultEnvironment

try:
    env = DefaultEnvironment()
    proj = env.get("PROJECT_DIR")
    root_yml = os.path.join(proj, "idf_component.yml")
    main_yml = os.path.join(proj, "main", "idf_component.yml")
    if os.path.exists(main_yml) and not os.path.exists(root_yml):
        import shutil
        shutil.copyfile(main_yml, root_yml)
        print("[pio_extra_script] linked main/idf_component.yml -> idf_component.yml")
except Exception as e:
    print("[pio_extra_script] note:", e)
