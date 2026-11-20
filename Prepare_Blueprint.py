"""Validate a native coaster export and prepare an import plan with destination configuration checks."""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import sys

NORMAL_ID = -115856756
MAX_SUPPORTS = 512

def require(condition, message):
    if not condition:
        raise ValueError(message)

def number(value):
    require(type(value) in (int, float) and math.isfinite(value), 'Expected a finite number')
    try:
        result = struct.unpack('<f', struct.pack('<f', value))[0]
    except (OverflowError, struct.error):
        raise ValueError('Number exceeds float32 range') from None
    require(math.isfinite(result), 'Number exceeds float32 range')
    return result

def native(record, field, length=None):
    value = record[field]
    values = [value] if length is None else value
    require(type(values) is list and len(values) == (1 if length is None else length), f'{field}: invalid vector length')
    numbers = [number(v) for v in values]
    bits = record.get(field + 'Float32Bits')
    require(bits is not None, f'{field}: native float32 bits missing; use a current F8 export')
    bits = [bits] if length is None else bits
    require(type(bits) is list and len(bits) == len(numbers), f'{field}: invalid native bits')
    packed = b''
    for expected, word in zip(numbers, bits):
        require(type(word) is str and len(word) == 10 and word.startswith('0x'), f'{field}: malformed native bits')
        raw = struct.pack('<I', int(word, 16))
        actual = struct.unpack('<f', raw)[0]
        require(math.isfinite(actual) and actual == expected, f'{field}: numeric value and native bits disagree')
        packed += raw
    return packed

def quaternion(raw, upright=False):
    q = struct.unpack('<4f', raw)
    require(abs(sum(x*x for x in q)-1) <= .00001, 'Quaternion must be unit length')
    if upright:
        require(abs(q[0]) < .00001 and abs(q[2]) < .00001, 'Tilted station orientations are unsupported')

def identifier(value):
    if type(value) is dict:
        require(len(value) == 1, 'Malformed wrapped ID')
        value = next(iter(value.values()))
    require(type(value) in (str, int) and type(value) is not bool, 'Invalid ID')
    if type(value) is str:
        require(value.lstrip('-').isdigit(), 'Invalid ID text')
    return int(value)

def prepare(document):
    require(type(document) is dict, 'Expected an exported JSON object')
    require(document.get('schema') == 'tpm-coaster-snapshot' and document.get('schemaVersion') == 2, 'Only schema-2 native exports are supported')
    orientation = document['supportOrientation']
    require(orientation.get('sourceSpace') == 'world' and orientation.get('sourceSpaceConfirmed') is True, 'World-space support rotation must be confirmed')
    frame = document['stationFrameProbe']
    require(frame.get('status') == 'position-and-quaternion', 'Station frame missing')
    origin = tuple(number(v) for v in frame['transformWorldPosition'])
    require(len(origin) == 3, 'Invalid source station position')
    rotation = tuple(number(v) for v in frame['transformWorldRotation'])
    require(len(rotation) == 4, 'Invalid station rotation')
    quaternion(struct.pack('<4f', *rotation), upright=True)
    floor = document['floorFrameProbe']
    require(floor.get('status') == 'world-frame-read', 'Source floor frame missing')
    require(floor['worldRotation'] in ([0, 0, 0, 1], [0, 0, 0, -1]), 'Rotated source floors are unsupported')
    source_id = identifier(document['sourceStationId'])
    require(0 < source_id < 2**63, 'Invalid source station ID')
    supports, pieces = document['TrackSupportData'], document['TrackPieceData']
    require(type(supports) is list and type(pieces) is list and 1 <= len(supports) == len(pieces) <= MAX_SUPPORTS, 'Require 1..512 supports and an equal number of pieces')
    for records in (supports, pieces):
        require(all(type(r.get('listIndex')) is int and r['listIndex'] == i for i, r in enumerate(records)), 'Invalid or duplicate list indices')
        require(all(type(r.get('SectionIndex')) is int for r in records), 'Section indices must be integers')
        require(sorted(r['SectionIndex'] for r in records) == list(range(len(records))), 'Section indices must be unique and contiguous from zero')
    supports = sorted(supports, key=lambda r: r['SectionIndex'])
    pieces = sorted(pieces, key=lambda r: r['SectionIndex'])
    floors = set()
    body = bytearray()
    for index, (support, piece) in enumerate(zip(supports, pieces)):
        require(type(support['SectionIndex']) is int and support['SectionIndex'] == index and type(piece['SectionIndex']) is int and piece['SectionIndex'] == index, 'Section indices must be ordered and contiguous from zero')
        position, world = native(support, 'Position', 3), native(support, 'WorldPosition', 3)
        xyz = struct.unpack('<3f', position)
        require(xyz == struct.unpack('<3f', world), 'Different local/world support positions are unsupported')
        require(xyz[1] == origin[1] and all(abs(v-o) <= 1024 for v, o in zip(xyz, origin)), 'Support bases must share the station elevation and be within 1024 units of the station on each axis')
        rotation_raw = native(support, 'Rotation', 4)
        quaternion(rotation_raw)
        angle = native(support, 'LocalRotation')
        require(0 <= struct.unpack('<f', angle)[0] <= 360, 'LocalRotation outside 0..360')
        require(support['Valid'] is True and type(support['Height']) is int and 1 <= support['Height'] <= 1024, 'Require valid supports with height 1..1024')
        modifiers = b''.join(native(support, field) for field in ('TangentInModifier', 'TangentOutModifier', 'BankingModifier'))
        tangent_in, tangent_out, bank = struct.unpack('<3f', modifiers)
        require(abs(tangent_in) <= 100 and abs(tangent_out) <= 100 and abs(bank) <= 360, 'Support modifiers exceed validation bounds')
        floors.add(identifier(support['FloorAreaInstance']))
        piece_id = identifier(piece['TrackPieceDefinitionID'])
        decoration_id = identifier(piece['TrackPieceDecorationDefinition'])
        require(-(2**63) <= piece_id < 2**63 and piece_id != 0 and -(2**63) <= decoration_id < 2**63, 'Invalid piece/decoration ID')
        decoration_pos = native(piece, 'DecorationPosition')
        require(0 <= struct.unpack('<f', decoration_pos)[0] <= 1, 'Decoration position must be in 0..1')
        body += position + rotation_raw + angle + struct.pack('<i', support['Height']) + modifiers + struct.pack('<qq', piece_id, decoration_id) + decoration_pos + struct.pack('<I', 0)
    require(len(floors) == 1 and next(iter(floors)) > 0, 'Source supports must share a nonzero floor ID')
    station_definition = document.get('sourceStationDefinitionId')
    if station_definition is None:
        station_definition = 0
    else:
        station_definition = identifier(station_definition)
        require(-(2**63) <= station_definition < 2**63 and station_definition != 0, 'Invalid station definition ID')
    magic = b'TPMPLAN3' if station_definition else b'TPMPLAN2'
    header = struct.pack('<8sIIQq3f4f', magic, len(supports), 0, source_id, station_definition, *origin, *rotation)
    data = header + body
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    return data + struct.pack('<I', checksum)

def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON key: {key}')
        result[key] = value
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('blueprint', nargs='?', type=Path)
    parser.add_argument('--output', type=Path, default=None)
    args = parser.parse_args()
    if args.output is None:
        profile = os.environ.get('USERPROFILE') or os.environ.get('HOME')
        if not profile:
            print('REJECTED: user profile is unavailable; specify --output.')
            return 1
        args.output = Path(profile)/'AppData'/'LocalLow'/'Two Point Studios'/'TPM Coaster Library'/'Diagnostics'/'SelectedBlueprint.tpmplan'
    if args.blueprint is None:
        import tkinter as tk
        from tkinter.filedialog import askopenfilename
        root = tk.Tk(); root.withdraw()
        chosen = askopenfilename(title='Choose an F8 coaster blueprint', filetypes=[('Coaster blueprint JSON', '*.json')])
        root.destroy()
        if not chosen:
            print('Selection cancelled. Existing selection unchanged.'); return 1
        args.blueprint = Path(chosen)
    try:
        require(args.blueprint.stat().st_size <= 4*1024*1024, 'JSON exceeds 4 MiB')
        data = prepare(json.loads(args.blueprint.read_text(encoding='utf-8-sig'), object_pairs_hook=no_duplicates))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_suffix('.tmp')
        with temporary.open('wb') as stream:
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
        temporary.replace(args.output)
    except (ValueError, KeyError, TypeError, OSError, OverflowError, struct.error) as error:
        print('REJECTED:', error)
        print('Existing selection is unchanged; do not use F9 expecting this rejected blueprint.')
        return 1
    print('Selected:', args.blueprint.resolve())
    print('Prepared:', args.output.resolve())
    fields = struct.unpack('<8sIIQq3f4f', data[:60])
    count, station_id = fields[1], fields[3]
    position, q = fields[5:8], fields[8:12]
    heading = math.degrees(math.atan2(2*q[1]*q[3], 1-2*q[1]*q[1])) % 360
    print(f'{count} supports/pieces; source station {station_id}; source position {position}; source heading {heading:.2f} degrees (destination heading can differ)')
    if data[:8] == b'TPMPLAN3':
        print('F10 station-placement test available: the plan is stored under LocalLow/TPM Coaster Library/Diagnostics; deselect everything, then press F10 in a disposable save.')
    else:
        print('Legacy export: F10 unavailable; export again with v0.5.7 for station definition data.')
    print('The plan is stored in LocalLow/TPM Coaster Library/Diagnostics. Place a NEW matching station with space for the rotated layout, in a disposable save.')
    print('Deselect the support while keeping the coaster controls open, then press F9 once. Do not save until the log and track are checked.')
    return 0

if __name__ == '__main__':
    sys.exit(main())
