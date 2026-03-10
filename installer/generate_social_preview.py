"""Generate a GitHub social preview image (1280x640) for TEGE."""

from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os
import math

# --- Configuration ---
WIDTH, HEIGHT = 1280, 640
BG_COLOR = (13, 17, 23)          # #0d1117 (GitHub dark)
ACCENT_COLOR = (88, 166, 255)    # Soft blue accent
TEXT_COLOR = (230, 237, 243)     # #e6edf3 (GitHub light text)
SUBTEXT_COLOR = (139, 148, 158)  # #8b949e (GitHub muted text)
TAGLINE_COLOR = (88, 166, 255)   # Blue for the tagline

ICON_PATH = os.path.join(os.path.dirname(__file__), "TEGE_ICON_256.png")
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "social_preview.png")


def load_font(size, bold=False):
    """Try to load a nice system font, fall back to default."""
    if bold:
        candidates = [
            "C:/Windows/Fonts/segoeuib.ttf",
            "C:/Windows/Fonts/arialbd.ttf",
            "C:/Windows/Fonts/calibrib.ttf",
        ]
    else:
        candidates = [
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/calibri.ttf",
        ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except (OSError, IOError):
            continue
    return ImageFont.load_default()


# --- Create base image ---
img = Image.new("RGB", (WIDTH, HEIGHT), BG_COLOR)
draw = ImageDraw.Draw(img)

# --- Radial vignette using fast alpha compositing ---
# Create a radial gradient mask (white center, black edges)
vignette = Image.new("L", (WIDTH, HEIGHT), 0)
# Draw a large white ellipse in the center, then blur it
vignette_draw = ImageDraw.Draw(vignette)
# Ellipse covering ~70% of the image
margin_x = int(WIDTH * 0.05)
margin_y = int(HEIGHT * 0.05)
vignette_draw.ellipse(
    [margin_x, margin_y, WIDTH - margin_x, HEIGHT - margin_y],
    fill=255
)
# Heavy blur for smooth falloff
vignette = vignette.filter(ImageFilter.GaussianBlur(radius=150))

# Create a slightly brighter version of the background for the center
bright_bg = Image.new("RGB", (WIDTH, HEIGHT), (18, 24, 33))
# Composite: where vignette is white -> bright_bg, where black -> dark bg
img = Image.composite(bright_bg, img, vignette)
draw = ImageDraw.Draw(img)

# --- Subtle accent lines at top and bottom ---
line_layer = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
line_draw = ImageDraw.Draw(line_layer)
for x in range(WIDTH):
    fade = 1.0 - abs(x - WIDTH / 2) / (WIDTH / 2)
    alpha = int(50 * fade * fade)  # Quadratic falloff
    color = (*ACCENT_COLOR, alpha)
    line_draw.point((x, 1), fill=color)
    line_draw.point((x, 2), fill=color)
    line_draw.point((x, HEIGHT - 2), fill=color)
    line_draw.point((x, HEIGHT - 3), fill=color)
img = Image.alpha_composite(img.convert("RGBA"), line_layer).convert("RGB")
draw = ImageDraw.Draw(img)

# --- Load and place the icon ---
icon = Image.open(ICON_PATH).convert("RGBA")
icon_size = 180
icon = icon.resize((icon_size, icon_size), Image.LANCZOS)

# --- Load fonts ---
font_title = load_font(80, bold=True)
font_subtitle = load_font(32, bold=False)
font_tagline = load_font(24, bold=False)

# --- Measure text to compute total content height ---
# Use font.getbbox() for accurate metrics, then use fixed line heights
# to avoid textbbox quirks with y-offsets
title_text = "TEGE"
title_w = draw.textlength(title_text, font=font_title)
title_line_h = 80  # Match font size

subtitle_text = "The Enjin Game Engine"
sub_w = draw.textlength(subtitle_text, font=font_subtitle)
sub_line_h = 36

tagline_text = "C++20  \u2022  Vulkan  \u2022  BSL 1.1"
tag_w = draw.textlength(tagline_text, font=font_tagline)
tag_line_h = 28

# Gaps between elements
gap_icon_title = 30
gap_title_sub = 16
gap_sub_tag = 22

total_h = icon_size + gap_icon_title + title_line_h + gap_title_sub + sub_line_h + gap_sub_tag + tag_line_h
start_y = (HEIGHT - total_h) // 2

# --- Place icon ---
icon_x = (WIDTH - icon_size) // 2
icon_y = start_y
# Paste with alpha
img_rgba = img.convert("RGBA")
img_rgba.paste(icon, (icon_x, icon_y), icon)
img = img_rgba.convert("RGB")
draw = ImageDraw.Draw(img)

# --- Draw title "TEGE" ---
title_x = int((WIDTH - title_w) // 2)
title_y = icon_y + icon_size + gap_icon_title
draw.text((title_x, title_y), title_text, fill=TEXT_COLOR, font=font_title)

# --- Draw subtitle ---
sub_x = int((WIDTH - sub_w) // 2)
sub_y = title_y + title_line_h + gap_title_sub
draw.text((sub_x, sub_y), subtitle_text, fill=SUBTEXT_COLOR, font=font_subtitle)

# --- Draw tagline ---
tag_x = int((WIDTH - tag_w) // 2)
tag_y = sub_y + sub_line_h + gap_sub_tag
draw.text((tag_x, tag_y), tagline_text, fill=TAGLINE_COLOR, font=font_tagline)

# --- Save ---
img.save(OUTPUT_PATH, "PNG")
print(f"Social preview saved to: {OUTPUT_PATH}")
print(f"Image size: {img.size[0]}x{img.size[1]}")
