"""Generate a printable A4 sheet of actual-size display outlines.

Print at 100% / "Actual size" and check the 50 mm calibration bar.

    python3 tools/size_cutouts.py docs/size-cutouts.pdf
"""

import math
import sys

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas

FONT = "Helvetica-Bold"
CAP_HEIGHT = 0.72  # Helvetica cap height as a fraction of font size

# (label, diagonal inches, resolution w, resolution h, shared two-player layout)
DISPLAYS = [
    ("1.47\" LCD (Waveshare C6)", 1.47, 320, 172, False),
    ("1.54\" IPS - the one you own", 1.54, 240, 240, False),
    ("1.8\" AMOLED", 1.8, 448, 368, False),
    ("1.9\" LCD (T-Display S3)", 1.9, 320, 170, False),
    ("2.0\" LCD (Estardyn)", 2.0, 320, 240, False),
    ("2.13\" e-paper / reflective", 2.13, 250, 122, False),
    ("2.41\" AMOLED", 2.41, 600, 450, False),
    ("2.7\" Sharp Memory LCD", 2.7, 400, 240, False),
    ("2.79\" bar LCD (Estardyn)", 2.79, 428, 142, False),
    ("2.8\" LCD (Cheap Yellow)", 2.8, 320, 240, False),
    ("2.9\" e-paper (WeAct)", 2.9, 296, 128, False),
    ("2.8\" LCD - shared", 2.8, 320, 240, True),
    ("3.5\" LCD - shared", 3.5, 480, 320, True),
    ("4.2\" reflective (Osptek) - shared", 4.2, 400, 300, True),
]

MARGIN = 10 * mm
GAP = 8 * mm
LABEL_H = 9 * mm


def active_area(diag_in, px_w, px_h):
    """Active area in points, assuming square pixels."""
    diag = diag_in * 25.4 * mm
    hyp = math.hypot(px_w, px_h)
    return diag * px_w / hyp, diag * px_h / hyp


def draw_number(c, text, cx, cy, box_w, box_h, upside_down=False):
    size = min(box_h * 0.7 / CAP_HEIGHT, box_w * 0.8 / stringWidth(text, FONT, 1))
    c.saveState()
    c.translate(cx, cy)
    if upside_down:
        c.rotate(180)
    c.setFont(FONT, size)
    c.drawCentredString(0, -size * CAP_HEIGHT / 2, text)
    c.restoreState()


def draw_display(c, x, y, w, h, label, diag, px_w, px_h, shared):
    c.setDash(3, 2)
    c.setLineWidth(0.6)
    c.rect(x, y, w, h)
    c.setDash()
    if shared:
        c.setLineWidth(0.3)
        c.line(x, y + h / 2, x + w, y + h / 2)
        draw_number(c, "20", x + w / 2, y + h * 0.25, w, h / 2)
        draw_number(c, "17", x + w / 2, y + h * 0.75, w, h / 2, upside_down=True)
    else:
        draw_number(c, "20", x + w / 2, y + h / 2, w, h)
    c.setFont("Helvetica", 7)
    c.drawString(x, y - 4 * mm, label)
    c.drawString(x, y - 7.5 * mm, f"{px_w}x{px_h}, {w / mm:.0f} x {h / mm:.0f} mm active area")


def main(path):
    page_w, page_h = A4
    c = canvas.Canvas(path, pagesize=A4)
    c.setTitle("BattleBuddy display size cutouts")

    c.setFont(FONT, 12)
    c.drawString(MARGIN, page_h - MARGIN - 4 * mm, "BattleBuddy - actual-size display cutouts")
    c.setFont("Helvetica", 8)
    c.drawString(MARGIN, page_h - MARGIN - 9 * mm,
                 "Print at 100% / Actual size. The bar below must measure 50 mm. "
                 "Outlines are the visible screen only; boards and cases are larger.")
    bar_y = page_h - MARGIN - 15 * mm
    c.setLineWidth(1.5)
    c.line(MARGIN, bar_y, MARGIN + 50 * mm, bar_y)
    c.setLineWidth(0.6)
    for i in range(6):
        c.line(MARGIN + i * 10 * mm, bar_y - 1.5 * mm, MARGIN + i * 10 * mm, bar_y + 1.5 * mm)
    c.drawString(MARGIN + 52 * mm, bar_y - 1 * mm, "50 mm")

    x = MARGIN
    row_top = bar_y - 8 * mm
    row_h = 0
    for label, diag, px_w, px_h, shared in DISPLAYS:
        w, h = active_area(diag, px_w, px_h)
        slot_w = max(w, stringWidth(label, "Helvetica", 7), 42 * mm)
        if x + w > page_w - MARGIN:
            x = MARGIN
            row_top -= row_h + LABEL_H + GAP
            row_h = 0
        if row_top - h - LABEL_H < MARGIN:
            c.showPage()
            x = MARGIN
            row_top = page_h - MARGIN
            row_h = 0
        draw_display(c, x, row_top - h, w, h, label, diag, px_w, px_h, shared)
        x += slot_w + GAP
        row_h = max(row_h, h)

    c.save()


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "size-cutouts.pdf")
