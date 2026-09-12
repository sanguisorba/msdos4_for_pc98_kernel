#!/usr/bin/env python3
"""Build only KERNEL.SYS for the PC-98 MS-DOS 4.0 BIOS bridge."""

from pathlib import Path
import os
import shutil
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
OUTPUT_DIR = ROOT / "bin"
FD = ROOT / "source" / "freedos"
MS = ROOT / "source" / "msdos"
WCC = Path(os.environ.get("WCC", ROOT / "tools" / "wcc"))
NASM = Path(os.environ.get("NASM", ROOT / "tools" / "nasm"))
EMU2 = Path(os.environ.get("EMU2", ROOT / "tools" / "emu2"))
MASM = ROOT / "tools" / "dos" / "MASM.EXE"
LINK = ROOT / "tools" / "dos" / "LINK.EXE"


def fail(message):
    raise SystemExit(message)


def run_native(name, args, cwd=BUILD):
    result = subprocess.run(
        [str(x) for x in args],
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    (BUILD / f"{name}.log").write_bytes(result.stdout)
    if result.returncode:
        print(result.stdout.decode(errors="replace"))
        fail(f"{name} failed with exit code {result.returncode}")


def normalize_microsoft_source(source, destination):
    data = source.read_bytes()
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        text = data.decode("cp437")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(text.replace("\n", "\r\n").encode("cp437"))


def stage_microsoft_sources():
    stage = BUILD / "ms"
    for source in sorted(MS.rglob("*")):
        if source.is_file():
            normalize_microsoft_source(source, stage / source.relative_to(MS))


def run_dos(name, tool, args, dos_cwd):
    env = os.environ.copy()
    env["EMU2_DRIVE_C"] = str(BUILD)
    env["EMU2_CWD"] = dos_cwd
    result = subprocess.run(
        [str(EMU2), str(tool), *args],
        cwd=BUILD,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    (BUILD / f"{name}.log").write_bytes(result.stdout)
    if result.returncode:
        print(result.stdout.decode("cp437", errors="replace"))
        fail(f"{name} failed with exit code {result.returncode}")


def build_sysinit_objects():
    for module in ("SYSINIT1", "SYSCONF", "SYSINIT2", "SYSIMES"):
        run_dos(
            f"masm-{module.lower()}",
            MASM,
            [
                "-Mx",
                "-IC:\\ms\\INC",
                "-IC:\\ms\\BIOS",
                f"{module}.ASM,{module}.OBJ,NUL,NUL;",
            ],
            "C:\\ms\\BIOS",
        )


def build_native_objects():
    include_dirs = (FD / "hdr", FD / "nec98" / "kernel", FD / "kernel")
    cflags = [
        "-0", "-ms", "-s", "-os", "-zq", "-zl", "-zp1",
        "-dNEC98", "-dNO_REVISION", "-dWATCOM", "-nt=HMA_TEXT",
        *[f"-i={path}" for path in include_dirs],
    ]
    c_sources = (
        ("dsk", FD / "nec98" / "kernel" / "dsk.c"),
        ("initdisk", FD / "nec98" / "kernel" / "initdisk.c"),
        ("sysclk", FD / "nec98" / "kernel" / "sysclk.c"),
        ("int29dc", FD / "nec98/kernel/int29dc.c"),
        ("initoem", FD / "nec98/kernel/initoem.c"),
        ("prf", FD / "nec98/kernel/prf.c"),
        ("bridge", ROOT / "src" / "bridge.c"),
    )
    for name, source in c_sources:
        run_native(
            f"wcc-{name}",
            [WCC, *cflags, *(["-d_INIT"] if name == "prf" else []), f"-fo={name}.obj", source],
        )

    asm_sources = (
        ("entry", ROOT / "src" / "entry.asm"),
        ("pc98_iogate", ROOT / "src/pc98_iogate.asm"),
        ("io", FD / "nec98/kernel/io.asm"),
        ("console", FD / "nec98/kernel/console.asm"),
        ("floppy", FD / "nec98" / "drivers" / "floppy.asm"),
        ("rw98clk", FD / "nec98" / "drivers" / "rw98clk.asm"),
        ("asmsupt", FD / "kernel" / "asmsupt.asm"),
        ("intr", FD / "nec98" / "kernel" / "intr.asm"),
    )
    nasm_flags = [
        "-I" + str(ROOT / "src") + "/",
        "-f", "obj", "-DNEC98", "-DWATCOM", "-DXCPU=86",
        "-I" + str(FD / "kernel") + "/",
        "-I" + str(FD / "nec98" / "kernel") + "/",
        "-I" + str(FD / "hdr") + "/",
    ]
    for name, source in asm_sources:
        run_native(
            f"nasm-{name}",
            [NASM, *nasm_flags, "-o", BUILD / f"{name}.obj", source],
            cwd=FD / "kernel",
        )


def link_kernel():
    native = (
        "entry", "dsk", "initdisk", "sysclk", "bridge",
        "floppy", "rw98clk", "asmsupt", "intr",
        "pc98_iogate", "io", "console", "int29dc", "initoem", "prf",
    )
    sysinit = ("SYSINIT1", "SYSCONF", "SYSINIT2", "SYSIMES")
    response = "+\r\n".join(f"{name}.obj" for name in native)
    response += "+\r\n"
    response += "+\r\n".join(f"ms\\BIOS\\{name}.obj" for name in sysinit)
    response += ",\r\nkernel.exe,\r\nkernel.map/M;\r\n"
    (BUILD / "link.rsp").write_bytes(response.encode("ascii"))
    run_dos("link", LINK, ["@link.rsp"], "C:\\")


def flatten_mz(source, destination, load_segment):
    data = source.read_bytes()
    if len(data) < 28:
        fail("kernel.exe is too short")
    header = struct.unpack_from("<14H", data)
    if header[0] != 0x5A4D:
        fail("kernel.exe is not an MZ executable")
    if header[10] or header[11]:
        fail("linked image requires a nonzero initial SS:SP")
    size = (header[2] - 1) * 512 + (header[1] or 512)
    image = bytearray(data[header[4] * 16:size])
    for index in range(header[3]):
        offset, segment = struct.unpack_from("<HH", data, header[12] + index * 4)
        position = segment * 16 + offset
        value = struct.unpack_from("<H", image, position)[0]
        struct.pack_into("<H", image, position, (value + load_segment) & 0xFFFF)
    destination.write_bytes(image)
    return len(image)


def main():
    required = (WCC, NASM, EMU2, MASM, LINK)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        fail("Missing build tools:\n" + "\n".join(missing))

    if BUILD.exists():
        shutil.rmtree(BUILD)
    BUILD.mkdir()
    OUTPUT_DIR.mkdir(exist_ok=True)
    output = OUTPUT_DIR / "KERNEL.SYS"
    if output.exists():
        output.unlink()

    stage_microsoft_sources()
    build_sysinit_objects()
    build_native_objects()
    link_kernel()
    size = flatten_mz(BUILD / "kernel.exe", output, 0x60)
    print(f"KERNEL.SYS: {size} bytes")
    print(output)


if __name__ == "__main__":
    main()
