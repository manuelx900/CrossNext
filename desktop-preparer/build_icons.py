"""Generate desktop icons from the existing CrossNext mark."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent
assets = root / 'shell/assets'
mark = Image.open(assets / 'logo.png').convert('RGBA')
icon = Image.new('RGBA', (1024, 1024))
ImageDraw.Draw(icon).rounded_rectangle((24, 24, 1000, 1000), radius=218, fill='#242424')
white = Image.new('RGBA', mark.size, 'white')
white.putalpha(mark.getchannel('A'))
white = white.resize((680, 680), Image.Resampling.LANCZOS)
icon.alpha_composite(white, (172, 172))
icon.save(assets / 'icon.png')
icon.save(assets / 'icon.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
icon.save(assets / 'icon.icns')
print('Generated Windows and macOS icons.')
