Import("env")
from os.path import join

proj = env["PROJECT_DIR"]
mk = join(proj, "components", "microkernel")
env.Append(CPPPATH=[join(mk, "include"), join(mk, "arch", "esp32s3")])
mk_lib = env.BuildLibrary(
    join("$BUILD_DIR", "microkernel"),
    mk,
    src_filter=[
        "+<core/*.c>",
        "+<ipc/*.c>",
        "+<time/*.c>",
        "+<memory/*.c>",
        "+<arch/esp32s3/*.c>",
    ],
)
env.Prepend(LIBS=[mk_lib])
