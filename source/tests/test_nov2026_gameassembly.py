"""Offline verifier for the Nov 2026 Windows compatibility map (no game launch).

Usage: python test_nov2026_gameassembly.py GAMEASSEMBLY_DLL GLOBAL_METADATA_DAT
The game files must be locally supplied; do not redistribute them.
"""
import ctypes
import hashlib
import json
import mmap
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
cfg = json.loads((HERE / 'nov2026_native_map.json').read_text())


def fnv(buf):
    value = 2166136261
    for byte in buf:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def main(game, metadata):
    game, metadata = Path(game), Path(metadata)
    with game.open('rb') as file:
        mm = mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ)
        assert hashlib.sha256(mm).hexdigest() == cfg['game_sha256'], 'Unexpected game assembly: no compatibility approval'
        pe_offset = struct.unpack_from('<I', mm, 0x3c)[0]
        assert mm[pe_offset:pe_offset + 4] == b'PE\0\0'
        assert struct.unpack_from('<I', mm, pe_offset + 8)[0] == cfg['game_pe_timestamp']
        section_count = struct.unpack_from('<H', mm, pe_offset + 6)[0]
        optional_size = struct.unpack_from('<H', mm, pe_offset + 20)[0]
        sections = []
        for i in range(section_count):
            _, virtual_size, virtual_rva, raw_size, raw_offset = struct.unpack_from(
                '<8sIIII', mm, pe_offset + 24 + optional_size + i * 40)
            sections.append((virtual_rva, min(virtual_size, raw_size), raw_offset))

        def code_at(rva, size):
            for base, n, off in sections:
                if base <= rva and rva + size <= base + n:
                    return mm[off + rva - base:off + rva - base + size]
            raise AssertionError(f'RVA {rva:#x} outside on-disk sections')

        source = (ROOT / 'first_write.inc').read_text()
        for row in cfg['rows']:
            expected = f'{{0x{row["oldRva"]:x}U, 0x{row["oldSize"]:x}U, 0x{row["oldHash"]:x}U, 0x{row["newRva"]:x}U, {row["newSize"]}U, 0x{row["newHash"]:x}U}}'
            assert expected in source, f"Missing native guard for {row['name']}"
            body = code_at(row['newRva'], row['newSize'])
            assert fnv(body) == row['newHash'], f"Changed implementation: {row['name']}"

        # Verify the experimental auto-finish guard points to the actual static
        # FinishPlaceItem method in the user's exact current metadata, not just
        # to arbitrary code at a plausible RVA.
        meta = metadata.read_bytes()
        strings_off, strings_len = struct.unpack_from('<II', meta, 0x18)
        methods_off, methods_len = struct.unpack_from('<II', meta, 0x30)
        assert methods_len % 36 == 0
        finish = next(row for row in cfg['rows'] if row['name'] == 'FinishPlaceItem')
        matches = []
        for index in range(methods_len // 36):
            name_index, declaring_type, return_type = struct.unpack_from('<III', meta, methods_off + index * 36)
            if (name_index < strings_len and
                    meta[strings_off + name_index:strings_off + name_index + 16] == b'FinishPlaceItem\0'):
                matches.append((index, declaring_type, return_type))
        assert matches == [(finish['metadataMethodIndex'], finish['metadataDeclaringTypeIndex'],
                            finish['metadataReturnTypeIndex'])], matches
        imagebase = struct.unpack_from('<Q', mm, pe_offset + 24 + 24)[0]
        assert imagebase == 0x180000000
        for method_index, rva in [(12822, 0x2f25f20), (12823, 0x2f25f40), (60192, 0x2a96b20)]:
            expected_offset = finish['methodPointerTableFileOffset'] - 8 * (finish['metadataMethodIndex'] - method_index)
            assert struct.unpack_from('<Q', mm, expected_offset)[0] == imagebase + rva
        assert struct.unpack_from('<Q', mm, finish['methodPointerTableFileOffset'])[0] == imagebase + finish['newRva']
        print('PASS: metadata FinishPlaceItem identity and method pointer RVA')

        # Execute the actual extracted production guard under a Linux-only mock.
        # All 10 approved methods must pass; wrong RVA/identity or byte edits must fail.
        compiler = shutil.which('clang') or shutil.which('gcc')
        if compiler and sys.platform.startswith('linux'):
            full = (ROOT / 'first_write.inc').read_text()
            snippet = full[full.index('static unsigned write_hash('):full.index('static void write_log(')]
            prefix = r'''
                #include <stdint.h>
                #include <stddef.h>
                #include <string.h>
                #include <wchar.h>
                typedef uintptr_t UINT_PTR;
                typedef void* HMODULE;
                static uintptr_t fake_module = 0x10000000;
                static unsigned current_rva, current_size;
                static const unsigned char *current_data;
                static HMODULE GetModuleHandleW(const wchar_t *name) {
                  (void)name;
                  return (HMODULE)fake_module;
                }
                static int orientation_code(void *method, void **code, unsigned char *out, unsigned n) {
                  if (!method || !current_data || n > current_size) return 0;
                  *code = (void *)(fake_module + current_rva);
                  memcpy(out, current_data, n);
                  return 1;
                }
            '''
            postfix = r'''
                __attribute__((visibility("default")))
                int Check(unsigned oldRva, unsigned size, unsigned oldHash, unsigned newRva,
                          const unsigned char *bytes, unsigned n) {
                  current_rva = newRva; current_data = bytes; current_size = n;
                  return write_code_guard((void*)1, oldRva, size, oldHash);
                }
            '''
            with tempfile.TemporaryDirectory() as temp:
                source_path, binary_path = Path(temp) / 'guard.c', Path(temp) / 'guard.so'
                source_path.write_text(prefix + snippet + postfix)
                subprocess.run([compiler, '-shared', '-fPIC', '-O1', '-o', str(binary_path), str(source_path)], check=True)
                dll = ctypes.CDLL(str(binary_path))
                dll.Check.argtypes = [ctypes.c_uint] * 4 + [ctypes.c_void_p, ctypes.c_uint]
                dll.Check.restype = ctypes.c_int
                for row in cfg['rows']:
                    original = code_at(row['newRva'], row['newSize'])
                    raw = ctypes.create_string_buffer(original)
                    def check(rva=row['newRva'], digest=row['oldHash'], length=row['newSize']):
                        return dll.Check(row['oldRva'], row['oldSize'], digest, rva, raw, length)
                    assert check() == 1, ('guard incorrectly rejects', row['name'])
                    assert check(rva=row['newRva'] + 16) == 0, ('wrong RVA accepted', row['name'])
                    assert check(digest=row['oldHash'] ^ 1) == 0, ('wrong identity accepted', row['name'])
                    assert check(length=row['newSize'] - 1) == 0, ('short code accepted', row['name'])
                    raw[0] = bytes([original[0] ^ 1])
                    assert check() == 0, ('tampered code accepted', row['name'])
                    print('PASS native guarded method', row['name'])
        else:
            print('SKIP: native execution test requires Linux with clang or gcc')

    content = metadata.read_bytes()
    for name in [b'<SetComponents>g__Snapshot|32', b'<SetComponents>g__Revert|33',
                 b'<SetComponents>g__ToggleTrackSupportArrow|15_28']:
        assert content.count(name) == 1, ('metadata name missing/ambiguous', name)
    print('PASS: 11 current game method hashes, current PE identity, metadata aliases; fail-closed guards')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
