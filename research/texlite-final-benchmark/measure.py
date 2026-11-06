import json,os,subprocess,time,statistics,hashlib
from pathlib import Path
root=Path('/tmp/texlite-final-benchmark')
clock_ticks=os.sysconf('SC_CLK_TCK')
checkpoints=[0.10,0.25,0.50,1.0,2.0,4.0,5.0]
scenarios={
    'empty':[],
    'source_100':['/tmp/texstudio-baseline/source-100.tex'],
    'source_5000':['/tmp/texstudio-baseline/source-5000.tex'],
    'source_50000':['/tmp/texstudio-baseline/source-50000.tex'],
    'pdf_12pages':['--pdf-only','/tmp/texstudio-baseline/fixture.pdf']
}
binary=Path('/tmp/texlite-native/build/texlite')
initial_digest=hashlib.sha256(binary.read_bytes()).hexdigest()
assert initial_digest=='95f7aecaefc044cf138f0efe402e76bc91a990996336aea05475fd9ada45b328', 'Final binary hash does not match the frozen revision'
results=[]
for scenario,args in scenarios.items():
    for trial in range(1,4):
        assert hashlib.sha256(binary.read_bytes()).hexdigest()==initial_digest, 'Prototype binary changed during benchmark'
        run=root/'runs'/f'{scenario}-{trial}'
        run.mkdir(parents=True,exist_ok=True)
        config=run/'config';config.mkdir(exist_ok=True)
        env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_CACHE_HOME=str(root/'cache'),XDG_CONFIG_HOME=str(run/'xdg-config'),XDG_DATA_HOME=str(run/'xdg-data'))
        command=['/tmp/texlite-native/build/texlite']+args
        with (run/'stdout.log').open('w') as stdout,(run/'stderr.log').open('w') as stderr:
            start=time.monotonic()
            process=subprocess.Popen(command,env=env,stdin=subprocess.DEVNULL,stdout=stdout,stderr=stderr,cwd=run)
            samples=[]
            for target in checkpoints:
                time.sleep(max(0,start+target-time.monotonic()))
                if process.poll() is not None: break
                sample={'time_s':round(time.monotonic()-start,4)}
                try:
                    status={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path(f'/proc/{process.pid}/status').read_text().splitlines() if ':' in line}
                    for key in ['VmRSS','VmHWM','VmSize','Threads']:
                        sample[key]=int(status[key].split()[0])
                    parts=Path(f'/proc/{process.pid}/stat').read_text().rsplit(')',1)[1].split()
                    sample['cpu_seconds']=(int(parts[11])+int(parts[12]))/clock_ticks
                    try:
                        rollup={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path(f'/proc/{process.pid}/smaps_rollup').read_text().splitlines() if ':' in line}
                        sample['Pss_kib']=int(rollup['Pss'].split()[0])
                        sample['Private_kib']=sum(int(rollup[key].split()[0]) for key in ['Private_Clean','Private_Dirty'])
                    except (PermissionError,OSError,KeyError): sample['Pss_kib']=None
                except (ProcessLookupError,FileNotFoundError): break
                samples.append(sample)
            exit_before_stop=process.poll()
            if exit_before_stop is None:
                process.terminate()
                try: process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill();process.wait(timeout=2)
            assert hashlib.sha256(binary.read_bytes()).hexdigest()==initial_digest, 'Prototype binary changed during benchmark'
            item={'binary_sha256':initial_digest,'scenario':scenario,'trial':trial,'command':command,'exit_before_stop':exit_before_stop,'samples':samples,'stderr_log':str(run/'stderr.log')}
            results.append(item)
            (root/'measurements.json').write_text(json.dumps(results,indent=2))
            print(scenario,trial,json.dumps(samples[-1] if samples else {'early_exit':exit_before_stop}),flush=True)
summary={}
for scenario in scenarios:
    runs=[item for item in results if item['scenario']==scenario and item['samples']]
    if not runs:continue
    summary[scenario]={
        'trials':len(runs),
        'rss_mib_median_at_5s':statistics.median(item['samples'][-1]['VmRSS']/1024 for item in runs),
        'pss_mib_median_at_5s':statistics.median(item['samples'][-1]['Pss_kib']/1024 for item in runs if item['samples'][-1]['Pss_kib'] is not None),
        'private_mib_median_at_5s':statistics.median(item['samples'][-1]['Private_kib']/1024 for item in runs if 'Private_kib' in item['samples'][-1]),
        'peak_rss_mib_median':statistics.median(max(sample['VmHWM'] for sample in item['samples'])/1024 for item in runs),
        'cpu_seconds_median_at_5s':statistics.median(item['samples'][-1]['cpu_seconds'] for item in runs),
        'cpu_seconds_last_second_median':statistics.median(item['samples'][-1]['cpu_seconds']-item['samples'][-2]['cpu_seconds'] for item in runs),
        'threads_median_at_5s':statistics.median(item['samples'][-1]['Threads'] for item in runs)
    }
(root/'summary.json').write_text(json.dumps(summary,indent=2))
print('SUMMARY',json.dumps(summary),flush=True)
