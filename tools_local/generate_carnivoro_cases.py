from pathlib import Path
from itertools import combinations
import random,json
r=random.Random(0xCA72126);names=['Guilty','Innocent','Either','Both','Truth','Lie','SameTruth','DifferentTruth','Carnivore','Herbivore','Taller','Shorter','Neighbour'];heights=[180,165,195,172,205,188,198]
def evaluate(s,i,g,t,n):
 k,a,b=s;ga=bool(g>>a&1);gb=bool(g>>b&1);ta=bool(t>>a&1);tb=bool(t>>b&1)
 if k==0:return ga
 if k==1:return not ga
 if k==2:return ga or gb
 if k==3:return ga and gb
 if k==4:return ta
 if k==5:return not ta
 if k==6:return ta==tb
 if k==7:return ta!=tb
 if k==8:return bool(g&5)
 if k==9:return bool(g&96)
 if k==10:return any(g>>j&1 and heights[j]>heights[i] for j in range(n))
 if k==11:return any(g>>j&1 and heights[j]<heights[i] for j in range(n))
 return bool(g&((1<<((i+1)%n))|(1<<((i+n-1)%n))))
def solutions(n,c,l,ss):
 out=[]
 for comb in combinations(range(n),c):
  g=sum(1<<i for i in comb)
  for t in range(1<<n):
   if t.bit_count()!=n-l:continue
   if all(evaluate(s,i,g,t,n)==bool(t>>i&1) for i,s in enumerate(ss)):
    out.append((g,t))
    if len(out)>1:return out
 return out
def semantic(n,c,l,ss):
 result=[]
 for k,a,b in ss:
  if k>=8:a=b=0
  elif k in (0,1,4,5):b=0
  elif a>b:a,b=b,a
  result.append((k,a,b))
 return n,c,l,tuple(result)
cases=[];seen=set();meaning=set();tries=0
import argparse,re
parser=argparse.ArgumentParser(description='Generate the reproducible Carnivoro case catalogue.')
mode=parser.add_mutually_exclusive_group()
mode.add_argument('--extend-beginner',action='store_true',help='Retain the original 1000 cases before appending the expansions.')
mode.add_argument('--extend-progression',action='store_true',help='Retain the original 1200 cases before adding the progressive culprit groups.')
parser.add_argument('--output',type=Path,default=Path('src/apps_local/carnivoro/CarnivoroCases.h'))
args=parser.parse_args()
if args.extend_beginner or args.extend_progression:
 keep=1200 if args.extend_progression else 1000
 data=Path('src/apps_local/carnivoro/CarnivoroCases.h').read_text(encoding='utf-8')
 rows=re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*\{(.*?)\}\s*\},',data,re.S)
 for n,c,l,body in rows[:keep]:
  ss=[(names.index(k),int(a),int(b)) for k,a,b in re.findall(r'Claim::(\w+),\s*(\d+),\s*(\d+)',body)]
  assert len(ss)==int(n)
  cases.append(dict(count=int(n),culprits=int(c),lies=int(l),statements=ss))
 assert len(cases)==keep
else:
 for case in range(1000):
  n=(3 if case<20 else 4 if case<50 else 5 if case<80 else 7) if case<100 else (4 if case<300 else 6 if case<650 else 7);c=2 if 80<=case<100 or case>=450 else 1
  if case==0:ss=[(0,2,0),(1,0,0),(0,0,0)];l=1;sol=solutions(n,c,l,ss)
  else:
   while True:
    tries+=1;l=r.randrange(1,n)
    ss=[]
    for i in range(n):
     allowed=[0,1,2,8,10,11,12] if case<20 else list(range(13))
     if n<6:allowed=[k for k in allowed if k!=9]
     k=r.choice(allowed);a=r.randrange(n);b=r.randrange(n)
     if k in (4,5) and a==i:a=(a+1)%n
     if k in (2,3,6,7) and a==b:b=(b+1)%n
     ss.append((k,a,b))
    key=(n,c,l,tuple(ss))
    if key in seen or (case>=100 and semantic(n,c,l,ss) in meaning):continue
    sol=solutions(n,c,l,ss)
    if len(sol)==1:seen.add(key);break
  meaning.add(semantic(n,c,l,ss))
  cases.append(dict(count=n,culprits=c,lies=l,statements=ss,solution=sol[0]))
  if (case+1)%100==0:print(case+1,"validated cases",flush=True)

# Appended IDs preserve every original puzzle and existing save reference.
meaning={semantic(c['count'],c['culprits'],c['lies'],c['statements']) for c in cases}
beginner=random.Random(0xCA720200)
while len(cases)<1200:
 n=3;c=1;l=beginner.randrange(1,3);ss=[]
 for i in range(n):
  k=beginner.choice([0,1,2,8,10,11,12]);a=beginner.randrange(n);b=beginner.randrange(n)
  if k==2 and a==b:b=(b+1)%n
  ss.append((k,a,b))
 key=semantic(n,c,l,ss)
 if key in meaning:continue
 sol=solutions(n,c,l,ss)
 if len(sol)!=1:continue
 meaning.add(key);cases.append(dict(count=n,culprits=c,lies=l,statements=ss,solution=sol[0]))
progression=random.Random(0xCA721470)
for n,c in [(4,2),(7,1)]:
 for l in range(1,n):
  added=0
  while added<30:
   tries+=1;ss=[]
   for i in range(n):
    allowed=[k for k in range(13) if n>=6 or k!=9]
    k=progression.choice(allowed);a=progression.randrange(n);b=progression.randrange(n)
    if k in (4,5) and a==i:a=(a+1)%n
    if k in (2,3,6,7) and a==b:b=(b+1)%n
    ss.append((k,a,b))
   key=semantic(n,c,l,ss)
   if key in meaning:continue
   sol=solutions(n,c,l,ss)
   if len(sol)!=1:continue
   meaning.add(key);cases.append(dict(count=n,culprits=c,lies=l,statements=ss,solution=sol[0]));added+=1
  print(len(cases),'validated cases;',n,'characters;',c,'culprits;',l,'liars',flush=True)
assert len(cases)==1470
lines=['#pragma once','#include "CarnivoroCore.h"','namespace carnivoro {','// Original cases generated offline; the host suite exhaustively checks every world.','inline constexpr Case kCaseData[kCases] = {']
for case in cases:
 ss=', '.join('{Claim::%s, %d, %d}'%(names[k],a,b) for k,a,b in case['statements'])
 lines.append('  {%d, %d, %d, {%s}},'%(case['count'],case['culprits'],case['lies'],ss))
lines.extend(['};','}  // namespace carnivoro'])
args.output.write_text('\n'.join(lines)+'\n',encoding='utf-8')

print('1470 original full-world cases generated;',tries,'candidates;',len(meaning),'distinct statement sets')
