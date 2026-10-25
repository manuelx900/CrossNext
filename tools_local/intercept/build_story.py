"""Build the bounded adaptation table and its English source strings."""
from pathlib import Path
import json,argparse,re
ROOT=Path(__file__).resolve().parents[2]
def generate(check=False):
 nodes=json.loads((ROOT/'assets_local/intercept/story.json').read_text(encoding='utf-8'))
 endings={'none':'None','free':'Free','suspicion':'Suspicion','captured':'Captured','escape':'Escape','betrayed':'Betrayed'}
 strings={};parts=['// Generated from assets_local/intercept/story.json.\n// Adapted from The Intercept, Copyright (c) 2016 inkle Ltd., MIT.\n// clang-format off\n#include "InterceptCore.h"\nnamespace intercept {\nconstexpr Node nodes[]={\n']
 for n,node in enumerate(nodes):
  title=f'STR_INTERCEPT_TITLE_{n}';text=f'STR_INTERCEPT_TEXT_{n}';strings[title]=node['title'];strings[text]=node['text'];choices=[]
  assert len(node['choices'])<=3 and node['ending'] in endings
  for k,c in enumerate(node['choices']):
   assert 0<=c['next']<len(nodes)
   key=f'STR_INTERCEPT_CHOICE_{n}_{k}';strings[key]=c['label']
   choices.append('{StrId::'+key+','+','.join(str(c.get(field,0)) for field in ('next','require','forbid','set','clear'))+'}')
  choices+=['{StrId::STR_NONE_OPT,0,0,0,0,0}']*(3-len(choices))
  parts.append('{StrId::'+title+',StrId::'+text+',{'+','.join(choices)+'},'+str(len(node['choices']))+','+str(node['scene'])+',Ending::'+endings[node['ending']]+'},\n')
 parts.append('};\nconst uint16_t nodeCount=sizeof(nodes)/sizeof(nodes[0]);\n}\n// clang-format on\n')
 out=ROOT/'src/apps_local/intercept/InterceptData.cpp';content=''.join(parts)
 yaml=ROOT/'lib/I18n/translations/english.yaml';source=yaml.read_text(encoding='utf-8');marker='STR_INTERCEPT_TITLE_0:'
 source='\n'.join(line for line in source.splitlines() if not re.match(r'^STR_INTERCEPT_(TITLE|TEXT|CHOICE)_',line))
 source=source.rstrip()+'\n\n'+''.join(f'{k}: {json.dumps(v,ensure_ascii=False)}\n' for k,v in strings.items())
 if check:
  assert out.read_text(encoding='utf-8')==content,'Story table is stale'
  assert yaml.read_text(encoding='utf-8')==source,'Story translations are stale'
 else:
  if not out.exists() or out.read_text(encoding='utf-8')!=content:out.write_text(content,encoding='utf-8',newline='\n')
  if yaml.read_text(encoding='utf-8')!=source:yaml.write_text(source,encoding='utf-8',newline='\n')
 print(len(nodes),'nodes,',len(strings),'translated strings')
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');generate(parser.parse_args().check)
