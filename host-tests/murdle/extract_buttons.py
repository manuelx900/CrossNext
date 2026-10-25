"""Build the actual campaign-button branch against a host input shim."""
from pathlib import Path
import sys

source = Path('../../src/apps_local/murdle/MurdleActivity.cpp').read_text(encoding='utf8')
start = source.index('  if (view == View::Cases)', source.index('void MurdleActivity::loop()'))
end = source.index('  fui::InputSnapshot', start)
Path(sys.argv[1]).write_text(source[start:end], encoding='utf8')
start = source.index('      view = View::Cases;', source.index('case ui::ActionCampaign:'))
end = source.index('      break;', start)
Path(sys.argv[1]).with_name('campaign_entry.generated.inc').write_text(source[start:end], encoding='utf8')

start = source.index('    case ui::ActionCampaignPage:')
end = source.index('    case ui::ActionOpenCampaign:', start)
Path(sys.argv[1]).with_name('campaign_page.generated.inc').write_text(source[start:end], encoding='utf8')
start = source.index('    case ui::ActionTier:')
end = source.index('    default:', start)
Path(sys.argv[1]).with_name('campaign_filter.generated.inc').write_text(source[start:end], encoding='utf8')
