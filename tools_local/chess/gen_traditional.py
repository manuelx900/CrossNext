"""Original traditional silhouettes, rendered through the existing SVG asset pipeline."""
from pathlib import Path
import io
from PIL import Image, ImageFilter
import resvg_py
from gen_chess_pieces import silhouette,to_bytes,format_array
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'assets_local/pieces/traditional';OUT.mkdir(parents=True,exist_ok=True)
shapes={
'Pawn':'<circle cx="32" cy="15" r="9"/><path d="M27 25C27 36 24 42 17 48L13 52Q10 58 17 58H47Q54 58 51 52L47 48C40 42 37 36 37 25Z"/>',
'Knight':'<path d="M14 58H51L49 51H43C50 39 49 26 41 17L31 9L18 8L22 15L13 27Q9 34 17 34L29 27L24 38L16 47Z"/><path d="M29 16L30 21" fill="none"/>',
'Bishop':'<circle cx="32" cy="9" r="5"/><path d="M31 14C19 25 13 33 22 42L27 46L21 51H15L12 58H52L49 51H43L37 46L42 42C50 34 45 25 37 19L29 29L25 26L34 16Z"/>',
'Rook':'<path d="M14 9H22V18H28V9H36V18H42V9H50V29L43 32V49L49 51L51 58H13L15 51L21 49V32L14 29Z"/>',
'Queen':'<path d="M15 23L20 47H44L49 23L41 31L40 17L33 29L25 17L24 31Z"/><circle cx="14" cy="19" r="4"/><circle cx="25" cy="13" r="4"/><circle cx="40" cy="13" r="4"/><circle cx="50" cy="19" r="4"/><path d="M20 49H44L50 58H14Z"/>',
'King':'<path d="M29 4H35V10H41V16H35V23H29V16H23V10H29Z"/><path d="M31 25C21 15 10 25 12 36Q13 44 23 48L18 52L15 58H49L46 52L41 48Q51 43 52 36C54 25 43 15 33 25Z"/>'}
parts=['#pragma once\n#include <cstdint>\n// Generated from original SVG silhouettes by tools_local/chess/gen_traditional.py.\n// clang-format off\nnamespace traditional {\n']
codes=['Pawn','Knight','Bishop','Rook','Queen','King'];sheet=Image.new('L',(48*6,96),255)
for i,name in enumerate(codes):
 for dark in [False,True]:
  # Fill and outline share the silhouette, with a white interior for light pieces.
  svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><g fill="'+('black' if dark else 'white')+'" stroke="black" stroke-width="2.6" stroke-linejoin="round">'+shapes[name]+('<path d="M17 22L25 15L33 25L40 15L47 22L43 38H21Z"/>' if dark and name=='Queen' else '<path d="M28 16H36V30H28Z"/>' if dark and name=='King' else '')+'</g></svg>'
  (OUT/(('b' if dark else 'w')+name+'.svg')).write_text(svg)
  im=Image.open(io.BytesIO(resvg_py.svg_to_bytes(svg_string=svg,width=48,height=48,background='white'))).convert('L')
  bits=[[im.getpixel((x,y))<180 for x in range(48)]for y in range(48)]
  parts.append(format_array(name+('Dark' if dark else 'Light'),to_bytes(bits))+'\n')
  if not dark:parts.append(format_array(name+'Mask',to_bytes(silhouette(bits)))+'\n')
  else:
   expanded=im.point(lambda p:255 if p<180 else 0).filter(ImageFilter.MaxFilter(5))
   parts.append(format_array(name+'DarkMask',to_bytes([[expanded.getpixel((x,y))>0 for x in range(48)]for y in range(48)]))+'\n')
  sheet.paste(im,(i*48,48 if dark else 0))
for category in ['Light','Dark','Mask','DarkMask']:
 parts.append('inline const uint8_t* '+('darkMask' if category=='DarkMask' else category.lower())+'(int type) { switch(type) {'+''.join(f'case {i+1}:return {name}{category};' for i,name in enumerate(codes))+'default:return nullptr;} }\n')
parts.append('}\n// clang-format on\n')
(ROOT/'src/apps_local/chess/TraditionalPieces.h').write_text(''.join(parts),newline='\n');sheet.save(OUT/'preview.png')
print('Traditional chess silhouettes compiled')
