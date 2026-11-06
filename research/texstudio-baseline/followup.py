import os,time,subprocess,json,re
from pathlib import Path
root=Path('/tmp/texstudio-baseline')
run=root/'long-source';run.mkdir(exist_ok=True)
config=run/'config';config.mkdir(exist_ok=True)
env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_CACHE_HOME=str(root/'cache'),XDG_CONFIG_HOME=str(run/'xdg-config'),XDG_DATA_HOME=str(run/'xdg-data'))
start=time.monotonic()
log=(run/'stderr.log').open('w')
p=subprocess.Popen(['/usr/bin/texstudio','--config',str(config),'--start-always','--no-session',str(root/'source-50000.tex')],cwd=run,env=env,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=log)
points=[.1,.25,.5,1,2,4,8,12,16,20]
samples=[];marker=None
while time.monotonic()-start<20.02 and p.poll() is None:
    now=time.monotonic()-start
    ini=config/'texstudio.ini'
    if marker is None and ini.exists() and 'startupCompletion=complete' in ini.read_text(): marker=now
    if points and now>=points[0]:
        target=points.pop(0)
        status={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path(f'/proc/{p.pid}/status').read_text().splitlines() if ':' in line}
        parts=Path(f'/proc/{p.pid}/stat').read_text().rsplit(')',1)[1].split()
        rollup={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path(f'/proc/{p.pid}/smaps_rollup').read_text().splitlines() if ':' in line}
        sample={'time_s':now,'rss_kib':int(status['VmRSS'].split()[0]),'pss_kib':int(rollup['Pss'].split()[0]),'cpu_s':(int(parts[11])+int(parts[12]))/os.sysconf('SC_CLK_TCK')}
        samples.append(sample);print(json.dumps(sample),flush=True)
    time.sleep(.01)
try: smaps=Path(f'/proc/{p.pid}/smaps').read_text()
except OSError:smaps=''
(run/'smaps.txt').write_text(smaps)
try:p.terminate();p.wait(timeout=2)
except subprocess.TimeoutExpired:p.kill();p.wait(timeout=2)
log.close()
data={'samples':samples,'startup_complete_marker_s':marker,'method':'single 20-second source-50,000 follow-up; marker time is not proof of first paint or interactive readiness'}
(root/'large-source-followup.json').write_text(json.dumps(data,indent=2))
# Aggregate observed mapping residency. This does not causally attribute allocations to a feature.
summary={}
category=''
for line in smaps.splitlines():
    if re.match(r'^[0-9a-f]+-[0-9a-f]+ ',line):
        parts=line.split(maxsplit=5)
        name=parts[5] if len(parts)>5 else '[anonymous]'
        if name.startswith('/usr/lib/libQt6'): category='Qt6 library mappings'
        elif 'poppler' in name: category='Poppler library mappings'
        elif name=='/usr/bin/texstudio': category='TeXstudio binary mappings'
        elif name=='[heap]': category='Heap'
        elif name.startswith('/'): category='Other file mappings'
        else:category='Other anonymous mappings'
    elif line.startswith(('Rss:','Pss:','Private_Clean:','Private_Dirty:')):
        key,value,*_=line.split()
        summary.setdefault(category,{}).setdefault(key[:-1],0)
        summary[category][key[:-1]]+=int(value)
(root/'large-source-mapping-summary.json').write_text(json.dumps(summary,indent=2))
print('MARKER',marker,flush=True)
