#!/usr/bin/env python3
"""Render the share card, from the page's own mark, type and screenshot.

The previous og.png was drawn with PIL against whatever system font matched
"Arial Bold", which is how it ended up not set in Jersey 25 at all. This lays
the card out in HTML and photographs it in the same browser that renders the
site, so the wordmark, the mark and the panel are the real ones by construction.

    uv run --with playwright python site/make-og.py

Writes assets/shots/og.png at 1200x630. Re-run it whenever the mark, the
headline or the shelf screenshot changes -- nothing on the page shows you this
image, so it is the one asset that goes stale silently.
"""

import asyncio
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / "assets" / "shots" / "og.png"

CARD = """
<style>
  @font-face { font-family:"Jersey 25"; src:url("assets/fonts/jersey25.woff2") format("woff2") }
  @font-face { font-family:"Instrument Serif"; src:url("assets/fonts/instrumentserif.woff2") format("woff2") }
  *{box-sizing:border-box} html,body{margin:0}
  body{width:1200px;height:630px;background:#faf9f6;color:#111110;overflow:hidden;
       display:grid;grid-template-columns:1fr 420px;align-items:center;
       font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
  .left{padding:0 0 0 76px}
  .wm{display:flex;align-items:center;gap:14px;font-family:"Jersey 25";font-size:40px;line-height:1}
  .wm svg{width:40px;height:40px}
  h1{font-family:"Jersey 25";font-weight:400;font-size:96px;line-height:.96;margin:38px 0 26px}
  p{font-family:"Instrument Serif";font-size:30px;line-height:1.35;margin:0;max-width:22ch;color:#2a2822}
  .meta{margin-top:40px;font:12px/1 ui-monospace,Menlo,monospace;letter-spacing:.18em;
        text-transform:uppercase;color:#5a584e;display:flex;gap:26px}
  .panel{height:630px;display:flex;align-items:center;justify-content:center;overflow:hidden}
  .panel img{width:330px;border:3px solid #111110;display:block;transform:rotate(4deg) translateY(14px)}
</style>
<div class="left">
  <div class="wm">
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" role="img" aria-label="CrossNext"><title>CrossNext</title><g fill="currentColor"><rect x="2" y="3" width="7" height="1"/><rect x="15" y="3" width="7" height="1"/><rect x="2" y="4" width="7" height="1"/><rect x="15" y="4" width="7" height="1"/><rect x="0" y="5" width="2" height="1"/><rect x="9" y="5" width="2" height="1"/><rect x="13" y="5" width="2" height="1"/><rect x="22" y="5" width="2" height="1"/><rect x="0" y="6" width="2" height="1"/><rect x="9" y="6" width="2" height="1"/><rect x="13" y="6" width="2" height="1"/><rect x="22" y="6" width="2" height="1"/><rect x="0" y="7" width="2" height="1"/><rect x="11" y="7" width="2" height="1"/><rect x="22" y="7" width="2" height="1"/><rect x="0" y="8" width="2" height="1"/><rect x="11" y="8" width="2" height="1"/><rect x="15" y="8" width="2" height="1"/><rect x="22" y="8" width="2" height="1"/><rect x="0" y="9" width="2" height="1"/><rect x="11" y="9" width="2" height="1"/><rect x="15" y="9" width="3" height="1"/><rect x="22" y="9" width="2" height="1"/><rect x="0" y="10" width="2" height="1"/><rect x="11" y="10" width="2" height="1"/><rect x="16" y="10" width="3" height="1"/><rect x="22" y="10" width="2" height="1"/><rect x="0" y="11" width="2" height="1"/><rect x="11" y="11" width="2" height="1"/><rect x="17" y="11" width="3" height="1"/><rect x="22" y="11" width="2" height="1"/><rect x="0" y="12" width="2" height="1"/><rect x="11" y="12" width="2" height="1"/><rect x="17" y="12" width="3" height="1"/><rect x="22" y="12" width="2" height="1"/><rect x="0" y="13" width="2" height="1"/><rect x="11" y="13" width="2" height="1"/><rect x="16" y="13" width="3" height="1"/><rect x="22" y="13" width="2" height="1"/><rect x="0" y="14" width="2" height="1"/><rect x="11" y="14" width="2" height="1"/><rect x="15" y="14" width="3" height="1"/><rect x="22" y="14" width="2" height="1"/><rect x="0" y="15" width="2" height="1"/><rect x="11" y="15" width="2" height="1"/><rect x="15" y="15" width="2" height="1"/><rect x="22" y="15" width="2" height="1"/><rect x="0" y="16" width="2" height="1"/><rect x="11" y="16" width="2" height="1"/><rect x="22" y="16" width="2" height="1"/><rect x="0" y="17" width="9" height="1"/><rect x="11" y="17" width="2" height="1"/><rect x="15" y="17" width="9" height="1"/><rect x="0" y="18" width="9" height="1"/><rect x="11" y="18" width="2" height="1"/><rect x="15" y="18" width="9" height="1"/><rect x="9" y="19" width="2" height="1"/><rect x="13" y="19" width="2" height="1"/><rect x="9" y="20" width="2" height="1"/><rect x="13" y="20" width="2" height="1"/><rect x="11" y="21" width="2" height="1"/><rect x="11" y="22" width="2" height="1"/></g></svg><span>CrossNext</span>
  </div>
  <h1>E-ink is good<br>at waiting.</h1>
  <p>Games, study and comics for the Xteink X4 Pro and the Seeed reTerminal Sticky.</p>
  <div class="meta"><span>A fork of CrossPlay and CrossPoint</span><span>MIT</span></div>
</div>
<div class="panel"><img src="assets/shots/games.png" alt=""></div>
"""


async def main():
    from playwright.async_api import async_playwright

    page_path = HERE / ".og-card.html"
    page_path.write_text(CARD)
    try:
        async with async_playwright() as p:
            browser = await p.chromium.launch(channel="chrome")
            page = await browser.new_page(
                viewport={"width": 1200, "height": 630}, device_scale_factor=1
            )
            await page.goto(page_path.as_uri(), wait_until="networkidle")
            # The webfonts load from disk here; give the layout a beat to settle
            # before the shutter, or the wordmark photographs in the fallback.
            await page.wait_for_timeout(600)
            await page.screenshot(path=str(OUT))
            await browser.close()
    finally:
        page_path.unlink(missing_ok=True)
    print(f"wrote {OUT.relative_to(HERE)} ({OUT.stat().st_size / 1024:.0f} KB)")


asyncio.run(main())
