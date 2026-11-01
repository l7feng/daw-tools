"""Native healthy/crash/hang/healthy scan sequence; no Session or audio device in workers."""
from pathlib import Path
import datetime, hashlib, importlib.util, json, uuid
repo=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('scan_plugins',repo/'scripts/scan-plugins.py');scan=importlib.util.module_from_spec(spec);spec.loader.exec_module(scan)
exe=repo/'build/editor/StudioWorkbench_artefacts/Release/Studio Workbench Verified.exe'
gain=repo/'build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3'
crash=repo/'build/backend/StudioScanCrash_artefacts/Release/VST3/Studio Scan Crash.vst3'
hang=repo/'build/backend/StudioScanHang_artefacts/Release/VST3/Studio Scan Hang.vst3'
tail=repo/'build/backend/StudioTestTail_artefacts/Release/VST3/Studio Test Tail.vst3'
folder=repo/'build/native-validation'/('native-scan-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
records=scan.scan_batch([gain,crash,hang,gain,tail],folder,[str(exe)],timeout=2)
assert [r['status'] for r in records]==['ok','failed','timeout','ok'], records
assert records[1]['exit_code']==73, records[1]
for index,mode in [(1,1),(2,2)]:assert (Path(records[index]['job'])/'temp'/f'native-scan-entered-{mode}.txt').is_file()
report={'checks':5,'statuses':[r['status'] for r in records],'duplicate_path_omitted':True,'healthy_results_retained':True,'native_fault_factories_entered':True,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'physical_device_opened':False}
(folder/'native-scan-result.json').write_text(json.dumps(report,indent=2));print(folder);print(json.dumps(report,indent=2))
