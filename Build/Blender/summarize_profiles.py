"""Summarize UE CSV profiler samples; ignore footer metadata, preserve raw CSVs."""
import csv
import json
import statistics
import sys
from pathlib import Path

def summarize(path):
    with path.open(newline='',encoding='utf-8-sig') as stream:
        rows=list(csv.DictReader(stream))
    output={'file':str(path),'metrics':{}}
    keys=[k for k in rows[0] if k and (k in {'FrameTime','GameThread','RenderThread','GPU','RHI/DrawCalls','GPUMem/LocalUsedMB'} or 'GPUTime' in k)]
    for key in keys:
        values=[]
        for row in rows:
            try:
                value=float(row[key])
                # Valid samples have a numeric FrameTime; metadata rows do not.
                float(row['FrameTime'])
                values.append(value)
            except (ValueError,TypeError,KeyError):
                continue
        values.sort()
        if values:
            output['metrics'][key]={'samples':len(values),'mean':statistics.mean(values),'p50':statistics.median(values),'p95':values[min(len(values)-1,int(len(values)*.95))]}
    return output

if __name__=='__main__':
    print(json.dumps([summarize(Path(p)) for p in sys.argv[1:]],indent=2))
