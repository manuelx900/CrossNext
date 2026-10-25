"""Deterministic CC0 geometric puzzles; every grid is proved uniquely line-solvable."""
from pathlib import Path
import json, random
import gen_picross as gen

root=Path(__file__).resolve().parents[2]
existing=[p for p in gen.parse(str(root/'assets_local/picross/nonograms.txt')) if len(p[1]) in (5,8,9,10)]
seen={tuple(tuple(row) for row in cells) for _,cells,_ in existing}
rng=random.Random(0x63726f7373706c61)
extra=[]
for n,target in ((5,100),(8,100),(9,100),(10,150)):
    need=target-sum(len(cells)==n for _,cells,_ in existing)
    found=0
    while found<need:
        # Overlapping filled rectangles, bars and cuts form simple geometric motifs.
        cells=[[0]*n for _ in range(n)]
        for _ in range(rng.randrange(3,8)):
            x,y=rng.randrange(n),rng.randrange(n)
            w,h=rng.randrange(1,n-x+1),rng.randrange(1,n-y+1)
            for r in range(y,y+h):
                for c in range(x,x+w):cells[r][c]=1
        key=tuple(tuple(row) for row in cells)
        if key in seen:continue
        ok,_=gen.evaluate('',cells)
        if not ok:continue
        seen.add(key);extra.append({'size':n,'cells':cells});found+=1
    print(n,target,flush=True)
(root/'assets_local/picross/extra-geometric.json').write_text(json.dumps(extra,separators=(',',':'))+'\n')
