"""Kill only our isolated capture child; recover complete PCM frames without editing the original."""
from pathlib import Path
import datetime, hashlib, json, os, struct, subprocess, time, uuid, wave
repo=Path(__file__).resolve().parents[1]
root=repo/'build/native-validation'/('capture-recovery-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
root.mkdir(parents=True);(root/'studio-validation.marker').write_text('isolated crash recovery validation');(root/'temp').mkdir()
exe=repo/'build/backend/BackendProbe_artefacts/Release/Studio Backend Probe.exe'
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 child=subprocess.Popen([str(exe),str(root),'--capture','1024','30'],env=dict(os.environ,TEMP=str(root/'temp'),TMP=str(root/'temp')),stdout=out,stderr=err)
 deadline=time.monotonic()+15;observed=0
 try:
  while child.poll() is None and time.monotonic()<deadline:
   try: observed=int((root/'capture-progress.txt').read_text())
   except (FileNotFoundError,ValueError,PermissionError): pass
   if observed>=96000:break
   time.sleep(.05)
  if observed<96000 or child.poll() is not None:raise RuntimeError('Child did not reach capture checkpoint')
 finally:
  if child.poll() is None:child.kill()
  child.wait(timeout=10)
 original=root/'captured.wav';data=original.read_bytes();original_hash=hashlib.sha256(data).hexdigest()
 if data[:4]!=b'RIFF' or data[8:12]!=b'WAVE':raise RuntimeError('Not a RIFF WAV')
 offset=12;fmt=None;data_start=None
 while offset+8<=len(data):
  kind=data[offset:offset+4];size=struct.unpack_from('<I',data,offset+4)[0]
  if kind==b'fmt ':fmt=struct.unpack_from('<HHIIHH',data,offset+8)
  if kind==b'data':data_start=offset+8;data_size_offset=offset+4;break
  offset+=8+size+(size&1)
 if fmt is None or data_start is None or fmt[0]!=1 or fmt[1]!=2 or fmt[2]!=48000 or fmt[4]!=6 or fmt[5]!=24:raise RuntimeError('Unexpected PCM layout')
 whole=(len(data)-data_start)//6*6
 if whole<48000*6:raise RuntimeError('Insufficient complete frames')
 recovered=bytearray(data[:data_start+whole]);struct.pack_into('<I',recovered,4,len(recovered)-8);struct.pack_into('<I',recovered,data_size_offset,whole)
 target=root/'recovered.wav'
 with target.open('xb') as f:f.write(recovered)
 with wave.open(str(target),'rb') as f:
  frames=f.getnframes();payload=f.readframes(frames)
 if frames!=whole//6 or payload!=data[data_start:data_start+whole]:raise RuntimeError('Recovered PCM differs')
 if hashlib.sha256(original.read_bytes()).hexdigest()!=original_hash:raise RuntimeError('Original modified')
 report={'child_exit':child.returncode,'observed_frames_before_kill':observed,'recovered_frames':frames,'recovered_seconds':frames/48000,'original_unchanged':True,'pcm_bytes_equal':True,'original_sha256':original_hash,'recovered_sha256':hashlib.sha256(recovered).hexdigest(),'sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [exe,repo/'tests/backend/device_probe.hpp',Path(__file__)]}}
 (root/'recovery-result.json').write_text(json.dumps(report,indent=2));print(root);print(json.dumps(report,indent=2))
