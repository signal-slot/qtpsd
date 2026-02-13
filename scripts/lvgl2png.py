#!/usr/bin/env python3
import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont


def clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, value))


def parse_int(text: str, default: int = 0) -> int:
    try:
        return int(float(text))
    except Exception:
        return default


def parse_color(text: str, alpha: int) -> tuple[int, int, int, int]:
    if not text:
        return (0, 0, 0, alpha)
    value = text.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    if len(value) != 6 or any(c not in "0123456789abcdef" for c in value):
        return (0, 0, 0, alpha)
    r = int(value[0:2], 16)
    g = int(value[2:4], 16)
    b = int(value[4:6], 16)
    return (r, g, b, alpha)


def parse_font_size(text: str, default_size: int = 14) -> int:
    if not text:
        return default_size
    m = re.search(r"(\d+)$", text.strip())
    if not m:
        return default_size
    return max(1, int(m.group(1)))


def load_font(size: int) -> ImageFont.ImageFont:
    candidates = []
    env_font = os.environ.get("LVGL_FONT_PATH", "").strip()
    if env_font:
        candidates.append(env_font)
    candidates.extend(
        [
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        ]
    )
    for path in candidates:
        if os.path.isfile(path):
            try:
                return ImageFont.truetype(path, size=size)
            except Exception:
                continue
    return ImageFont.load_default()


def alpha_composite_clipped(dst: Image.Image, src: Image.Image, x: int, y: int) -> None:
    dst_w, dst_h = dst.size
    src_w, src_h = src.size
    if src_w <= 0 or src_h <= 0:
        return

    left = max(0, x)
    top = max(0, y)
    right = min(dst_w, x + src_w)
    bottom = min(dst_h, y + src_h)
    if right <= left or bottom <= top:
        return

    cropped = src.crop((left - x, top - y, right - x, bottom - y))
    dst.alpha_composite(cropped, (left, top))


def wrap_text(text: str, draw: ImageDraw.ImageDraw, font: ImageFont.ImageFont, max_width: int) -> list[str]:
    if max_width <= 0:
        return [text]
    lines: list[str] = []
    for paragraph in text.splitlines() or [""]:
        words = paragraph.split(" ")
        if not words:
            lines.append("")
            continue
        line = words[0]
        for word in words[1:]:
            candidate = f"{line} {word}"
            bbox = draw.textbbox((0, 0), candidate, font=font)
            width = bbox[2] - bbox[0]
            if width <= max_width:
                line = candidate
            else:
                lines.append(line)
                line = word
        lines.append(line)
    return lines


class LvglRenderer:
    def __init__(self, main_xml_path: str) -> None:
        self.main_xml_path = main_xml_path
        self.base_dir = os.path.dirname(main_xml_path)
        self.image_map = self._load_globals()

        tree = ET.parse(main_xml_path)
        root = tree.getroot()
        self.view = None
        for child in list(root):
            self.view = child
            break
        if self.view is None:
            raise RuntimeError(f"No root view found: {main_xml_path}")

        width = max(1, parse_int(self.view.attrib.get("width", "1"), 1))
        height = max(1, parse_int(self.view.attrib.get("height", "1"), 1))
        self.canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))

    def _load_globals(self) -> dict[str, str]:
        path = os.path.join(self.base_dir, "globals.xml")
        mapping: dict[str, str] = {}
        if not os.path.isfile(path):
            return mapping

        try:
            tree = ET.parse(path)
            root = tree.getroot()
            for file_node in root.findall("./images/file"):
                name = (file_node.attrib.get("name") or "").strip()
                src = (file_node.attrib.get("src_path") or "").strip()
                if not name or not src:
                    continue
                if src.startswith("A:"):
                    src = src[2:]
                if os.path.isabs(src):
                    abs_path = src
                else:
                    abs_path = os.path.join(self.base_dir, src)
                mapping[name] = abs_path
        except Exception:
            return mapping
        return mapping

    def _resolve_image_path(self, name: str) -> str:
        if name in self.image_map:
            return self.image_map[name]
        fallback = os.path.join(self.base_dir, "images", f"{name}.png")
        return fallback

    def render(self) -> Image.Image:
        self._render_node(self.view, 0, 0, 255)
        return self.canvas

    def _render_node(self, node: ET.Element, parent_x: int, parent_y: int, parent_opa: int) -> None:
        attrs = node.attrib
        hidden = (attrs.get("hidden") or "").strip().lower()
        if hidden in ("true", "1", "yes"):
            return

        x = parent_x + parse_int(attrs.get("x", "0"), 0)
        y = parent_y + parse_int(attrs.get("y", "0"), 0)
        w = max(0, parse_int(attrs.get("width", "0"), 0))
        h = max(0, parse_int(attrs.get("height", "0"), 0))

        style_opa = clamp(parse_int(attrs.get("style_opa", "255"), 255), 0, 255)
        effective_opa = (parent_opa * style_opa) // 255
        tag = node.tag

        if tag in ("view", "lv_obj", "lv_button"):
            bg = attrs.get("style_bg_color", "")
            bg_opa = clamp(parse_int(attrs.get("style_bg_opa", "0"), 0), 0, 255)
            radius = max(0, parse_int(attrs.get("style_radius", "0"), 0))
            if bg and bg_opa > 0 and w > 0 and h > 0:
                alpha = (effective_opa * bg_opa) // 255
                fill = parse_color(bg, alpha)
                layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
                draw = ImageDraw.Draw(layer)
                if radius > 0:
                    draw.rounded_rectangle((0, 0, w - 1, h - 1), radius=radius, fill=fill)
                else:
                    draw.rectangle((0, 0, w - 1, h - 1), fill=fill)
                alpha_composite_clipped(self.canvas, layer, x, y)

            for child in list(node):
                self._render_node(child, x, y, effective_opa)
            return

        if tag == "lv_image":
            src = attrs.get("src", "").strip()
            if src and w > 0 and h > 0:
                image_path = self._resolve_image_path(src)
                if os.path.isfile(image_path):
                    try:
                        img = Image.open(image_path).convert("RGBA")
                        if img.size != (w, h):
                            img = img.resize((w, h), Image.Resampling.BILINEAR)
                        if effective_opa < 255:
                            alpha = img.getchannel("A")
                            alpha = alpha.point(lambda px: (px * effective_opa) // 255)
                            img.putalpha(alpha)
                        alpha_composite_clipped(self.canvas, img, x, y)
                    except Exception:
                        pass
            for child in list(node):
                self._render_node(child, x, y, effective_opa)
            return

        if tag == "lv_label":
            text = attrs.get("text", "")
            if text:
                font_size = parse_font_size(attrs.get("style_text_font", ""), 14)
                font = load_font(font_size)
                color = parse_color(attrs.get("style_text_color", "0x000000"), effective_opa)
                align = (attrs.get("style_text_align") or "left").strip().lower()
                rw = max(1, w if w > 0 else 1)
                rh = max(1, h if h > 0 else font_size + 4)
                label = Image.new("RGBA", (rw, rh), (0, 0, 0, 0))
                draw = ImageDraw.Draw(label)
                lines = wrap_text(text, draw, font, rw)
                line_height = max(1, int(font_size * 1.25))
                total_height = line_height * len(lines)
                cursor_y = max(0, (rh - total_height) // 2)
                for line in lines:
                    bbox = draw.textbbox((0, 0), line, font=font)
                    tw = bbox[2] - bbox[0]
                    if align == "center":
                        cursor_x = max(0, (rw - tw) // 2)
                    elif align == "right":
                        cursor_x = max(0, rw - tw)
                    else:
                        cursor_x = 0
                    draw.text((cursor_x, cursor_y), line, font=font, fill=color)
                    cursor_y += line_height
                alpha_composite_clipped(self.canvas, label, x, y)
            for child in list(node):
                self._render_node(child, x, y, effective_opa)
            return

        for child in list(node):
            self._render_node(child, x, y, effective_opa)


def main() -> int:
    parser = argparse.ArgumentParser(description="Render LVGL XML export to PNG")
    parser.add_argument("input_xml", help="Path to LVGL MainScreen.xml")
    parser.add_argument("output_png", help="Output PNG path")
    args = parser.parse_args()

    input_xml = os.path.abspath(args.input_xml)
    output_png = os.path.abspath(args.output_png)
    if not os.path.isfile(input_xml):
        print(f"Input XML does not exist: {input_xml}", file=sys.stderr)
        return 1

    try:
        renderer = LvglRenderer(input_xml)
        image = renderer.render()
    except Exception as exc:
        print(f"Failed to render LVGL XML: {exc}", file=sys.stderr)
        return 1

    os.makedirs(os.path.dirname(output_png), exist_ok=True)
    image.save(output_png, format="PNG")
    return 0


if __name__ == "__main__":
    sys.exit(main())
