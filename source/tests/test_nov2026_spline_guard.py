"""Offline test of Coaster Manager v1.1.0 native guard against a supplied local game assembly.

Usage: python tests/test_nov2026_spline_guard.py '/path/to/GameAssembly.dll'
Does not execute any game code, attach to the game, or distribute game binaries.
Runs the actual production write_code_guard extracted from first_write.inc in a Linux mock
and verifies the current-build whitelisted method, address, full function fingerprint,
and fail-closed responses to mutation or mismatches.
"""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / 'first_write.inc'
GAME = Path(sys.argv[1]) if len(sys.argv) > 1 else None
EXPECTED_SHA = 'f89d2e3fb6050172863eb752ddf355e8c05c87bfb6dff3257de0a98520a602ec'
OLD = (0x2ead6b0, 0x1270, 0x621a450e)
NEW = (0x2efac70, 0x170b, 0x30468546)


def pe_sections(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    opt_size = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    assert struct.unpack_from('<H', data, opt)[0] == 0x20b
    count = struct.unpack_from('<H', data, pe + 6)[0]
    secs = []
    for i in range(count):
        pos = opt + opt_size + i * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from('<IIII', data, pos + 8)
        secs.append((virtual_address, raw_offset, raw_size))
    def off(rva):
        for base, raw, size in secs:
            if base <= rva < base + size:
                return raw + rva - base
        raise AssertionError('RVA not mapped to disk: ' + hex(rva))
    return off, opt


def hash_bytes(blob):
    v = 2166136261
    for b in blob:
        v = ((v ^ b) * 16777619) & 0xffffffff
    return v


def test():
    source = SRC.read_text()
    row = ('{0x2ead6b0U, 0x1270U, 0x621a450eU, 0x2efac70U, '
           '0x170bU, 0x30468546U}')
    assert source.count(row) == 1
    assert source.count('if (!write_code_guard(spline, 0x2ead6b0, 0x1270, 0x621a450eU))') == 1
    assert source.count('if (verified && !write_request_recalculation(') == 1
    assert 'if (!pf_same_entity(current, entity))' in source
    assert 'if (verified && !write_request_recalculation(' in source
    print('PASS: guarded original three-parameter BuildSpline call remains behind verification')

    if not GAME:
        print('Assembly not supplied; code-mapping checks only')
        return
    data = GAME.read_bytes()
    assert hashlib.sha256(data).hexdigest() == EXPECTED_SHA, 'Wrong game binary; guard intentionally closes'
    off, opt = pe_sections(data)
    function = data[off(NEW[0]):off(NEW[0]) + NEW[1]]
    assert len(function) == NEW[1]
    assert hash_bytes(function) == NEW[2]
    assert function[:14] == bytes.fromhex('488bc44889581048897020448840')
    # Native unwind function entry confirms exactly one method begins here.
    ex_rva, ex_len = struct.unpack_from('<II', data, opt + 112 + 3 * 8)
    entries = data[off(ex_rva):off(ex_rva) + ex_len]
    matches = [struct.unpack_from('<III', entries, i) for i in range(0,len(entries)-11,12)
               if struct.unpack_from('<I', entries, i)[0] == NEW[0]]
    assert len(matches) == 1 and matches[0][1] - matches[0][0] == NEW[1]
    print('PASS: current game BuildSpline start, unwind extent and full FNV fingerprint')

    if sys.platform != 'linux':
        print('Host executable guard mock skipped; this test uses Linux gcc')
        return

    src = source[source.index('static unsigned write_hash('):source.index('static void write_log(')]
    mock = r'''
#include <stdint.h>
#include <string.h>
#include <wchar.h>
typedef void * HMODULE;
typedef uintptr_t UINT_PTR;
static unsigned inputRva = 0;
static unsigned char *input = 0;
static unsigned inputSize = 0;
static HMODULE GetModuleHandleW(const wchar_t *p) { (void)p; return (void *)(uintptr_t)0x10000000; }
static int orientation_code(void *m, void **code, unsigned char *b, unsigned n) {
    if (!m || !input || n > inputSize) return 0;
    *code = (void *)((uintptr_t)0x10000000 + inputRva);
    memcpy(b, input, n);return 1;
}
'''
    exports = r'''
int Check(unsigned oldRva,unsigned oldLen,unsigned oldHash,unsigned currentRva,
          unsigned char *b,unsigned size) {
    inputRva = currentRva; input = b; inputSize = size;
    return write_code_guard((void *)1, oldRva, oldLen, oldHash);
}
'''
    with tempfile.TemporaryDirectory() as t:
        c = Path(t) / 'guard.c'
        so = Path(t) / 'guard.so'
        c.write_text(mock + src + exports)
        subprocess.run(['gcc','-std=c11','-shared','-fPIC','-O2','-o',str(so),str(c)], check=True)
        lib = ctypes.CDLL(str(so)); lib.Check.argtypes = [ctypes.c_uint]*4 + [ctypes.c_void_p,ctypes.c_uint]
        lib.Check.restype=ctypes.c_int
        buf = ctypes.create_string_buffer(function)
        def run(old_rva=OLD[0],old_len=OLD[1],old_hash=OLD[2],new_rva=NEW[0],size=NEW[1]):
            return lib.Check(old_rva,old_len,old_hash,new_rva,buf,size)
        assert run() == 1
        assert run(new_rva=NEW[0]+1) == 0
        assert run(old_hash=OLD[2] ^ 1) == 0
        assert run(old_len=OLD[1]+1) == 0
        assert run(size=NEW[1]-1) == 0
        buf[10] = bytes([function[10] ^ 1]); assert run() == 0
        buf[10] = function[10]; assert run() == 1
    print('PASS: production native guard accepts exact BuildSpline and rejects modified bytes, wrong RVA, wrong old key and short body')

if __name__ == '__main__':
    test()
