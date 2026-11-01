"""Run the current application workers against verified backend snapshots, no audio device."""
from pathlib import Path
import datetime, hashlib, json, os, shutil, struct, subprocess, uuid
import xml.etree.ElementTree as ET
repo=Path(__file__).resolve().parents[1]
validation=repo/'build/native-validation'
roots=sorted(validation.glob('backend-*'),reverse=True)
source=next(p for p in roots if (p/'run.json').is_file() and json.loads((p/'run.json').read_text()).get('case')=='full' and json.loads((p/'run.json').read_text()).get('exit_code')==0)
exe=repo/'build/editor/StudioWorkbench_artefacts/Release/Studio Workbench Verified.exe'
root=validation/('app-snapshots-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);root.mkdir()
reports=[]
for name,stem,frozen in [('plugin',False,False),('frozen',False,True),('stems',True,False)]:
    candidates=(source/('stem-jobs' if stem else 'render-guards')).glob('stems-*' if stem else 'export-*')
    def suitable(folder):
        status=folder/('manifest.json' if stem else 'job-status.json')
        if not status.is_file() or json.loads(status.read_text()).get('status')!='complete':return False
        # JUCE's ValueTree uses a literal base64: attribute prefix without XML namespaces.
        tree=ET.fromstring((folder/'p.tracktionedit').read_text(encoding='utf-8').replace('base64:','base64_'))
        return tree.find('.//PLUGIN[@type="vst"]') is not None and (tree.find('.//TRACK[@frozenIndividually="1"]') is not None)==frozen
    original=next(p for p in candidates if suitable(p))
    folder=root/name;folder.mkdir();(folder/'temp').mkdir();(folder/'studio-validation.marker').write_text('application worker validation')
    shutil.copy2(original/'p.tracktionedit',folder/'p.tracktionedit')
    if (original/'p.tracktionedit.assets').is_dir():shutil.copytree(original/'p.tracktionedit.assets',folder/'p.tracktionedit.assets')
    inputs=json.loads((original/'inputs.json').read_text())
    for item in inputs:
        path=Path(item['path'])
        if path.is_relative_to(original):path=folder/path.relative_to(original)
        item['path']=str(path)
        assert path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']
    (folder/'inputs.json').write_text(json.dumps(inputs,indent=2))
    request=json.loads((original/'request.json').read_text());request['timeoutMs']=15000;request['staging']=str(folder/'delivery.partial');(folder/'request.json').write_text(json.dumps(request,indent=2))
    with (folder/'stdout.log').open('wb') as out,(folder/'stderr.log').open('wb') as err:
        result=subprocess.run([str(exe),'--workspace',str(folder),'--stem-worker' if stem else '--render-worker'],env=dict(os.environ,TEMP=str(folder/'temp'),TMP=str(folder/'temp')),stdout=out,stderr=err,timeout=30)
    assert result.returncode==0, (folder,(folder/'FAIL.txt').read_text() if (folder/'FAIL.txt').exists() else result.returncode)
    output=folder/'delivery.partial'/'track-0.wav' if stem else folder/'delivery.partial'
    data=output.read_bytes();offset=12;channels=None;sample=None
    while offset+8<=len(data):
        tag=data[offset:offset+4];size=struct.unpack_from('<I',data,offset+4)[0];offset+=8
        if tag==b'fmt ':channels=struct.unpack_from('<H',data,offset+2)[0];assert struct.unpack_from('<H',data,offset+14)[0]==32
        if tag==b'data':sample=struct.unpack_from('<f',data,offset+12000*channels*4)[0];break
        offset+=size+(size%2)
    assert sample is not None and abs(sample-.00025)<1e-7,(name,sample)
    reports.append({'case':name,'source':str(original),'exit_code':result.returncode,'sample_at_12000':sample,'staged_sha256':hashlib.sha256(data).hexdigest(),'worker_staged_only':True})
report={'cases':reports,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'physical_device_opened':False}
(root/'app-snapshots-result.json').write_text(json.dumps(report,indent=2));print(root);print(json.dumps(report,indent=2))
