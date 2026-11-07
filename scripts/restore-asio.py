"""Restore only the pinned official ASIO SDK; never install a driver."""
from pathlib import Path
import hashlib, json, urllib.request, zipfile
repo=Path(__file__).resolve().parents[1];lock=json.loads((repo/'dependencies.lock.json').read_text())['asio']
archive=repo/'build/asio-sdk-download.zip'
if not archive.exists():
 with urllib.request.urlopen(lock['archive_url'],timeout=60) as response:data=response.read()
 if hashlib.sha256(data).hexdigest()!=lock['archive_sha256']:raise RuntimeError('SDK download hash mismatch')
 archive.parent.mkdir(parents=True,exist_ok=True)
 with archive.open('xb') as f:f.write(data)
if hashlib.sha256(archive.read_bytes()).hexdigest()!=lock['archive_sha256']:raise RuntimeError('SDK archive hash mismatch')
dest=(repo/lock['local_path']).parent.resolve()
with zipfile.ZipFile(archive) as z:
 for info in z.infolist():
  target=(dest/info.filename).resolve()
  if not target.is_relative_to(dest):raise RuntimeError('Unsafe archive path')
  if info.is_dir():target.mkdir(parents=True,exist_ok=True);continue
  data=z.read(info)
  if target.exists():
   if target.read_bytes()!=data:raise RuntimeError('Existing SDK differs: '+str(target))
  else:
   target.parent.mkdir(parents=True,exist_ok=True)
   with target.open('xb') as f:f.write(data)
print('Pinned ASIO SDK files verified:',dest)
