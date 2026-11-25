"""Pack the supplied artwork for the Apps menu."""
from generate_home import ROOT, generate

if __name__ == '__main__':
    generate(ROOT / 'assets_local/app-menu-icons', ROOT / 'src/apps_local/ui/AppMenuIcons.h',
             namespace='appicons', sizes=(40,), inset=6)
