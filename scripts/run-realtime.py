from pathlib import Path
import datetime, hashlib, json, os, subprocess, uuid
repo=Path(__file__).resolve().parents[1]
root=repo/'build/native-validation'/('realtime-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'studio-validation.marker').write_text('isolated realtime validation');(root/'temp').mkdir()
exe=repo/'build/backend/BackendProbe_artefacts/Release/Studio Backend Probe.exe'
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 result=subprocess.run([str(exe),str(root),'--realtime'],env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err,timeout=40)
files=[exe,repo/'src/session.hpp',repo/'tests/backend/main.cpp',repo/'tests/backend/realtime_probe.hpp',repo/'dependencies.lock.json',Path(__file__)]
(root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}},indent=2))
print(root,'exit',result.returncode)
for name in ['FAIL.txt','realtime-result.json']:
 if (root/name).exists():print((root/name).read_text(encoding='utf-8'))
raise SystemExit(result.returncode)
