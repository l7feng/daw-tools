from pathlib import Path
import datetime, hashlib, json, os, subprocess, uuid
repo=Path(__file__).resolve().parents[1]
root=repo/'build/native-validation'/('backend-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'studio-validation.marker').write_text('isolated backend validation');(root/'temp').mkdir()
exe=repo/'build/backend/BackendProbe_artefacts/Release/Studio Backend Probe.exe'
fixture=repo/'build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3'
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
    result=subprocess.run([str(exe),str(root),str(fixture)],env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err,timeout=90)
files=[exe,repo/'src/session.hpp',repo/'tests/backend/main.cpp',repo/'tests/backend/midi_engine.hpp',repo/'tests/backend/midi_transport.hpp',repo/'tests/backend/plugin_recovery.hpp',repo/'tests/backend/tail_cases.hpp',repo/'tests/backend/stem_cases.hpp',repo/'tests/backend/freeze_cases.hpp',repo/'tests/backend/tail_fixture.cpp',*(repo/'build/backend/StudioTestTail_artefacts/Release/VST3').rglob('*.vst3'),repo/'tests/backend/midi_fixture.cpp',*(repo/'build/backend/StudioMidiFixture_artefacts/Release/VST3').rglob('*.vst3'),*fixture.rglob('*.vst3'),repo/'tests/backend/delay_fixture.cpp',repo/'tests/backend/CMakeLists.txt',* (repo/'build/backend/StudioTestDelay_artefacts/Release/VST3').rglob('*.vst3')]
(root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}},indent=2))
print(root,'exit',result.returncode)
for name in ['FAIL.txt','backend-result.json','pdc/pdc-result.json','midi-engine/midi-engine-result.json','midi-transport/midi-transport-result.json','plugin-recovery/plugin-recovery-result.json','tail/tail-result.json','master-plugin-recovery/plugin-recovery-result.json','stems/stems-result.json','freeze/freeze-result.json']:
    if (root/name).exists():print((root/name).read_text())
raise SystemExit(result.returncode)
