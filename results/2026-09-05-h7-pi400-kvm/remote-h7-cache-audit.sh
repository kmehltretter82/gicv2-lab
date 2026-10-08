set -eu
python3 - <<'PYCODE'
from pathlib import Path
import hashlib, importlib.util, json, marshal, struct, sys
root=Path('/home/karl/gicv2-lab-20260905-hardware/h7-kvm')
rows=[]
for directory in sorted(root.glob('run-*')):
    bundle=directory/'bundle'
    manifest=json.loads((bundle/'build.json').read_text())
    for cached in sorted((bundle/'source/tools/__pycache__').glob('*.pyc')):
        source=cached.parent.parent/(cached.name.split('.')[0]+'.py')
        source_bytes=source.read_bytes()
        source_sha=hashlib.sha256(source_bytes).hexdigest()
        assert source_sha==manifest['artifacts'][str(source.relative_to(bundle))]
        data=cached.read_bytes()
        assert data[:4]==importlib.util.MAGIC_NUMBER
        assert struct.unpack_from('<I',data,4)[0]==0
        assert struct.unpack_from('<I',data,12)[0]==len(source_bytes)
        code=marshal.loads(data[16:])
        rebuilt=compile(source_bytes,code.co_filename,'exec',dont_inherit=True,optimize=0)
        assert code==rebuilt, str(cached)
        rows.append({'path':str(cached.relative_to(root)),'sha256':hashlib.sha256(data).hexdigest(),'source_sha256':source_sha,'matches_source':True})
assert len(rows)==60
print(json.dumps({'python':sys.version,'cache_files_checked':len(rows),'all_match_preserved_source':True,'files':rows},indent=2))
PYCODE
