"""Build the submission PDF. Usage: python tools/build_pdf.py --font-dir PATH.

Requires ReportLab and DejaVuSans{,-Bold,Mono}.ttf in the selected directory.
Graph PNG is generated separately from docs/graph.dot using Graphviz.
"""
import argparse
import re
from pathlib import Path
from xml.sax.saxutils import escape
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                               TableStyle, Image, Preformatted, KeepTogether)

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--font-dir', type=Path, required=True)
    args = parser.parse_args()
    for name, file in [('Body', 'DejaVuSans.ttf'), ('Bold', 'DejaVuSans-Bold.ttf'),
                       ('Mono', 'DejaVuSansMono.ttf')]:
        pdfmetrics.registerFont(TTFont(name, str(args.font_dir / file)))
    pdfmetrics.registerFontFamily('Body', normal='Body', bold='Bold', italic='Body', boldItalic='Bold')
    ink, blue = colors.HexColor('#20324a'), colors.HexColor('#245d87')
    styles = {
        'body': ParagraphStyle('body', fontName='Body', fontSize=9.3, leading=13.5,
                               textColor=ink, spaceAfter=6),
        'title': ParagraphStyle('title', fontName='Bold', fontSize=23, leading=28,
                                textColor=ink, spaceAfter=16),
        'h2': ParagraphStyle('h2', fontName='Bold', fontSize=13.5, leading=19,
                             textColor=blue, spaceBefore=15, spaceAfter=9, keepWithNext=True),
        'cell': ParagraphStyle('cell', fontName='Body', fontSize=8, leading=11.5,
                               textColor=ink),
        'code': ParagraphStyle('code', fontName='Mono', fontSize=8, leading=12,
                               textColor=ink, backColor=colors.HexColor('#f0f4f8'),
                               borderPadding=8, spaceBefore=7, spaceAfter=12),
    }
    width = 487
    story = []
    lines = (ROOT/'SOLUTION.md').read_text().splitlines()
    i = 0
    def para(text, style='body'):
        text = escape(text)
        text = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', text)
        return Paragraph(text, styles[style])
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            i += 1
            continue
        if line.startswith('# '):
            story.append(para(line[2:], 'title'))
            i += 1
        elif line.startswith('## '):
            story.append(para(line[3:], 'h2'))
            i += 1
        elif line.startswith('```'):
            i += 1
            code = []
            while i < len(lines) and not lines[i].startswith('```'):
                code.append(lines[i]); i += 1
            story.append(Preformatted('\n'.join(code), styles['code']))
            i += 1
        elif line.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                cells = [x.strip() for x in lines[i].strip('|').split('|')]
                if not all(re.fullmatch(r'[- :]+', c) for c in cells):
                    rows.append([para(c, 'cell') for c in cells])
                i += 1
            count = len(rows[0])
            widths = {2: [175,312], 3: [225,155,107], 4: [60,170,150,107]}[count]
            if rows[0][0].text == "Этап":
                widths = [365, 122]
            table = Table(rows, colWidths=widths, repeatRows=1, hAlign='LEFT')
            table.setStyle(TableStyle([
                ('BACKGROUND',(0,0),(-1,0),colors.HexColor('#dce8f3')),
                ('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,colors.HexColor('#f4f7fa')]),
                ('VALIGN',(0,0),(-1,-1),'TOP'),
                ('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),8),
                ('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),
                ('LINEBELOW',(0,0),(-1,0),0.5,colors.HexColor('#94adc4')),
            ]))
            story.extend([KeepTogether([table, Spacer(1,10)]) if 'Этап' in str(rows[0][0].text) else table, Spacer(1,10)])
        elif line.startswith('!['):
            path = ROOT/re.search(r'\((.+)\)',line).group(1)
            w,h = ImageReader(str(path)).getSize()
            scale = min(width/w, 550/h)
            story.append(Image(str(path), width=w*scale, height=h*scale))
            i += 1
        elif line.startswith('https://'):
            label = 'Открыть официальный источник'
            story.append(Paragraph(f'<link href="{escape(line)}" color="#245d87">{label}</link>', styles['body']))
            i += 1
        else:
            paragraph = [line]
            i += 1
            while (i < len(lines) and lines[i].strip() and
                   not lines[i].startswith(('#','|','```','![','https://','- '))):
                paragraph.append(lines[i]); i += 1
            story.append(para(' '.join(paragraph)))
    def page(canvas, doc):
        canvas.saveState()
        canvas.setStrokeColor(colors.HexColor('#dce4ed'))
        canvas.line(54, 43, 541, 43)
        canvas.setFont('Body', 8)
        canvas.setFillColor(colors.HexColor('#61738a'))
        canvas.drawString(54, 29, 'Потоковая АКФ · RK3588 / Mali-G610')
        canvas.drawRightString(541, 29, str(doc.page))
        canvas.restoreState()
    doc = SimpleDocTemplate(str(ROOT/'SOLUTION.pdf'), pagesize=(595.28,841.89),
                            leftMargin=54,rightMargin=54,topMargin=45,bottomMargin=59,
                            title='Потоковая АКФ на RK3588 / Mali-G610',
                            author='Artem Gudzenko')
    doc.build(story, onFirstPage=page, onLaterPages=page)


if __name__ == '__main__':
    main()
