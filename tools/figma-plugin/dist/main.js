"use strict";
(() => {
  // src/main.ts
  function figmaColorToRGB(c) {
    return { r: c.r, g: c.g, b: c.b };
  }
  function figmaColorToRGBA(c) {
    return { r: c.r, g: c.g, b: c.b, a: c.a };
  }
  function fillToPaint(fill) {
    var _a, _b, _c, _d, _e, _f;
    if (fill.visible === false) return null;
    if (fill.type === "SOLID" && fill.color) {
      const paint = {
        type: "SOLID",
        color: figmaColorToRGB(fill.color),
        opacity: (_b = (_a = fill.opacity) != null ? _a : fill.color.a) != null ? _b : 1,
        visible: fill.visible !== false
      };
      return paint;
    }
    if (fill.type.startsWith("GRADIENT_") && fill.gradientHandlePositions && fill.gradientStops) {
      const stops = fill.gradientStops.map((s) => ({
        position: s.position,
        color: figmaColorToRGBA(s.color)
      }));
      const handles = fill.gradientHandlePositions;
      if (fill.type === "GRADIENT_LINEAR") {
        const paint = {
          type: "GRADIENT_LINEAR",
          gradientStops: stops,
          gradientTransform: computeGradientTransform(handles),
          opacity: (_c = fill.opacity) != null ? _c : 1,
          visible: fill.visible !== false
        };
        return paint;
      }
      if (fill.type === "GRADIENT_RADIAL") {
        const paint = {
          type: "GRADIENT_RADIAL",
          gradientStops: stops,
          gradientTransform: computeGradientTransform(handles),
          opacity: (_d = fill.opacity) != null ? _d : 1,
          visible: fill.visible !== false
        };
        return paint;
      }
      if (fill.type === "GRADIENT_ANGULAR") {
        const paint = {
          type: "GRADIENT_ANGULAR",
          gradientStops: stops,
          gradientTransform: computeGradientTransform(handles),
          opacity: (_e = fill.opacity) != null ? _e : 1,
          visible: fill.visible !== false
        };
        return paint;
      }
      if (fill.type === "GRADIENT_DIAMOND") {
        const paint = {
          type: "GRADIENT_DIAMOND",
          gradientStops: stops,
          gradientTransform: computeGradientTransform(handles),
          opacity: (_f = fill.opacity) != null ? _f : 1,
          visible: fill.visible !== false
        };
        return paint;
      }
    }
    return null;
  }
  function computeGradientTransform(handles) {
    if (handles.length < 2) {
      return [
        [1, 0, 0],
        [0, 1, 0]
      ];
    }
    const h0 = handles[0];
    const h1 = handles[1];
    const h2 = handles.length >= 3 ? handles[2] : { x: h0.x - (h1.y - h0.y), y: h0.y + (h1.x - h0.x) };
    const dx1 = h1.x - h0.x;
    const dy1 = h1.y - h0.y;
    const dx2 = h2.x - h0.x;
    const dy2 = h2.y - h0.y;
    return [
      [dx1, dx2, h0.x],
      [dy1, dy2, h0.y]
    ];
  }
  function fillsToPaints(fills) {
    const paints = [];
    for (const fill of fills) {
      const paint = fillToPaint(fill);
      if (paint) paints.push(paint);
    }
    return paints;
  }
  function convertEffects(effects) {
    var _a, _b, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m;
    const result = [];
    for (const eff of effects) {
      if (eff.visible === false) continue;
      if (eff.type === "DROP_SHADOW" && eff.color) {
        result.push({
          type: "DROP_SHADOW",
          color: figmaColorToRGBA(eff.color),
          offset: { x: (_b = (_a = eff.offset) == null ? void 0 : _a.x) != null ? _b : 0, y: (_d = (_c = eff.offset) == null ? void 0 : _c.y) != null ? _d : 0 },
          radius: (_e = eff.radius) != null ? _e : 0,
          spread: (_f = eff.spread) != null ? _f : 0,
          visible: true,
          blendMode: "NORMAL"
        });
      } else if (eff.type === "INNER_SHADOW" && eff.color) {
        result.push({
          type: "INNER_SHADOW",
          color: figmaColorToRGBA(eff.color),
          offset: { x: (_h = (_g = eff.offset) == null ? void 0 : _g.x) != null ? _h : 0, y: (_j = (_i = eff.offset) == null ? void 0 : _i.y) != null ? _j : 0 },
          radius: (_k = eff.radius) != null ? _k : 0,
          spread: (_l = eff.spread) != null ? _l : 0,
          visible: true,
          blendMode: "NORMAL"
        });
      } else if (eff.type === "LAYER_BLUR") {
        result.push({
          type: "LAYER_BLUR",
          radius: (_m = eff.radius) != null ? _m : 0,
          visible: true
        });
      }
    }
    return result;
  }
  function setCommonProps(node, data, parentX, parentY) {
    if (data.absoluteBoundingBox) {
      const box = data.absoluteBoundingBox;
      node.x = box.x - parentX;
      node.y = box.y - parentY;
      node.resize(Math.max(1, box.width), Math.max(1, box.height));
    }
    if (data.opacity !== void 0) {
      node.opacity = data.opacity;
    }
    if (data.visible === false) {
      node.visible = false;
    }
  }
  function base64ToUint8Array(base64) {
    const binaryString = atob(base64);
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return bytes;
  }
  async function createImagePaint(base64Data) {
    try {
      let bytes;
      if (typeof figma.base64Decode === "function") {
        bytes = figma.base64Decode(base64Data);
      } else {
        bytes = base64ToUint8Array(base64Data);
      }
      const image = figma.createImage(bytes);
      return {
        type: "IMAGE",
        scaleMode: "FILL",
        imageHash: image.hash
      };
    } catch (e) {
      console.error("Failed to create image:", e);
      return null;
    }
  }
  async function createNode(data, parentX, parentY, images) {
    const nodeType = data.type;
    try {
      if (nodeType === "FRAME" || nodeType === "COMPONENT") {
        return await createFrameNode(data, parentX, parentY, images);
      }
      if (nodeType === "GROUP") {
        return await createGroupNode(data, parentX, parentY, images);
      }
      if (nodeType === "RECTANGLE") {
        return await createRectangleNode(data, parentX, parentY, images);
      }
      if (nodeType === "VECTOR") {
        return await createVectorNode(data, parentX, parentY);
      }
      if (nodeType === "TEXT") {
        return await createTextNode(data, parentX, parentY);
      }
      if (nodeType === "ELLIPSE") {
        return createEllipseNode(data, parentX, parentY);
      }
      console.warn(`Unknown node type: ${nodeType}, creating frame`);
      return await createFrameNode(data, parentX, parentY, images);
    } catch (e) {
      console.error(`Error creating ${nodeType} node "${data.name}":`, e);
      return null;
    }
  }
  async function createFrameNode(data, parentX, parentY, images) {
    var _a;
    const frame = figma.createFrame();
    setCommonProps(frame, data, parentX, parentY);
    frame.name = data.name;
    if (data.fills) {
      frame.fills = fillsToPaints(data.fills);
    }
    if (data.clipsContent !== void 0) {
      frame.clipsContent = data.clipsContent;
    }
    if (data.cornerRadius !== void 0) {
      frame.cornerRadius = data.cornerRadius;
    }
    if (data.effects) {
      frame.effects = convertEffects(data.effects);
    }
    const box = (_a = data.absoluteBoundingBox) != null ? _a : { x: 0, y: 0, width: 0, height: 0 };
    if (data.children) {
      for (const childData of data.children) {
        const child = await createNode(childData, box.x, box.y, images);
        if (child) {
          frame.appendChild(child);
        }
      }
    }
    return frame;
  }
  async function createGroupNode(data, parentX, parentY, images) {
    const box = data.absoluteBoundingBox;
    const hasValidBox = box && (box.width > 0 || box.height > 0);
    const childParentX = hasValidBox ? box.x : parentX;
    const childParentY = hasValidBox ? box.y : parentY;
    const children = [];
    if (data.children) {
      for (const childData of data.children) {
        const child = await createNode(childData, childParentX, childParentY, images);
        if (child) children.push(child);
      }
    }
    if (children.length === 0) {
      const frame = figma.createFrame();
      setCommonProps(frame, data, parentX, parentY);
      frame.name = data.name;
      frame.fills = [];
      return frame;
    }
    for (const child of children) {
      figma.currentPage.appendChild(child);
    }
    const group = figma.group(children, figma.currentPage);
    group.name = data.name;
    if (hasValidBox) {
      group.x = box.x - parentX;
      group.y = box.y - parentY;
    }
    if (data.opacity !== void 0) {
      group.opacity = data.opacity;
    }
    if (data.visible === false) {
      group.visible = false;
    }
    if (data.effects) {
      group.effects = convertEffects(data.effects);
    }
    return group;
  }
  async function createRectangleNode(data, parentX, parentY, images) {
    const rect = figma.createRectangle();
    setCommonProps(rect, data, parentX, parentY);
    rect.name = data.name;
    if (data.cornerRadius !== void 0) {
      rect.cornerRadius = data.cornerRadius;
    }
    if (data.rectangleCornerRadii && data.rectangleCornerRadii.length === 4) {
      rect.topLeftRadius = data.rectangleCornerRadii[0];
      rect.topRightRadius = data.rectangleCornerRadii[1];
      rect.bottomRightRadius = data.rectangleCornerRadii[2];
      rect.bottomLeftRadius = data.rectangleCornerRadii[3];
    }
    let hasImageFill = false;
    if (data.fills) {
      const paints = [];
      for (const fill of data.fills) {
        if (fill.type === "IMAGE" && fill.imageRef && images[fill.imageRef]) {
          const imgPaint = await createImagePaint(images[fill.imageRef]);
          if (imgPaint) {
            paints.push(imgPaint);
            hasImageFill = true;
            continue;
          }
        }
        const paint = fillToPaint(fill);
        if (paint) paints.push(paint);
      }
      rect.fills = paints;
    }
    if (data.strokes) {
      rect.strokes = fillsToPaints(data.strokes);
    }
    if (data.strokeWeight !== void 0) {
      rect.strokeWeight = data.strokeWeight;
    }
    if (data.strokeAlign) {
      rect.strokeAlign = data.strokeAlign;
    }
    if (data.effects) {
      rect.effects = convertEffects(data.effects);
    }
    return rect;
  }
  async function createVectorNode(data, parentX, parentY) {
    const vector = figma.createVector();
    setCommonProps(vector, data, parentX, parentY);
    vector.name = data.name;
    if (data.fillGeometry && data.fillGeometry.length > 0) {
      const paths = data.fillGeometry.map((geom) => {
        var _a;
        return {
          data: geom.path,
          windingRule: (_a = geom.windingRule) != null ? _a : "NONZERO"
        };
      });
      vector.vectorPaths = paths;
    }
    if (data.fills) {
      vector.fills = fillsToPaints(data.fills);
    }
    if (data.strokes) {
      vector.strokes = fillsToPaints(data.strokes);
    }
    if (data.strokeWeight !== void 0) {
      vector.strokeWeight = data.strokeWeight;
    }
    if (data.strokeAlign) {
      vector.strokeAlign = data.strokeAlign;
    }
    if (data.effects) {
      vector.effects = convertEffects(data.effects);
    }
    return vector;
  }
  async function createTextNode(data, parentX, parentY) {
    var _a, _b, _c, _d;
    const text = figma.createText();
    setCommonProps(text, data, parentX, parentY);
    text.name = data.name;
    const segments = (_a = data.styledTextSegments) != null ? _a : [];
    const characters = (_b = data.characters) != null ? _b : "";
    try {
      await figma.loadFontAsync({ family: "Inter", style: "Regular" });
    } catch (e) {
      console.error("Failed to load Inter Regular");
    }
    const fontsToLoad = /* @__PURE__ */ new Set();
    const loadedFonts = /* @__PURE__ */ new Set();
    loadedFonts.add("Inter::Regular");
    if (segments.length > 0) {
      for (const seg of segments) {
        const style = seg.italic ? "Italic" : "Regular";
        fontsToLoad.add(`${seg.fontFamily}::${style}`);
        if (seg.fontWeight >= 700) {
          fontsToLoad.add(`${seg.fontFamily}::Bold`);
        }
      }
    } else if (data.style) {
      const fontFamily = (_c = data.style.fontFamily) != null ? _c : "Inter";
      const italic = data.style.italic;
      const style = italic ? "Italic" : "Regular";
      fontsToLoad.add(`${fontFamily}::${style}`);
    }
    for (const fontKey of fontsToLoad) {
      const [family, style] = fontKey.split("::");
      try {
        await figma.loadFontAsync({ family, style });
        loadedFonts.add(fontKey);
      } catch (e) {
        const variations = ["Regular", "Bold", "Medium", "Light"];
        let loaded = false;
        for (const variant of variations) {
          try {
            await figma.loadFontAsync({ family, style: variant });
            loadedFonts.add(`${family}::${variant}`);
            loaded = true;
            break;
          } catch (e2) {
          }
        }
        if (!loaded) {
          console.warn(`Font not available: ${family}, using Inter Regular`);
        }
      }
    }
    text.characters = characters;
    if (segments.length > 0) {
      let offset = 0;
      for (const seg of segments) {
        const segLen = seg.characters.length;
        if (segLen === 0) continue;
        const start = offset;
        const end = offset + segLen;
        const fontFamily = (_d = seg.fontFamily) != null ? _d : "Inter";
        const fontStyle = seg.italic ? "Italic" : "Regular";
        try {
          text.setRangeFontName(start, end, {
            family: fontFamily,
            style: fontStyle
          });
        } catch (e) {
          text.setRangeFontName(start, end, {
            family: "Inter",
            style: "Regular"
          });
        }
        if (seg.fontSize) {
          text.setRangeFontSize(start, end, seg.fontSize);
        }
        if (seg.letterSpacing !== void 0) {
          text.setRangeLetterSpacing(start, end, {
            value: seg.letterSpacing,
            unit: "PIXELS"
          });
        }
        if (seg.lineHeightPx !== void 0) {
          text.setRangeLineHeight(start, end, {
            value: seg.lineHeightPx,
            unit: "PIXELS"
          });
        }
        if (seg.fills) {
          const paints = fillsToPaints(seg.fills);
          if (paints.length > 0) {
            text.setRangeFills(start, end, paints);
          }
        }
        if (seg.textDecoration === "UNDERLINE") {
          text.setRangeTextDecoration(start, end, "UNDERLINE");
        } else if (seg.textDecoration === "STRIKETHROUGH") {
          text.setRangeTextDecoration(start, end, "STRIKETHROUGH");
        }
        offset = end;
      }
    }
    if (data.style) {
      const autoResize = data.style.textAutoResize;
      if (autoResize === "NONE") {
        text.textAutoResize = "NONE";
      } else if (autoResize === "HEIGHT") {
        text.textAutoResize = "HEIGHT";
      } else {
        text.textAutoResize = "WIDTH_AND_HEIGHT";
      }
      const hAlign = data.style.textAlignHorizontal;
      if (hAlign === "CENTER") text.textAlignHorizontal = "CENTER";
      else if (hAlign === "RIGHT") text.textAlignHorizontal = "RIGHT";
      else if (hAlign === "JUSTIFIED") text.textAlignHorizontal = "JUSTIFIED";
      else text.textAlignHorizontal = "LEFT";
      const vAlign = data.style.textAlignVertical;
      if (vAlign === "CENTER") text.textAlignVertical = "CENTER";
      else if (vAlign === "BOTTOM") text.textAlignVertical = "BOTTOM";
      else text.textAlignVertical = "TOP";
    }
    if (data.effects) {
      text.effects = convertEffects(data.effects);
    }
    return text;
  }
  function createEllipseNode(data, parentX, parentY) {
    const ellipse = figma.createEllipse();
    setCommonProps(ellipse, data, parentX, parentY);
    ellipse.name = data.name;
    if (data.fills) {
      ellipse.fills = fillsToPaints(data.fills);
    }
    if (data.strokes) {
      ellipse.strokes = fillsToPaints(data.strokes);
    }
    if (data.strokeWeight !== void 0) {
      ellipse.strokeWeight = data.strokeWeight;
    }
    if (data.effects) {
      ellipse.effects = convertEffects(data.effects);
    }
    return ellipse;
  }
  async function importFigmaJson(jsonStr) {
    var _a, _b;
    console.log(`[PSD\u2192Figma] Starting import, JSON size: ${jsonStr.length} chars`);
    let data;
    try {
      data = JSON.parse(jsonStr);
    } catch (e) {
      figma.notify("Invalid JSON format");
      console.error("[PSD\u2192Figma] JSON parse failed:", e);
      return;
    }
    if (!data.document) {
      figma.notify("Missing 'document' key in JSON");
      return;
    }
    const images = (_a = data.images) != null ? _a : {};
    const imageCount = Object.keys(images).length;
    console.log(`[PSD\u2192Figma] Document parsed. Images: ${imageCount}`);
    const pages = data.document.children;
    if (!pages || pages.length === 0) {
      figma.notify("No pages found in document");
      return;
    }
    const page = pages[0];
    const pageChildren = (_b = page.children) != null ? _b : [];
    console.log(`[PSD\u2192Figma] Page "${page.name}": ${pageChildren.length} top-level nodes`);
    figma.notify(`Importing ${pageChildren.length} nodes...`);
    let importedCount = 0;
    let errorCount = 0;
    for (let i = 0; i < pageChildren.length; i++) {
      const nodeData = pageChildren[i];
      console.log(`[PSD\u2192Figma] Creating node ${i + 1}/${pageChildren.length}: ${nodeData.type} "${nodeData.name}"`);
      try {
        const node = await createNode(nodeData, 0, 0, images);
        if (node) {
          figma.currentPage.appendChild(node);
          importedCount++;
          console.log(`[PSD\u2192Figma]   \u2192 OK (${node.type})`);
        } else {
          console.warn(`[PSD\u2192Figma]   \u2192 returned null`);
        }
      } catch (e) {
        errorCount++;
        console.error(`[PSD\u2192Figma]   \u2192 ERROR:`, e);
      }
    }
    if (figma.currentPage.children.length > 0) {
      figma.viewport.scrollAndZoomIntoView(figma.currentPage.children);
    }
    const msg = `Imported ${importedCount} node(s)` + (errorCount > 0 ? `, ${errorCount} error(s)` : "");
    console.log(`[PSD\u2192Figma] ${msg}`);
    figma.notify(msg);
  }
  figma.showUI(__html__, { width: 480, height: 400 });
  figma.ui.onmessage = async (msg) => {
    if (msg.type === "import" && msg.data) {
      await importFigmaJson(msg.data);
      figma.closePlugin();
    } else if (msg.type === "cancel") {
      figma.closePlugin();
    }
  };
})();
