from pathlib import Path
import subprocess,os,json,datetime
r=Path(__file__).resolve().parents[1]
root=r/'build/native-validation'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
root.mkdir(parents=True,exist_ok=False)
assert root.resolve().is_relative_to((r/'build').resolve())
(root/'studio-validation.marker').write_text('isolated test run')
(root/'os-temp').mkdir()
env=dict(os.environ,STUDIO_TEST_ROOT=str(root),TEMP=str(root/'os-temp'),TMP=str(root/'os-temp'))
exe=r/'build/engine/third_party/tracktion_engine-3.2.0/examples/TestRunner/TestRunner_artefacts/Release/TestRunner.exe'
with (root/'upstream.log').open('w',encoding='utf-8') as log:
 result=subprocess.run([str(exe),'--junit-xml-file',str(root/'junit.xml')],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600,cwd=root)
(root/'run.json').write_text(json.dumps({'exit_code':result.returncode,'scope':'core/graph upstream tests only; no device initialisation','root':str(root)},indent=2))
print(root, 'exit',result.returncode)
raise SystemExit(result.returncode)
