from pathlib import Path
import datetime, hashlib, json, os, subprocess, uuid, argparse, shutil, ctypes, time
from ctypes import wintypes
parser=argparse.ArgumentParser();parser.add_argument('--tracks',type=int,choices=[1,8,32],default=1);parser.add_argument('--seconds',type=int,default=0);args=parser.parse_args()
if not 0<=args.seconds<=1800:parser.error('seconds must be 0..1800')
repo=Path(__file__).resolve().parents[1]
root=repo/'build/native-validation'/('realtime-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'studio-validation.marker').write_text('isolated realtime validation');(root/'temp').mkdir()
exe=repo/'build/backend/BackendProbe_artefacts/Release/Studio Backend Probe.exe'
snapshot=root/'probe.exe';shutil.copy2(exe,snapshot)
print('START',root,flush=True)
files=[snapshot,repo/'src/session.hpp',repo/'tests/backend/main.cpp',repo/'tests/backend/realtime_probe.hpp',repo/'dependencies.lock.json',Path(__file__)]
identity={str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
class Memory(ctypes.Structure):
 _fields_=[('cb',wintypes.DWORD),('faults',wintypes.DWORD)]+[(name,ctypes.c_size_t) for name in ['peak_working','working','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private']]
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
query=kernel.K32GetProcessMemoryInfo;query.argtypes=[wintypes.HANDLE,ctypes.POINTER(Memory),wintypes.DWORD];query.restype=wintypes.BOOL
samples=0;max_private=0;max_working=0;min_private=None
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 child=subprocess.Popen([str(snapshot),str(root),'--realtime',str(args.tracks),str(args.seconds)],env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err)
 start={'pid':child.pid,'tracks':args.tracks,'seconds':args.seconds,'sha256':identity};(root/'start.json').write_text(json.dumps(start,indent=2))
 deadline=time.monotonic()+args.seconds+240
 with (root/'memory.jsonl').open('w') as memorylog:
  while child.poll() is None:
   m=Memory();m.cb=ctypes.sizeof(m)
   if query(int(child._handle),ctypes.byref(m),m.cb):
    samples+=1;max_private=max(max_private,m.private);max_working=max(max_working,m.working);min_private=m.private if min_private is None else min(min_private,m.private)
    memorylog.write(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'private_bytes':m.private,'working_bytes':m.working})+'\n');memorylog.flush()
   if time.monotonic()>deadline:child.kill();child.wait();raise TimeoutError('Realtime child exceeded bounded timeout')
   time.sleep(1)
result_code=child.returncode
(root/'run.json').write_text(json.dumps({'exit_code':result_code,'sha256':identity,'memory_samples':samples,'max_private_bytes':max_private,'min_private_bytes':min_private,'max_working_bytes':max_working},indent=2))
print(root,'exit',result_code)
for name in ['FAIL.txt','realtime-result.json','stress-result.json']:
 if (root/name).exists():print((root/name).read_text(encoding='utf-8'))
raise SystemExit(result_code)
