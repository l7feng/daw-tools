from pathlib import Path
import subprocess,os,datetime,json,sys,hashlib
r=Path(__file__).resolve().parents[1]
gui='--gui' in sys.argv
root=r/'build/native-validation'/(('ui-' if gui else 'offline-')+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
root.mkdir(parents=True,exist_ok=False)
assert root.resolve().is_relative_to((r/'build').resolve())
(root/'studio-validation.marker').write_text('isolated workbench validation')
(root/'os-temp').mkdir()
env=dict(os.environ,TEMP=str(root/'os-temp'),TMP=str(root/'os-temp'))
exe=r/'build/engine/StudioWorkbench_artefacts/Release/Studio Workbench.exe'
args=[str(exe),'--workspace',str(root)]+([] if gui else ['--self-test'])
if gui:
 process=subprocess.Popen(args,env=env,cwd=r)
 (r/'build/last-ui-run.json').write_text(json.dumps({'root':str(root),'pid':process.pid}))
 print(root,'pid',process.pid)
else:
 result=subprocess.run(args,env=env,cwd=r,timeout=90,capture_output=True)
 (root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'scope':'offline only'},indent=2))
 print(root,'exit',result.returncode)
 for name in ['PASS.txt','FAIL.txt','signal-result.json']:
  if (root/name).exists():print(name,(root/name).read_text())
 raise SystemExit(result.returncode)
