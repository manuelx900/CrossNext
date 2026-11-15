"""Bake catalogue Thai clusters into pairs understood by the existing CPFONT loader."""
import io
import re

import uharfbuzz as hb
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


def cluster_codepoints(texts, input_alphabet=False):
    # Leading vowels are spacing glyphs; dependent vowels and tones attach to
    # the preceding consonant. Sara Am participates in Thai substitutions.
    clusters = set()
    if input_alphabet:
        for mark in ('\u0e31', *map(chr,range(0x0e34,0x0e3b)), *map(chr,range(0x0e47,0x0e4f))):
            clusters.add('\u25cc' + mark)
        # Bounded consonant/vowel/tone combinations for user-entered names.
        # These supplement catalogue text; typed Thai must not depend on a
        # syllable having happened to occur in a translated menu label.
        for consonant in map(chr, range(0x0e01, 0x0e2f)):
            clusters.add(consonant + "\u0e4e")
            for vowel in ('', '\u0e31', *map(chr,range(0x0e34,0x0e3b)), '\u0e47', '\u0e4d'):
                for tone in ('', *map(chr,range(0x0e48,0x0e4d))):
                    word = consonant + vowel + tone
                    for length in range(2,len(word)+1): clusters.add(word[:length])
            for tone in ('', *map(chr,range(0x0e48,0x0e4d))):
                word = consonant + tone + '\u0e33'
                for length in range(2,len(word)+1): clusters.add(word[:length])
    for text in texts:
        for match in re.finditer(r'[\u0e01-\u0e2e][\u0e31\u0e34-\u0e3a\u0e47-\u0e4e]*\u0e33?', text):
            word = match.group()
            for length in range(2, len(word) + 1):
                clusters.add(word[:length])
    ordered = sorted(clusters, key=lambda text: (len(text), text))
    if len(ordered) > 4096:
        raise ValueError('Thai cluster subset exceeds the CPFONT ligature budget (4096)')
    return {text: 0xE000 + i for i, text in enumerate(ordered)}


def add_clusters(combined, source_path, weight, clusters):
    """Shape at build time, retaining Unicode source text and bounded runtime lookup."""
    source = TTFont(source_path)
    if 'fvar' in source:
        axes = {axis.axisTag: axis.defaultValue for axis in source['fvar'].axes}
        axes['wght'] = weight
        source = instantiateVariableFont(source, axes, inplace=True)
    stream = io.BytesIO()
    source.save(stream)
    face = hb.Face(stream.getvalue())
    font = hb.Font(face)
    font.scale = (face.upem, face.upem)
    glyphs = source.getGlyphSet()
    cmap = combined.getBestCmap()
    order = list(combined.getGlyphOrder())
    rules = []
    for text, cp in clusters.items():
        if cp in cmap:
            raise ValueError(f'Private-use catalogue collision: U+{cp:04X}')
        buffer = hb.Buffer()
        buffer.add_str(text)
        buffer.guess_segment_properties()
        buffer.language = 'th'
        hb.shape(font, buffer)
        pen = TTGlyphPen(combined.getGlyphSet())
        advance_x = advance_y = 0
        for info, position in zip(buffer.glyph_infos, buffer.glyph_positions):
            if info.codepoint == 0:
                raise ValueError(f'Missing Thai cluster glyph: {text}')
            recording = DecomposingRecordingPen(glyphs)
            glyphs[source.getGlyphName(info.codepoint)].draw(recording)
            transform = TransformPen(pen, (1, 0, 0, 1, advance_x + position.x_offset,
                                          advance_y + position.y_offset))
            recording.replay(transform)
            advance_x += position.x_advance
            advance_y += position.y_advance
        name = f'interfaceThai{cp:04X}'
        glyph = pen.glyph()
        combined['glyf'][name] = glyph
        glyph.recalcBounds(combined['glyf'])
        combined['hmtx'][name] = (advance_x, glyph.xMin if glyph.numberOfContours else 0)
        order.append(name)
        for table in combined['cmap'].tables:
            if table.isUnicode() and table.format in (4, 12, 13):
                table.cmap[cp] = name
        cmap[cp] = name
        rules.append('sub ' + ' '.join(cmap[ord(char)] for char in text) + f' by {name};')
    combined.setGlyphOrder(order)
    if rules:
        addOpenTypeFeaturesFromString(combined, 'languagesystem DFLT dflt; feature liga {\n' +
                                      '\n'.join(rules) + '\n} liga;')
    source.close()
    return len(rules)
