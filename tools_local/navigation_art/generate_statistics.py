"""Pack the statistics book artwork at its two actual display sizes."""
from generate_home import ROOT, generate


if __name__ == '__main__':
    generate(ROOT / 'assets_local/statistics-icons', ROOT / 'src/apps_local/ui/StatisticsIcons.h',
             namespace='statisticsicons', sizes=(54, 80), inset=4)
