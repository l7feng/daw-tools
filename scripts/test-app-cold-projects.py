"""Repeat independent-process cold project cases with the actual application executable."""
from pathlib import Path
import argparse,datetime,hashlib,json,os,subprocess,uuid
parser=argparse.ArgumentParser();parser.add_argument('--verified',action='store_true');args=parser.parse_args()
repo=Path(__file__).resolve().parents[1];validation=repo/'build/native-validation'
source=next(folder for folder in sorted(validation.glob('backend-*'),reverse=True) if (folder/'run.json').is_file() and json.loads((folder/'run.json').read_text()).get('exit_code')==0 and (folder/'cold-projects/cold-projects-result.json').is_file() and json.loads((folder/'cold-projects/cold-projects-result.json').read_text()).get('checks',0)>=19)
exe=repo/'build/editor/DawTools_artefacts/Release'/('DAW Tools Verified.exe' if args.verified else 'DAW Tools.exe')
root=validation/('app-cold-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);root.mkdir()
summary=json.loads((source/'cold-projects/cold-projects-result.json').read_text());reports=[]
for row in summary['cases']:
    name=row['case'];folder=root/name;folder.mkdir();(folder/'temp').mkdir();(folder/'studio-validation.marker').write_text('actual application cold project validation')
    request=source/'cold-projects'/name/'cold-request.json';(folder/'cold-request.json').write_bytes(request.read_bytes())
    with (folder/'stdout.log').open('wb') as out,(folder/'stderr.log').open('wb') as err:
        result=subprocess.run([str(exe),'--workspace',str(folder),'--cold-project-test'],env=dict(os.environ,TEMP=str(folder/'temp'),TMP=str(folder/'temp')),stdout=out,stderr=err,timeout=40)
    if result.returncode:raise RuntimeError((name,(folder/'FAIL.txt').read_text() if (folder/'FAIL.txt').exists() else result.returncode))
    report=json.loads((folder/'cold-result.json').read_text());report['case']=name;reports.append(report)
    (root/'partial-result.json').write_text(json.dumps(reports,indent=2))
report={'checks':len(reports),'source':str(source),'cases':reports,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [repo/'src/session.hpp',repo/'src/workbench.cpp',repo/'tests/backend/cold_projects.hpp']},'physical_device_opened':False}
(root/'app-cold-result.json').write_text(json.dumps(report,indent=2));print(root);print(json.dumps({k:v for k,v in report.items() if k!='cases'},indent=2))
