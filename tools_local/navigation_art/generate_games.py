"""Pack supplied game artwork at the framed Shelf menu's native icon size."""
from generate_home import ROOT, generate

if __name__ == '__main__':
    generate(ROOT / 'assets_local/game-menu-icons', ROOT / 'src/apps_local/ui/GameMenuIcons.h',
             namespace='gameicons', sizes=(40,), inset=6)
