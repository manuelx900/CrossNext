"""Archive a generated master and its exact specification without modifying pixels."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'assets_local/quest/expansion'

def ingest(ident,source):
    catalog=json.loads((SOURCE/'catalog.json').read_text('utf8'))
    job=next(row for row in catalog['jobs'] if row['id']==ident)
    master=SOURCE/'masters'/job['master'];master.parent.mkdir(exist_ok=True)
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    with Image.open(source) as image:
        width,height=image.size
        assert width>=512 and height>=512,(ident,width,height)
        if job['transparent']:
            low,high=image.convert('RGBA').getchannel('A').getextrema()
            assert low==0 and high>=240,(ident,'not genuinely transparent')
        size=dict(width=width,height=height,mode=image.mode)
    if master.exists():
        assert hashlib.sha256(master.read_bytes()).hexdigest()==digest,'Refuse to replace a different master'
    else:shutil.copy2(source,master)
    provenance=SOURCE/'provenance';provenance.mkdir(exist_ok=True)
    record=dict(id=ident,master=job['master'],sha256=digest,tool='built-in image_gen',prompt=job['prompt'],reference=job['reference'],transparent=job['transparent'],**size)
    (provenance/(ident+'.json')).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n','utf8')
    print(ident,master.as_posix(),digest)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--id',required=True);parser.add_argument('--source',required=True,type=Path)
    args=parser.parse_args();ingest(args.id,args.source)
