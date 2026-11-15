#!/usr/bin/env python3
"""Genera docs/wiring.{es,en}.svg: ESP32-S3-DevKitC-1 y los módulos de la PharmaBox v2.

    python3 tools/gen_wiring.py es docs/wiring.es.svg
    python3 tools/gen_wiring.py en docs/wiring.en.svg

Los pines salen de include/config.h: si cambian ahí, cambiarlos acá. J1 (izquierda) y J3 (derecha)
están en el orden físico de la guía de usuario de Espressif del DevKitC-1.
"""
import sys

LANG, OUT = sys.argv[1], sys.argv[2]
EN = {
    "PharmaBox v2: cableado del módulo (ESP32-S3-DevKitC-1)": "PharmaBox v2: module wiring (ESP32-S3-DevKitC-1)",
    "J1 izquierda · J3 derecha": "J1 left · J3 right",
    "USB (carga del firmware y debug)": "USB (flashing and debug)",
    "golpes y movimiento · SA0 → GND": "shocks and motion · SA0 → GND",
    "Sonda interna (cable de 4 hilos)": "Internal probe (4-wire cable)",
    "SHT4x 0x44 + VEML7700 0x10 · adentro de la caja": "SHT4x 0x44 + VEML7700 0x10 · inside the box",
    "Precinto: lazo del cinto": "Seal: strap loop",
    "cable a lo largo del cinto, 10 kΩ en la hebilla": "wire along the strap, 10 kΩ in the buckle",
    "3V3 por 10 kΩ": "3V3 via 10 kΩ",
    "LAZO": "LOOP",
    "GND (hebilla)": "GND (buckle)",
    "Precinto: presión (FSR)": "Seal: pressure (FSR)",
    "entre el módulo y la tapa · 10 kΩ a GND": "between module and lid · 10 kΩ to GND",
    "SEÑAL": "SIGNAL",
    "LiPo + cargador/protección": "LiPo + charger/protection",
    "5 V al DevKit · divisor 100k/100k para medir": "5 V to the DevKit · 100k/100k divider to measure",
    "5V OUT": "5V OUT",
    "VBAT/2": "VBAT/2",
    "Pantalla TFT ILI9488 (SPI, sin táctil)": "ILI9488 TFT display (SPI, no touch)",
    "320x480 · QR de apertura y estado": "320x480 · unlock QR and status",
    "← 3V3 (pin 3V3 de J1)": "← 3V3 (3V3 pin on J1)",
    "← GND (pin G de J3)": "← GND (G pin on J3)",
    "(retroiluminación)": "(backlight)",
    "Botón": "Button",
    "enciende la pantalla y genera el QR": "wakes the display and creates the QR",
    "→ GPIO2 (INPUT_PULLUP)": "→ GPIO2 (INPUT_PULLUP)",
    "→ GND": "→ GND",
    "Buzzer activo": "Active buzzer",
    "precinto violado · celular fuera de alcance": "seal breached · phone out of range",
    "← GPIO47 (transistor si > 20 mA)": "← GPIO47 (transistor if > 20 mA)",
    "Notas": "Notes",
    "• Las tres entradas analógicas van en ADC1": "• The three analog inputs are on ADC1",
    "  (GPIO1-10): ADC2 no sirve con BLE encendido.": "  (GPIO1-10): ADC2 is unusable with BLE on.",
    "• SDO/MISO de la pantalla queda sin conectar.": "• The display's SDO/MISO stays unconnected.",
    "• Lazo cerrado ≈ 1,65 V. Abierto ≈ 3,3 V.": "• Loop closed ≈ 1.65 V. Open ≈ 3.3 V.",
    "  Puenteado sin la resistencia ≈ 0 V.": "  Bridged without the resistor ≈ 0 V.",
    "• La sonda entra por debajo de la tapa.": "• The probe goes in under the lid.",
    "Alimentación": "Power",
    "Usado por el proyecto": "Used by the project",
    "Strapping (no forzar al arrancar)": "Strapping (don't drive at boot)",
    "PSRAM octal (N8R8): no usar": "Octal PSRAM (N8R8): don't use",
    "Serial/USB": "Serial/USB",
    "INT1 del LIS3DH": "LIS3DH INT1",
    "lazo del precinto": "seal loop",
    "presión": "pressure",
    "batería": "battery",
    "pantalla (SPI)": "display (SPI)",
    "botón": "button",
    "Orden físico según la guía de Espressif del DevKitC-1 (J1/J3, USB abajo). Propuesta sin validar en hardware: verificar contra la serigrafía de tu placa.":
        "Physical order per Espressif's DevKitC-1 guide (J1/J3, USB at the bottom). Proposal, not validated on hardware: check against your board's silkscreen.",
}


def t(s):
    return s if LANG == "es" else EN.get(s, s)


W, H = 1200, 960
PITCH, Y0 = 30, 130
BX0, BX1, BY0, BY1 = 520, 720, 60, 800
LX0, LX1 = 392, 512
RX0, RX1 = 732, 852

RED, BLK, BLUE, GREEN = "#d62828", "#111111", "#1d6fd8", "#2a9d3f"
ORANGE, BROWN, MAGENTA = "#e07b00", "#8d5524", "#c2185b"
PURPLE, TEAL, GOLD, GRAY = "#6a1b9a", "#00897b", "#ad8b00", "#555555"
VBAT, CYAN = "#b45f06", "#0277bd"

J1 = [
    ("3V3", "used:" + RED), ("3V3", "pwr"), ("RST", "ctrl"), ("GPIO4 · LOOP", "used:" + BROWN),
    ("GPIO5 · INT1", "used:" + ORANGE), ("GPIO6 · FSR", "used:" + MAGENTA), ("GPIO7 · BAT", "used:" + VBAT),
    ("GPIO15", "gpio"), ("GPIO16", "gpio"), ("GPIO17", "gpio"), ("GPIO18", "gpio"),
    ("GPIO8 · SDA", "used:" + BLUE), ("GPIO3 · boot", "boot"), ("GPIO46 · boot", "boot"),
    ("GPIO9 · SCL", "used:" + GREEN), ("GPIO10", "gpio"), ("GPIO11", "gpio"), ("GPIO12", "gpio"),
    ("GPIO13", "gpio"), ("GPIO14", "gpio"), ("5V", "used:" + VBAT), ("GND", "used:" + BLK),
]
J3 = [
    ("GND", "gnd"), ("GPIO43 · TX0", "uart"), ("GPIO44 · RX0", "uart"), ("GPIO1", "gpio"),
    ("GPIO2 · BUTTON", "used:" + GRAY), ("GPIO42 · TFT CS", "used:" + CYAN), ("GPIO41 · TFT DC", "used:" + CYAN),
    ("GPIO40 · TFT RST", "used:" + CYAN), ("GPIO39 · MOSI", "used:" + CYAN), ("GPIO38 · SCK", "used:" + CYAN),
    ("GPIO37 · PSRAM", "nope"), ("GPIO36 · PSRAM", "nope"), ("GPIO35 · PSRAM", "nope"),
    ("GPIO0 · boot", "boot"), ("GPIO45 · boot", "boot"), ("GPIO48 · LED", "gpio"),
    ("GPIO47 · BUZZER", "used:" + GOLD), ("GPIO21 · TFT BL", "used:" + CYAN), ("GPIO20 · USB", "uart"),
    ("GPIO19 · USB", "uart"), ("GND", "gnd"), ("GND", "gnd"),
]
STYLE = {
    "pwr": ("#fde2e2", RED), "gnd": ("#e4e4e4", BLK), "boot": ("#fff6bf", "#b59b00"),
    "uart": ("#eeeeee", "#777777"), "gpio": ("#f7f7f7", "#999999"), "ctrl": ("#eeeeee", "#777777"),
    "nope": ("#e9d5ff", "#7e3fbf"),
}

o = []
a = o.append


def py(i):
    return Y0 + i * PITCH


def text(x, y, s, size=11, weight="normal", fill="#1a1a1a", anchor="start"):
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    a(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{s}</text>')


def wire(color, pts, dots=(), width=2.5):
    d = "M" + " L".join(f"{x} {y}" for x, y in pts)
    a(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"/>')
    for x, y in dots:
        a(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{color}"/>')


a(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="Helvetica, Arial, sans-serif">')
a(f'<rect width="{W}" height="{H}" fill="#ffffff"/>')
text(W // 2, 30, t("PharmaBox v2: cableado del módulo (ESP32-S3-DevKitC-1)"), 19, "bold", anchor="middle")
text(LX0 + 60, 52, "J1", 13, "bold", "#555", "middle")
text(RX0 + 60, 52, "J3", 13, "bold", "#555", "middle")

# placa
a(f'<rect x="{BX0}" y="{BY0}" width="{BX1-BX0}" height="{BY1-BY0}" rx="12" fill="#1f2937" stroke="#0b1220" stroke-width="2"/>')
a(f'<rect x="{BX0+35}" y="{BY0+12}" width="130" height="150" rx="4" fill="#c9ced6" stroke="#8a919c" stroke-width="1.5"/>')
text((BX0 + BX1) // 2, BY0 + 90, "ESP32-S3-WROOM-1", 12.5, "bold", anchor="middle")
text((BX0 + BX1) // 2, BY0 + 108, "WiFi + BLE 5", 10, fill="#444", anchor="middle")
text((BX0 + BX1) // 2, 380, "ESP32-S3-DevKitC-1", 11.5, fill="#9aa4b2", anchor="middle")
text((BX0 + BX1) // 2, 396, t("J1 izquierda · J3 derecha"), 10, fill="#9aa4b2", anchor="middle")
a(f'<rect x="{(BX0+BX1)//2-28}" y="{BY1-2}" width="56" height="32" rx="4" fill="#9aa4b2" stroke="#6b7280" stroke-width="2"/>')
text((BX0 + BX1) // 2, BY1 + 48, t("USB (carga del firmware y debug)"), 11, fill="#555", anchor="middle")


def chip(i, label, cat, side):
    y = py(i)
    if cat.startswith("used:"):
        col = cat.split(":")[1]
        fill, stroke, sw, wt = "#ffffff", col, 3, "bold"
    else:
        fill, stroke = STYLE[cat]
        sw, wt = 1.5, "normal"
    x0, x1 = (LX0, LX1) if side == "L" else (RX0, RX1)
    a(f'<line x1="{x1 if side == "L" else BX1}" y1="{y}" x2="{BX0 if side == "L" else x0}" y2="{y}" stroke="#999" stroke-width="2"/>')
    a(f'<rect x="{x0}" y="{y-12}" width="{x1-x0}" height="24" rx="5" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')
    text((x0 + x1) // 2, y + 4, label, 11, wt, anchor="middle")
    pc = stroke if cat.startswith("used:") else "#d4af37"
    a(f'<circle cx="{BX0 if side == "L" else BX1}" cy="{y}" r="5" fill="{pc}" stroke="#0b1220" stroke-width="1"/>')


for i, (l, c) in enumerate(J1):
    chip(i, l, c, "L")
for i, (l, c) in enumerate(J3):
    chip(i, l, c, "R")

# ---------------------------------------------------------------- izquierda: sensores y precinto
TX0, TX1 = 30, 250
RAIL = {"3V3": (276, RED, py(0)), "GND": (288, BLK, py(21)), "SDA": (300, BLUE, py(11)), "SCL": (312, GREEN, py(14))}
LANE = {"INT1": (326, ORANGE, py(4)), "LOOP": (338, BROWN, py(3)), "FSR": (350, MAGENTA, py(5)),
        "BAT": (362, VBAT, py(6)), "5V": (376, VBAT, py(20))}
taps = {k: [] for k in RAIL}
sig = {}


def ltile(y, title, sub, pins, fill="#f4f4f4", stroke="#555"):
    """pins: (etiqueta, red). red es una clave de RAIL o LANE."""
    h = 44 + 14 * len(pins) + 6
    a(f'<rect x="{TX0}" y="{y}" width="{TX1-TX0}" height="{h}" rx="8" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
    text(TX0 + 10, y + 17, title, 12, "bold")
    text(TX0 + 10, y + 31, sub, 9.3, fill="#555")
    for k, (nm, net) in enumerate(pins):
        yy = y + 48 + 14 * k
        text(TX1 - 8, yy + 3.5, nm, 10, anchor="end")
        a(f'<circle cx="{TX1}" cy="{yy}" r="3.2" fill="#1a1a1a"/>')
        if net in RAIL:
            taps[net].append(yy)
        else:
            sig[net] = yy
    return y + h + 12


y = 62
y = ltile(y, "LIS3DH · 0x18", t("golpes y movimiento · SA0 → GND"),
          [("VCC", "3V3"), ("GND", "GND"), ("SDA", "SDA"), ("SCL", "SCL"), ("INT1", "INT1")])
y = ltile(y, t("Sonda interna (cable de 4 hilos)"), t("SHT4x 0x44 + VEML7700 0x10 · adentro de la caja"),
          [("3V3", "3V3"), ("GND", "GND"), ("SDA", "SDA"), ("SCL", "SCL")], "#eaf4ff", BLUE)
y = ltile(y, t("Precinto: lazo del cinto"), t("cable a lo largo del cinto, 10 kΩ en la hebilla"),
          [(t("3V3 por 10 kΩ"), "3V3"), (t("LAZO"), "LOOP"), (t("GND (hebilla)"), "GND")], "#fff3e6", BROWN)
y = ltile(y, t("Precinto: presión (FSR)"), t("entre el módulo y la tapa · 10 kΩ a GND"),
          [("3V3", "3V3"), (t("SEÑAL"), "FSR"), ("GND", "GND")], "#fde8f1", MAGENTA)
y = ltile(y, t("LiPo + cargador/protección"), t("5 V al DevKit · divisor 100k/100k para medir"),
          [(t("5V OUT"), "5V"), (t("VBAT/2"), "BAT"), ("GND", "GND")], "#fff4e5", VBAT)

for net, (x, col, cy) in RAIL.items():
    ys = taps[net] + [cy]
    wire(col, [(x, min(ys)), (x, max(ys))])
    wire(col, [(LX0, cy), (x, cy)], dots=[(x, cy)])
    for ty_ in taps[net]:
        wire(col, [(TX1, ty_), (x, ty_)], dots=[(x, ty_)], width=2)
for net, (x, col, cy) in LANE.items():
    wire(col, [(TX1, sig[net]), (x, sig[net]), (x, cy), (LX0, cy)])

# ---------------------------------------------------------------- derecha: pantalla, botón, buzzer
MX0, MX1 = 900, 1180


def rtile(y, h, title, sub, pins, fill="#f4f4f4", stroke="#555"):
    a(f'<rect x="{MX0}" y="{y}" width="{MX1-MX0}" height="{h}" rx="8" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
    text(MX0 + 10, y + 17, title, 12, "bold")
    text(MX0 + 10, y + 31, sub, 9.5, fill="#555")
    out = {}
    for k, (nm, note) in enumerate(pins):
        yy = y + 48 + 15 * k
        out[nm] = yy
        a(f'<circle cx="{MX0}" cy="{yy}" r="3.2" fill="#1a1a1a"/>')
        text(MX0 + 8, yy + 3.5, nm, 10, "bold")
        if note:
            text(MX0 + 86, yy + 3.5, note, 9.5, fill="#555")
    return out


btn = rtile(70, 84, t("Botón"), t("enciende la pantalla y genera el QR"),
            [("1", t("→ GPIO2 (INPUT_PULLUP)")), ("2", t("→ GND"))])
wire(GRAY, [(RX1, py(4)), (876, py(4)), (876, btn["1"]), (MX0, btn["1"])])

tft = rtile(196, 176, t("Pantalla TFT ILI9488 (SPI, sin táctil)"), t("320x480 · QR de apertura y estado"),
            [("VCC", t("← 3V3 (pin 3V3 de J1)")), ("GND", t("← GND (pin G de J3)")), ("CS", "← GPIO42"),
             ("RESET", "← GPIO40"), ("DC/RS", "← GPIO41"), ("SDI (MOSI)", "← GPIO39"), ("SCK", "← GPIO38"),
             ("LED", "← GPIO21 " + t("(retroiluminación)"))], "#e6f4fb", CYAN)
for lane_x, pin_i, name in ((860, 5, "CS"), (866, 6, "DC/RS"), (872, 7, "RESET"), (878, 8, "SDI (MOSI)"),
                            (884, 9, "SCK")):
    wire(CYAN, [(RX1, py(pin_i)), (lane_x, py(pin_i)), (lane_x, tft[name]), (MX0, tft[name])], width=2.2)
wire(CYAN, [(RX1, py(17)), (890, py(17)), (890, tft["LED"]), (MX0, tft["LED"])], width=2.2)

buz = rtile(560, 84, t("Buzzer activo"), t("precinto violado · celular fuera de alcance"),
            [("+", t("← GPIO47 (transistor si > 20 mA)")), ("−", t("→ GND"))])
wire(GOLD, [(RX1, py(16)), (870, py(16)), (870, buz["+"]), (MX0, buz["+"])])

text(MX0, 680, t("Notas"), 11, "bold")
for k, line in enumerate([
    "• Las tres entradas analógicas van en ADC1", "  (GPIO1-10): ADC2 no sirve con BLE encendido.",
    "• SDO/MISO de la pantalla queda sin conectar.", "• Lazo cerrado ≈ 1,65 V. Abierto ≈ 3,3 V.",
    "  Puenteado sin la resistencia ≈ 0 V.", "• La sonda entra por debajo de la tapa.",
]):
    text(MX0, 700 + 16 * k, t(line), 10.5, fill="#333")

# ---------------------------------------------------------------- leyenda
ly, lx = 880, 30
for fill, stroke, s in [("#fde2e2", RED, "Alimentación"), ("#e4e4e4", BLK, "GND"), ("#ffffff", BLUE, "Usado por el proyecto"),
                        ("#fff6bf", "#b59b00", "Strapping (no forzar al arrancar)"),
                        ("#e9d5ff", "#7e3fbf", "PSRAM octal (N8R8): no usar"), ("#eeeeee", "#777777", "Serial/USB")]:
    s = t(s)
    a(f'<rect x="{lx}" y="{ly-11}" width="22" height="14" rx="3" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
    text(lx + 30, ly, s, 11)
    lx += 30 + int(len(s) * 6.2) + 24
ly2, lx = 906, 30
for col, s in ((BLUE, "SDA"), (GREEN, "SCL"), (ORANGE, "INT1 del LIS3DH"), (BROWN, "lazo del precinto"),
               (MAGENTA, "presión"), (VBAT, "batería"), (CYAN, "pantalla (SPI)"), (GRAY, "botón"), (GOLD, "buzzer")):
    s = t(s)
    a(f'<rect x="{lx}" y="{ly2-8}" width="22" height="4" fill="{col}"/>')
    text(lx + 30, ly2, s, 11)
    lx += 30 + int(len(s) * 6.2) + 22
text(30, 934, t("Orden físico según la guía de Espressif del DevKitC-1 (J1/J3, USB abajo). Propuesta sin validar en hardware: "
                "verificar contra la serigrafía de tu placa."), 10.5, fill="#555")
a("</svg>")
open(OUT, "w", encoding="utf-8").write("\n".join(o))
print("ok", OUT)
