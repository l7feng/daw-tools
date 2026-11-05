from pathlib import Path
import subprocess,os,datetime,json,sys,hashlib
r=Path(__file__).resolve().parents[1]
gui='--gui' in sys.argv
root=r/'build/native-validation'/(('interaction-' if '--interaction-ui-test' in sys.argv else 'workspace-' if '--workspace-ui-test' in sys.argv else 'ui-' if gui else 'plugin-ui-' if '--plugin-ui-test' in sys.argv else 'offline-')+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
root.mkdir(parents=True,exist_ok=False)
assert root.resolve().is_relative_to((r/'build').resolve())
(root/'studio-validation.marker').write_text('isolated workbench validation')
(root/'os-temp').mkdir()
env=dict(os.environ,TEMP=str(root/'os-temp'),TMP=str(root/'os-temp'))
build='editor' if '--editor' in sys.argv else 'engine'
exe=r/f'build/{build}/DawTools_artefacts/Release'/('DAW Tools Verified.exe' if '--verified' in sys.argv else 'DAW Tools.exe')
args=[str(exe),'--workspace',str(root)]+(['--gui-input-probe'] if gui and '--gui-input-probe' in sys.argv else [] if gui else ['--interaction-ui-test'] if '--interaction-ui-test' in sys.argv else ['--workspace-ui-test'] if '--workspace-ui-test' in sys.argv else ['--plugin-ui-test'] if '--plugin-ui-test' in sys.argv else ['--self-test'])
if gui:
 process=subprocess.Popen(args,env=env,cwd=r)
 (r/'build/last-ui-run.json').write_text(json.dumps({'root':str(root),'pid':process.pid}))
 print(root,'pid',process.pid)
else:
 result=subprocess.run(args,env=env,cwd=r,timeout=90,capture_output=True)
 (root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'scope':'offline only','source_sha256':{str(p.relative_to(r)).replace(chr(92),'/'):hashlib.sha256(p.read_bytes()).hexdigest() for base in ['src','tests'] for p in (r/base).glob('*') if p.is_file()}},indent=2))
 print(root,'exit',result.returncode)
 for name in ['PASS.txt','FAIL.txt','signal-result.json','editing/editor-result.json','crossfade/crossfade-result.json','mixer/mixer-result.json','export-tests/export-result.json','plugin-ui-result.json','workspace-result.json','interaction-result.json']:
  if (root/name).exists():print(name,(root/name).read_text())
 raise SystemExit(result.returncode)
