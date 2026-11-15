#!/usr/bin/env python3
"""Genera docs/box.{es,en}.svg: cómo va montada la PharmaBox v2 sobre una caja de telgopor (corte lateral).

    python3 tools/gen_box.py es docs/box.es.svg
    python3 tools/gen_box.py en docs/box.en.svg
"""
import sys

LANG, OUT = sys.argv[1], sys.argv[2]
EN = {
    "PharmaBox v2: montaje sobre la caja (corte lateral)": "PharmaBox v2: mounting on the box (side cut)",
    "Módulo": "Module",
    "ESP32-S3, pantalla, batería,": "ESP32-S3, display, battery,",
    "acelerómetro, botón, buzzer": "accelerometer, button, buzzer",
    "Sensor de presión (FSR)": "Pressure sensor (FSR)",
    "entre el módulo y la tapa": "between module and lid",
    "Cinto con cable adentro (lazo)": "Strap with a wire inside (loop)",
    "rodea la caja por encima de la tapa": "goes around the box, over the lid",
    "Hebilla con resistencia de 10 kΩ": "Buckle with a 10 kΩ resistor",
    "abrirla o cortar el cinto abre el lazo": "opening it or cutting the strap opens the loop",
    "Sonda interna": "Internal probe",
    "temperatura, humedad y luz": "temperature, humidity and light",
    "Cable plano por debajo de la tapa": "Flat cable under the lid",
    "Tapa": "Lid",
    "Caja de telgopor": "Styrofoam box",
    "Medicamentos + refrigerantes": "Medicines + ice packs",
    "La luz adentro delata la tapa abierta o un corte en la pared.": "Light inside reveals an open lid or a cut in the wall.",
    "Es evidencia, no una cerradura: lo que pase queda en una lectura firmada.": "It is evidence, not a lock: whatever happens ends up in a signed reading.",
}
t = lambda s: s if LANG == "es" else EN[s]
o = []
a = o.append
W, H = 1000, 560
a(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="Helvetica, Arial, sans-serif">')
a(f'<rect width="{W}" height="{H}" fill="#ffffff"/>')


def text(x, y, s, size=12, weight="normal", fill="#1a1a1a", anchor="start"):
    a(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{s}</text>')


def label(x, y, title, sub, tx, ty, col="#333", anchor="start"):
    a(f'<line x1="{x}" y1="{y}" x2="{tx}" y2="{ty}" stroke="{col}" stroke-width="1.5"/>')
    a(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{col}"/>')
    lx = tx + (6 if anchor == "start" else -6)
    text(lx, ty + 1, title, 12.5, "bold", anchor=anchor)
    text(lx, ty + 17, sub, 11, fill="#555", anchor=anchor)


text(W // 2, 34, t("PharmaBox v2: montaje sobre la caja (corte lateral)"), 19, "bold", anchor="middle")
BX0, BX1, BY0, BY1, LID = 300, 700, 190, 470, 240  # caja; LID = borde inferior de la tapa
WALL = 34
# cinto (por detrás de todo): rodea caja y tapa
a(f'<rect x="{BX0-12}" y="{BY0-12}" width="{BX1-BX0+24}" height="{BY1-BY0+24}" rx="10" fill="none" stroke="#8d5524" stroke-width="9"/>')
# caja y tapa
a(f'<rect x="{BX0}" y="{LID}" width="{BX1-BX0}" height="{BY1-LID}" fill="#f1f3f5" stroke="#868e96" stroke-width="2"/>')
a(f'<rect x="{BX0+WALL}" y="{LID}" width="{BX1-BX0-2*WALL}" height="{BY1-LID-WALL}" fill="#ffffff" stroke="#adb5bd" stroke-width="1.5"/>')
a(f'<rect x="{BX0}" y="{BY0}" width="{BX1-BX0}" height="{LID-BY0-3}" fill="#e9ecef" stroke="#868e96" stroke-width="2"/>')
text(BX0 + 16, BY0 + 30, t("Tapa"), 12, "bold", "#495057")
text(BX0 + 8, BY1 - 10, t("Caja de telgopor"), 11, "bold", "#495057")
# contenido
a(f'<rect x="{BX0+WALL+30}" y="{BY1-WALL-90}" width="250" height="80" rx="6" fill="#dbeafe" stroke="#1d6fd8" stroke-width="1.5"/>')
text(BX0 + WALL + 155, BY1 - WALL - 45, t("Medicamentos + refrigerantes"), 11.5, fill="#1d4e89", anchor="middle")
# módulo sobre la tapa (tapa el cinto en ese tramo)
MX0, MX1, MY0 = 420, 580, 132
a(f'<rect x="{MX0+10}" y="{BY0-20}" width="{MX1-MX0-20}" height="8" fill="#c2185b"/>')  # FSR
a(f'<rect x="{MX0}" y="{MY0}" width="{MX1-MX0}" height="{BY0-20-MY0}" rx="8" fill="#1f2937" stroke="#0b1220" stroke-width="2"/>')
a(f'<rect x="{MX0+16}" y="{MY0+8}" width="{MX1-MX0-70}" height="{BY0-20-MY0-16}" rx="3" fill="#7dd3fc"/>')  # pantalla
a(f'<circle cx="{MX1-26}" cy="{MY0+19}" r="9" fill="#9aa4b2"/>')  # botón
# cinto por encima del módulo
a(f'<path d="M{MX0-30} {BY0-12} L{MX0-6} {MY0-8} L{MX1+6} {MY0-8} L{MX1+30} {BY0-12}" fill="none" stroke="#8d5524" stroke-width="9" stroke-linejoin="round"/>')
# hebilla
a(f'<rect x="{BX1+2}" y="330" width="22" height="46" rx="4" fill="#ad8b00" stroke="#6b5600" stroke-width="2"/>')
# sonda: cable desde el módulo, por debajo de la tapa, hasta adentro
a(f'<path d="M{MX0+4} {BY0-24} L{BX0-3} {BY0-24} L{BX0-3} {LID-2} L{BX0+WALL+40} {LID-2} L{BX0+WALL+40} {LID+50}" fill="none" stroke="#1d6fd8" stroke-width="3" stroke-linejoin="round"/>')
a(f'<rect x="{BX0+WALL+26}" y="{LID+50}" width="28" height="36" rx="4" fill="#1d6fd8"/>')

label(MX1 - 40, MY0 + 6, t("Módulo"), t("ESP32-S3, pantalla, batería,"), 640, 80)
text(646, 114, t("acelerómetro, botón, buzzer"), 11, fill="#555")
label(MX0 + 40, BY0 - 16, t("Sensor de presión (FSR)"), t("entre el módulo y la tapa"), 250, 96, "#c2185b", "end")
label(BX1 + 12, 260, t("Cinto con cable adentro (lazo)"), t("rodea la caja por encima de la tapa"), 760, 230, "#8d5524")
label(BX1 + 24, 353, t("Hebilla con resistencia de 10 kΩ"), t("abrirla o cortar el cinto abre el lazo"), 760, 346, "#ad8b00")
label(BX0 + WALL + 40, LID + 70, t("Sonda interna"), t("temperatura, humedad y luz"), 250, 330, "#1d6fd8", "end")
label(BX0 - 3, BY0 + 20, t("Cable plano por debajo de la tapa"), "", 250, 200, "#1d6fd8", "end")
text(W // 2, 520, t("La luz adentro delata la tapa abierta o un corte en la pared."), 12, fill="#333", anchor="middle")
text(W // 2, 540, t("Es evidencia, no una cerradura: lo que pase queda en una lectura firmada."), 12, fill="#333", anchor="middle")
a("</svg>")
open(OUT, "w", encoding="utf-8").write("\n".join(o))
print("ok", OUT)
