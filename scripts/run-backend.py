from pathlib import Path
import datetime, hashlib, json, os, subprocess, uuid
repo=Path(__file__).resolve().parents[1]
root=repo/'build/native-validation'/('backend-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'studio-validation.marker').write_text('isolated backend validation');(root/'temp').mkdir()
exe=repo/'build/backend/BackendProbe_artefacts/Release/Studio Backend Probe.exe'
fixture=repo/'build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3'
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
    result=subprocess.run([str(exe),str(root),str(fixture)],env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err,timeout=90)
files=[exe,repo/'src/session.hpp',repo/'tests/backend/main.cpp',*fixture.rglob('*.vst3')]
(root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}},indent=2))
print(root,'exit',result.returncode)
for name in ['FAIL.txt','backend-result.json']:
    if (root/name).exists():print((root/name).read_text())
raise SystemExit(result.returncode)
