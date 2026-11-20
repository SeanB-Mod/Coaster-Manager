"""Compare a post-write native export against the selected source blueprint."""
import json
import math
import sys
from pathlib import Path


def verify(source, actual):
    failures = []
    count = len(source['TrackSupportData'])
    if actual.get('schemaVersion') != 2:
        return ['Expected schemaVersion 2']
    origin = source['stationPosition']
    target = actual['stationPosition']
    if len(target) != 3 or not all(math.isfinite(x) for x in target):
        return ['Invalid target station position']
    delta = [b - a for a, b in zip(origin, target)]
    def normalized(q):
        if not isinstance(q, (list, tuple)) or len(q) != 4:
            return None
        if not all(type(v) in (int, float) and math.isfinite(v) for v in q):
            return None
        length = math.sqrt(sum(v*v for v in q))
        if not math.isfinite(length) or length < 0.000001:
            return None
        return [v/length for v in q]
    def multiply(a,b):
        x,y,z,w=a; X,Y,Z,W=b
        return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
    sourceq=normalized(source['stationRotation']); targetq=normalized(actual['stationRotation'])
    if sourceq is None or targetq is None:
        return ['Invalid source or target station rotation']
    dq=multiply(targetq,[-sourceq[0],-sourceq[1],-sourceq[2],sourceq[3]])
    yaw=2*math.atan2(dq[1],dq[3]); co,si=math.cos(yaw),math.sin(yaw)
    for key in ('TrackSupportData', 'TrackPieceData'):
        if len(actual.get(key, [])) != count:
            failures.append(f'{key}: expected {count} records')
    supports = {r['SectionIndex']: r for r in actual.get('TrackSupportData', [])}
    pieces = {r['SectionIndex']: r for r in actual.get('TrackPieceData', [])}
    if set(supports) != set(range(count)) or set(pieces) != set(range(count)):
        return failures + ['Expected unique contiguous section indices in both lists']
    floors = set()
    for original in source['TrackSupportData']:
        i = original['SectionIndex']
        record = supports[i]
        for field in ('Position', 'WorldPosition'):
            x,y,z=[v-o for v,o in zip(original[field],origin)]
            expected = [target[0]+co*x+si*z,target[1]+y,target[2]-si*x+co*z]
            if len(record[field]) != 3 or any(not math.isfinite(v) or abs(v - e) > .002 for v, e in zip(record[field], expected)):
                failures.append(f'Support {i}: {field} differs from translated source')
        source_support_rotation = normalized(original['Rotation'])
        if source_support_rotation is None:
            failures.append('Invalid source support rotation')
            continue
        q, r = multiply(dq,source_support_rotation), record['Rotation']
        norm = math.sqrt(sum(x*x for x in q) * sum(x*x for x in r))
        if len(r) != 4 or not math.isfinite(norm) or not norm or abs(sum(x*x for x in r)-1) > .001 or abs(sum(x*y for x, y in zip(q, r))/norm) < .999999:
            failures.append(f'Support {i}: world orientation differs')
        for field in ('Height', 'TangentInModifier', 'TangentOutModifier', 'BankingModifier'):
            if record[field] != original[field]:
                failures.append(f'Support {i}: {field} differs')
        if not math.isfinite(record['LocalRotation']) or abs((record['LocalRotation'] - original['LocalRotation'] - math.degrees(yaw) + 180) % 360 - 180) > .001:
            failures.append(f'Support {i}: local rotation differs')
        if not record['Valid']:
            failures.append(f'Support {i}: game reports invalid')
        floors.add(str(next(iter(record['FloorAreaInstance'].values()))))
    if len(floors) != 1 or '0' in floors:
        failures.append('Supports do not share one nonzero destination floor ID')
    for original in source['TrackPieceData']:
        i = original['SectionIndex']
        record = pieces[i]
        if record['TrackPieceDefinitionID'] != original['TrackPieceDefinitionID']:
            failures.append(f'Piece {i}: definition differs')
        if record['TrackPieceDecorationDefinition'] != original['TrackPieceDecorationDefinition']:
            failures.append(f'Piece {i}: decoration definition differs')
        if str(next(iter(original['TrackPieceDecorationDefinition'].values()))) != '0':
            if not math.isfinite(record['DecorationPosition']) or abs(record['DecorationPosition']-original['DecorationPosition']) > .00001:
                failures.append(f'Piece {i}: decoration position differs')
        # With no decoration, its unused saved position may be reset by the game.
    return failures


if __name__ == '__main__':
    source = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
    actual = json.loads(Path(sys.argv[2]).read_text(encoding='utf-8-sig'))
    problems = verify(source, actual)
    for problem in problems:
        print('FAIL:', problem)
    if not problems:
        print('PASS: positioned/rotated supports, orientations, scalar settings, piece definitions and common floor ID')
        print('Station connection, collision/buildability and save/reload still require in-game checks.')
    sys.exit(bool(problems))
