Import("env")
from os.path import join

proj = env["PROJECT_DIR"]
mk = join(proj, "components", "microkernel")
env.Append(CPPPATH=[join(mk, "include"), join(mk, "arch", "esp32s3"), join(mk, "port"), join(mk, "port", "native"), join(mk, "port", "freertos")])
mk_lib = env.BuildLibrary(
    join("$BUILD_DIR", "microkernel"),
    mk,
    src_filter=[
        "+<core/*.c>",
        "+<ipc/*.c>",
        "+<time/*.c>",
        "+<memory/*.c>",
        "+<arch/esp32s3/*.c>",
        "+<port/*.c>",
        "+<port/native/*.c>",
        "+<port/freertos/*.c>",
    ],
)
env.Prepend(LIBS=[mk_lib])
