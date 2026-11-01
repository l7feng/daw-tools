from pathlib import Path
import datetime, hashlib, json, os, subprocess, uuid, argparse
parser=argparse.ArgumentParser();parser.add_argument('--capture',action='store_true');parser.add_argument('--block',type=int,choices=[128,256,512,1024],default=256);args=parser.parse_args()
repo=Path(__file__).resolve().parents[1]
root=repo/'build/native-validation'/('device-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'studio-validation.marker').write_text('isolated device validation');(root/'temp').mkdir()
exe=repo/'build/backend/BackendProbe_artefacts/Release/Studio Backend Probe.exe'
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 result=subprocess.run([str(exe),str(root),'--capture' if args.capture else '--devices',str(args.block)],env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err,timeout=30)
files=[exe,repo/'tests/backend/main.cpp',repo/'tests/backend/device_probe.hpp',Path(__file__)]
(root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'mode':'capture' if args.capture else 'enumerate','requested_block':args.block,'sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}},indent=2))
print(root,'exit',result.returncode)
for name in ['FAIL.txt','device-result.json'] if args.capture else ['FAIL.txt','devices.json']:
 if (root/name).exists():print((root/name).read_text(encoding='utf-8'))
raise SystemExit(result.returncode)
