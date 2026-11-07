"""Run the S1 MIDI VST3 verification without opening audio devices."""
from pathlib import Path
import datetime, hashlib, json, os, subprocess, uuid
repo=Path(__file__).resolve().parents[1]
exe=repo/'build/backend/MidiVerify_artefacts/Release/MidiVerify.exe'
fixture=repo/'build/backend/StudioMidiFixture_artefacts/Release/VST3/Studio MIDI Timing Fixture.vst3'
if not exe.is_file() or not fixture.is_dir():raise SystemExit('Build pending MIDI targets before running')
root=repo/'build/native-validation'/('midi-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'temp').mkdir();(root/'studio-validation.marker').write_text('isolated MIDI fixture validation')
files=[exe,repo/'tests/backend/midi_fixture.cpp',repo/'tests/backend/midi_verify.cpp',repo/'tests/backend/midi_targets.cmake',repo/'dependencies.lock.json',Path(__file__),*fixture.rglob('*.vst3')]
identity={str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 result=subprocess.run([str(exe),str(fixture),str(root/'midi-result.json')],cwd=repo,env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err,timeout=30)
(root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'sha256':identity},indent=2))
print(root,'exit',result.returncode)
for name in ['midi-result.json','stderr.log']:
 if (root/name).exists():print((root/name).read_text(encoding='utf-8',errors='replace'))
raise SystemExit(result.returncode)
