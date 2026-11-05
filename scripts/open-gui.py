"""Launch the latest development GUI without opening an audio device."""
from pathlib import Path
import subprocess,os,uuid,ctypes,argparse
parser=argparse.ArgumentParser();parser.add_argument('--verified',action='store_true');args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
exe=root/'build/editor/DawTools_artefacts/Release'/('DAW Tools Verified.exe' if args.verified else 'DAW Tools.exe')
if not exe.is_file():
    ctypes.windll.user32.MessageBoxW(None,'请先构建 DAW Tools。','DAW Tools',16)
    raise SystemExit(1)
workspace=root/'build/gui-sessions'/uuid.uuid4().hex
workspace.mkdir(parents=True)
(workspace/'studio-validation.marker').write_text('desktop GUI workspace')
(workspace/'temp').mkdir()
env=dict(os.environ,TEMP=str(workspace/'temp'),TMP=str(workspace/'temp'))
subprocess.Popen([str(exe),'--workspace',str(workspace)],cwd=root,env=env)
