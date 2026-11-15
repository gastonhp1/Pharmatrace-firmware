#!/usr/bin/env python3
"""Genera docs/seal-loop.{es,en}.svg: cómo es el lazo del precinto (vista física, hebilla y circuito).

    python3 tools/gen_seal_loop.py es docs/seal-loop.es.svg
    python3 tools/gen_seal_loop.py en docs/seal-loop.en.svg
"""
import sys

LANG, OUT = sys.argv[1], sys.argv[2]
EN = {
    "El lazo del precinto": "The seal loop",
    "1 · En la caja (corte lateral)": "1 · On the box (side cut)",
    "Módulo": "Module",
    "Cinto": "Strap",
    "Cable adentro del cinto": "Wire inside the strap",
    "Extremo fijo: el cable entra al módulo": "Fixed end: the wire enters the module",
    "Plug enchufado en el jack del módulo": "Plug in the module's jack",
    "Caja de telgopor": "Styrofoam box",
    "Tapa": "Lid",
    "2 · La hebilla eléctrica": "2 · The electrical buckle",
    "Jack en el módulo": "Jack on the module",
    "Plug de 3,5 mm en la punta del cinto": "3.5 mm plug at the end of the strap",
    "10 kΩ adentro del plug,": "10 kΩ inside the plug,",
    "entre punta y masa": "between tip and sleeve",
    "punta": "tip",
    "masa": "sleeve",
    "cable del cinto": "strap wire",
    "a GND": "to GND",
    "3 · El circuito": "3 · The circuit",
    "cable a lo largo del cinto": "wire along the strap",
    "en el plug": "in the plug",
    "en el módulo": "on the module",
    "(mide tensión)": "(measures voltage)",
    "Lo que mide el GPIO4": "What GPIO4 reads",
    "Enchufado": "Plugged in",
    "cerrado": "closed",
    "Desenchufado o cinto cortado": "Unplugged or strap cut",
    "abierto → violación": "open → breach",
    "Punta puenteada a masa con un cable": "Tip bridged to ground with a wire",
    "falta la resistencia → violación": "resistor missing → breach",
    "No detecta un puente sobre un tramo del cable que no incluya el plug: para eso está el sensor de luz adentro.":
        "It cannot detect a jumper across a stretch of wire that does not include the plug: the light sensor inside covers that.",
}
t = lambda s: s if LANG == "es" else EN[s]
num = lambda s: s if LANG == "es" else s.replace(",", ".")
o = []
a = o.append
W, H = 1100, 790
BROWN, ORANGE, RED, BLK, BLUE, GOLD, GREEN = "#8d5524", "#e07b00", "#d62828", "#111111", "#1d6fd8", "#ad8b00", "#2a9d3f"
a(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="Helvetica, Arial, sans-serif">')
a(f'<rect width="{W}" height="{H}" fill="#ffffff"/>')


def text(x, y, s, size=12, weight="normal", fill="#1a1a1a", anchor="start"):
    a(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{s}</text>')


def callout(x, y, tx, ty, s, col="#333", anchor="start"):
    a(f'<line x1="{x}" y1="{y}" x2="{tx}" y2="{ty}" stroke="{col}" stroke-width="1.5"/>')
    a(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{col}"/>')
    text(tx + (6 if anchor == "start" else -6), ty + 4, s, 12, "bold", anchor=anchor)


def resistor(x, y, w=70, label="10 kΩ", vertical=False):
    if vertical:
        a(f'<rect x="{x-11}" y="{y}" width="22" height="{w}" rx="3" fill="#fff8e1" stroke="{BLK}" stroke-width="2"/>')
        text(x + 18, y + w / 2 + 4, label, 12, "bold")
    else:
        a(f'<rect x="{x}" y="{y-11}" width="{w}" height="22" rx="3" fill="#fff8e1" stroke="{BLK}" stroke-width="2"/>')
        text(x + w / 2, y - 18, label, 12, "bold", anchor="middle")


text(W // 2, 34, t("El lazo del precinto"), 21, "bold", anchor="middle")

# ------------------------------------------------------------------ 1. vista física
text(40, 76, t("1 · En la caja (corte lateral)"), 14, "bold", "#1d4e89")
BX0, BX1, BY0, BY1, LID = 150, 470, 170, 380, 212
SX0, SX1, SY0, SY1 = BX0 - 12, BX1 + 12, BY0 - 12, BY1 + 12  # recorrido del cinto
MX0, MX1, MY0, MY1 = 250, 370, 118, BY0 - 14  # módulo
# cinto: sale del lado izquierdo del módulo, rodea la caja y vuelve por la derecha
strap = f"M{MX0} {MY1-10} L{SX0+10} {MY1-10} Q{SX0} {MY1-10} {SX0} {SY0+12} L{SX0} {SY1-10} Q{SX0} {SY1} {SX0+10} {SY1} L{SX1-10} {SY1} Q{SX1} {SY1} {SX1} {SY1-10} L{SX1} {SY0+12} Q{SX1} {MY1-10} {SX1-10} {MY1-10} L{MX1+34} {MY1-10}"
a(f'<path d="{strap}" fill="none" stroke="{BROWN}" stroke-width="11" stroke-linecap="butt"/>')
a(f'<path d="{strap}" fill="none" stroke="{ORANGE}" stroke-width="2.5" stroke-dasharray="7 5"/>')
# caja y tapa
a(f'<rect x="{BX0}" y="{LID}" width="{BX1-BX0}" height="{BY1-LID}" fill="#f1f3f5" stroke="#868e96" stroke-width="2"/>')
a(f'<rect x="{BX0+26}" y="{LID}" width="{BX1-BX0-52}" height="{BY1-LID-26}" fill="#ffffff" stroke="#adb5bd" stroke-width="1.5"/>')
a(f'<rect x="{BX0}" y="{BY0}" width="{BX1-BX0}" height="{LID-BY0-3}" fill="#e9ecef" stroke="#868e96" stroke-width="2"/>')
text(BX0 + 12, BY0 + 26, t("Tapa"), 11.5, "bold", "#495057")
text((BX0 + BX1) // 2, 300, t("Caja de telgopor"), 12, "bold", "#868e96", "middle")
# módulo, jack y plug
a(f'<rect x="{MX0}" y="{MY0}" width="{MX1-MX0}" height="{MY1-MY0}" rx="7" fill="#1f2937" stroke="#0b1220" stroke-width="2"/>')
text((MX0 + MX1) // 2, MY0 + 24, t("Módulo"), 12, "bold", "#ffffff", "middle")
a(f'<rect x="{MX1-4}" y="{MY1-18}" width="10" height="16" fill="#9aa4b2" stroke="#6b7280" stroke-width="1.5"/>')  # jack
a(f'<rect x="{MX1+6}" y="{MY1-14}" width="12" height="8" fill="#d4af37"/>')                                   # punta
a(f'<rect x="{MX1+18}" y="{MY1-18}" width="18" height="16" rx="3" fill="{GOLD}" stroke="#6b5600" stroke-width="1.5"/>')  # cuerpo
callout(MX0 - 2, MY1 - 10, 40, 100, t("Extremo fijo: el cable entra al módulo"), BROWN)
callout(MX1 + 27, MY1 - 18, 430, 100, t("Plug enchufado en el jack del módulo"), GOLD)
callout(SX0, 290, 60, 420, t("Cinto"), BROWN)
callout(SX1, 300, 400, 420, t("Cable adentro del cinto"), ORANGE)

# ------------------------------------------------------------------ 2. hebilla
text(600, 76, t("2 · La hebilla eléctrica"), 14, "bold", "#1d4e89")
JX, JY = 640, 190
a(f'<rect x="{JX}" y="{JY-45}" width="90" height="90" rx="8" fill="#e5e7eb" stroke="#6b7280" stroke-width="2"/>')
text(JX + 45, JY - 56, t("Jack en el módulo"), 12, "bold", anchor="middle")
a(f'<rect x="{JX+60}" y="{JY-12}" width="30" height="24" fill="#9aa4b2" stroke="#6b7280" stroke-width="1.5"/>')
a(f'<line x1="{JX+12}" y1="{JY+26}" x2="{JX+60}" y2="{JY+12}" stroke="{BLK}" stroke-width="2.5"/>')
text(JX + 8, JY + 40, t("a GND"), 11, "bold")
# plug separado, a punto de entrar
PX = 800
a(f'<rect x="{PX}" y="{JY-6}" width="52" height="12" rx="5" fill="#d4af37" stroke="#8a6d00" stroke-width="1.5"/>')      # punta
a(f'<rect x="{PX+18}" y="{JY-6}" width="4" height="12" fill="#333"/>')                                               # aislante
a(f'<rect x="{PX+52}" y="{JY-24}" width="150" height="48" rx="8" fill="#fff8e1" stroke="{GOLD}" stroke-width="2.5"/>')  # cuerpo
text(PX + 127, JY - 34, t("Plug de 3,5 mm en la punta del cinto"), 12, "bold", anchor="middle")
resistor(PX + 78, JY, 60, "")
text(PX + 108, JY + 4, "10 kΩ", 10.5, "bold", anchor="middle")
a(f'<line x1="{PX+52}" y1="{JY}" x2="{PX+78}" y2="{JY}" stroke="{ORANGE}" stroke-width="2.5"/>')
a(f'<line x1="{PX+138}" y1="{JY}" x2="{PX+160}" y2="{JY}" stroke="{BLK}" stroke-width="2.5"/>')
a(f'<path d="M{PX+160} {JY} L{PX+160} {JY+17} L{PX+30} {JY+17} L{PX+30} {JY+6}" fill="none" stroke="{BLK}" stroke-width="2"/>')
a(f'<rect x="{PX+202}" y="{JY-9}" width="60" height="18" fill="{BROWN}"/>')
a(f'<line x1="{PX+66}" y1="{JY-14}" x2="{PX+262}" y2="{JY-14}" stroke="{ORANGE}" stroke-width="2.5" stroke-dasharray="7 5"/>')
a(f'<line x1="{PX+66}" y1="{JY-14}" x2="{PX+66}" y2="{JY}" stroke="{ORANGE}" stroke-width="2.5"/>')
text(PX + 232, JY + 30, t("cable del cinto"), 11, "bold", ORANGE, "middle")
text(PX + 8, JY + 26, t("punta"), 10.5, fill="#555", anchor="middle")
text(PX + 36, JY + 40, t("masa"), 10.5, fill="#555", anchor="middle")
a(f'<path d="M{PX-10} {JY} L{JX+100} {JY}" stroke="#999" stroke-width="2" stroke-dasharray="4 4" marker-end="url(#arr)"/>')
a('<defs><marker id="arr" markerWidth="10" markerHeight="8" refX="9" refY="4" orient="auto"><path d="M0 0 L10 4 L0 8 z" fill="#999"/></marker></defs>')
text(PX + 127, JY + 62, t("10 kΩ adentro del plug,"), 11.5, fill="#333", anchor="middle")
text(PX + 127, JY + 78, t("entre punta y masa"), 11.5, fill="#333", anchor="middle")

# ------------------------------------------------------------------ 3. circuito
text(40, 476, t("3 · El circuito"), 14, "bold", "#1d4e89")
CY = 560
a(f'<circle cx="80" cy="{CY}" r="6" fill="{RED}"/>')
text(80, CY - 16, "3V3", 13, "bold", RED, "middle")
a(f'<line x1="86" y1="{CY}" x2="140" y2="{CY}" stroke="{RED}" stroke-width="3"/>')
resistor(140, CY, 80)
text(180, CY + 30, t("en el módulo"), 11, fill="#555", anchor="middle")
a(f'<line x1="220" y1="{CY}" x2="300" y2="{CY}" stroke="{ORANGE}" stroke-width="3"/>')
a(f'<circle cx="300" cy="{CY}" r="6" fill="{ORANGE}"/>')
a(f'<line x1="300" y1="{CY}" x2="300" y2="{CY+60}" stroke="{ORANGE}" stroke-width="3"/>')
a(f'<rect x="250" y="{CY+60}" width="100" height="42" rx="6" fill="#ffffff" stroke="{ORANGE}" stroke-width="3"/>')
text(300, CY + 79, "GPIO4 · ADC", 12, "bold", anchor="middle")
text(300, CY + 95, t("(mide tensión)"), 10.5, fill="#555", anchor="middle")
a(f'<line x1="306" y1="{CY}" x2="640" y2="{CY}" stroke="{BROWN}" stroke-width="11"/>')
a(f'<line x1="306" y1="{CY}" x2="640" y2="{CY}" stroke="{ORANGE}" stroke-width="2.5" stroke-dasharray="7 5"/>')
text(473, CY - 16, t("cable a lo largo del cinto"), 12, "bold", BROWN, "middle")
a(f'<rect x="640" y="{CY-28}" width="170" height="56" rx="8" fill="#fff8e1" stroke="{GOLD}" stroke-width="2.5"/>')
a(f'<line x1="640" y1="{CY}" x2="685" y2="{CY}" stroke="{ORANGE}" stroke-width="3"/>')
resistor(685, CY, 80, "")
text(725, CY + 4, "10 kΩ", 11, "bold", anchor="middle")
text(725, CY + 46, t("en el plug"), 11, fill="#555", anchor="middle")
a(f'<line x1="765" y1="{CY}" x2="870" y2="{CY}" stroke="{BLK}" stroke-width="3"/>')
a(f'<line x1="870" y1="{CY-14}" x2="870" y2="{CY+14}" stroke="{BLK}" stroke-width="4"/>')
a(f'<line x1="878" y1="{CY-9}" x2="878" y2="{CY+9}" stroke="{BLK}" stroke-width="3"/>')
a(f'<line x1="885" y1="{CY-4}" x2="885" y2="{CY+4}" stroke="{BLK}" stroke-width="2"/>')
text(905, CY + 5, "GND", 13, "bold")

# tabla de estados
TX, TY = 40, 712
text(TX, TY - 18, t("Lo que mide el GPIO4"), 12.5, "bold")
rows = [(GREEN, t("Enchufado"), "≈ 1,65 V", t("cerrado")),
        (RED, t("Desenchufado o cinto cortado"), "≈ 3,3 V", t("abierto → violación")),
        (RED, t("Punta puenteada a masa con un cable"), "≈ 0 V", t("falta la resistencia → violación"))]
x = TX
for col, what, volts, meaning in rows:
    w = 330
    a(f'<rect x="{x}" y="{TY-8}" width="{w}" height="46" rx="6" fill="#ffffff" stroke="{col}" stroke-width="2"/>')
    text(x + 10, TY + 10, what, 11.5, "bold")
    text(x + 10, TY + 28, num(volts) + " · " + meaning, 11.5, fill=col)
    x += w + 15
text(W // 2, H - 10, t("No detecta un puente sobre un tramo del cable que no incluya el plug: para eso está el sensor de luz adentro."),
     11, fill="#555", anchor="middle")
a("</svg>")
open(OUT, "w", encoding="utf-8").write("\n".join(o))
print("ok", OUT)
