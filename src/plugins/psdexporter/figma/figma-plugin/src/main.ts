// PSD to Figma Importer - Plugin Main (sandbox)
// Reads Figma REST API-compatible JSON and creates Figma nodes.

interface FigmaColor {
  r: number;
  g: number;
  b: number;
  a: number;
}

interface FigmaFill {
  type: string;
  color?: FigmaColor;
  opacity?: number;
  visible?: boolean;
  scaleMode?: string;
  imageRef?: string;
  gradientHandlePositions?: { x: number; y: number }[];
  gradientStops?: { position: number; color: FigmaColor }[];
}

interface FigmaEffect {
  type: string;
  visible?: boolean;
  color?: FigmaColor;
  offset?: { x: number; y: number };
  radius?: number;
  spread?: number;
}

interface FigmaStyledTextSegment {
  characters: string;
  fontFamily: string;
  fontSize: number;
  fontWeight: number;
  italic?: boolean;
  letterSpacing?: number;
  lineHeightPx?: number;
  fills?: FigmaFill[];
  textDecoration?: string;
}

interface FigmaNode {
  type: string;
  name: string;
  id: string;
  visible?: boolean;
  opacity?: number;
  blendMode?: string;
  absoluteBoundingBox?: { x: number; y: number; width: number; height: number };
  fills?: FigmaFill[];
  strokes?: FigmaFill[];
  strokeWeight?: number;
  strokeAlign?: string;
  cornerRadius?: number;
  rectangleCornerRadii?: number[];
  clipsContent?: boolean;
  children?: FigmaNode[];
  characters?: string;
  style?: Record<string, unknown>;
  styledTextSegments?: FigmaStyledTextSegment[];
  fillGeometry?: { path: string; windingRule: string }[];
  effects?: FigmaEffect[];
}

interface FigmaDocument {
  document: {
    type: string;
    children: {
      type: string;
      name: string;
      children: FigmaNode[];
    }[];
  };
  images?: Record<string, string>;
}

// ─── Color Conversion ─────────────────────────────────────────────────────

function figmaColorToRGB(c: FigmaColor): RGB {
  return { r: c.r, g: c.g, b: c.b };
}

function figmaColorToRGBA(c: FigmaColor): RGBA {
  return { r: c.r, g: c.g, b: c.b, a: c.a };
}

// ─── Paint Conversion ─────────────────────────────────────────────────────

function fillToPaint(fill: FigmaFill): Paint | null {
  if (fill.visible === false) return null;

  if (fill.type === "SOLID" && fill.color) {
    const paint: SolidPaint = {
      type: "SOLID",
      color: figmaColorToRGB(fill.color),
      opacity: fill.opacity ?? fill.color.a ?? 1,
      visible: fill.visible !== false,
    };
    return paint;
  }

  if (
    fill.type.startsWith("GRADIENT_") &&
    fill.gradientHandlePositions &&
    fill.gradientStops
  ) {
    const stops: ColorStop[] = fill.gradientStops.map((s) => ({
      position: s.position,
      color: figmaColorToRGBA(s.color),
    }));

    const handles = fill.gradientHandlePositions;

    if (fill.type === "GRADIENT_LINEAR") {
      const paint: GradientPaint = {
        type: "GRADIENT_LINEAR",
        gradientStops: stops,
        gradientTransform: computeGradientTransform(handles),
        opacity: fill.opacity ?? 1,
        visible: fill.visible !== false,
      };
      return paint;
    }

    if (fill.type === "GRADIENT_RADIAL") {
      const paint: GradientPaint = {
        type: "GRADIENT_RADIAL",
        gradientStops: stops,
        gradientTransform: computeGradientTransform(handles),
        opacity: fill.opacity ?? 1,
        visible: fill.visible !== false,
      };
      return paint;
    }

    if (fill.type === "GRADIENT_ANGULAR") {
      const paint: GradientPaint = {
        type: "GRADIENT_ANGULAR",
        gradientStops: stops,
        gradientTransform: computeGradientTransform(handles),
        opacity: fill.opacity ?? 1,
        visible: fill.visible !== false,
      };
      return paint;
    }

    if (fill.type === "GRADIENT_DIAMOND") {
      const paint: GradientPaint = {
        type: "GRADIENT_DIAMOND",
        gradientStops: stops,
        gradientTransform: computeGradientTransform(handles),
        opacity: fill.opacity ?? 1,
        visible: fill.visible !== false,
      };
      return paint;
    }
  }

  return null;
}

function computeGradientTransform(
  handles: { x: number; y: number }[]
): Transform {
  // Figma gradient handles are in [0,1] space relative to node bounds
  // Handle 0 = start, Handle 1 = end, Handle 2 = width vector
  if (handles.length < 2) {
    return [
      [1, 0, 0],
      [0, 1, 0],
    ];
  }

  const h0 = handles[0];
  const h1 = handles[1];
  const h2 = handles.length >= 3 ? handles[2] : { x: h0.x - (h1.y - h0.y), y: h0.y + (h1.x - h0.x) };

  // Build affine matrix from the three handle points
  // The gradient coordinate system maps (0,0)->h0, (1,0)->h1, (0,1)->h2
  const dx1 = h1.x - h0.x;
  const dy1 = h1.y - h0.y;
  const dx2 = h2.x - h0.x;
  const dy2 = h2.y - h0.y;

  return [
    [dx1, dx2, h0.x],
    [dy1, dy2, h0.y],
  ];
}

function fillsToPaints(fills: FigmaFill[]): Paint[] {
  const paints: Paint[] = [];
  for (const fill of fills) {
    const paint = fillToPaint(fill);
    if (paint) paints.push(paint);
  }
  return paints;
}

// ─── Effect Conversion ────────────────────────────────────────────────────

function convertEffects(effects: FigmaEffect[]): Effect[] {
  const result: Effect[] = [];
  for (const eff of effects) {
    if (eff.visible === false) continue;

    if (eff.type === "DROP_SHADOW" && eff.color) {
      result.push({
        type: "DROP_SHADOW",
        color: figmaColorToRGBA(eff.color),
        offset: { x: eff.offset?.x ?? 0, y: eff.offset?.y ?? 0 },
        radius: eff.radius ?? 0,
        spread: eff.spread ?? 0,
        visible: true,
        blendMode: "NORMAL",
      } as DropShadowEffect);
    } else if (eff.type === "INNER_SHADOW" && eff.color) {
      result.push({
        type: "INNER_SHADOW",
        color: figmaColorToRGBA(eff.color),
        offset: { x: eff.offset?.x ?? 0, y: eff.offset?.y ?? 0 },
        radius: eff.radius ?? 0,
        spread: eff.spread ?? 0,
        visible: true,
        blendMode: "NORMAL",
      } as InnerShadowEffect);
    } else if (eff.type === "LAYER_BLUR") {
      result.push({
        type: "LAYER_BLUR",
        radius: eff.radius ?? 0,
        visible: true,
      } as BlurEffect);
    }
  }
  return result;
}

// ─── Set Common Properties ────────────────────────────────────────────────

function setCommonProps(
  node: SceneNode,
  data: FigmaNode,
  parentX: number,
  parentY: number
) {
  // Convert absolute coordinates to parent-relative
  if (data.absoluteBoundingBox) {
    const box = data.absoluteBoundingBox;
    node.x = box.x - parentX;
    node.y = box.y - parentY;
    node.resize(Math.max(1, box.width), Math.max(1, box.height));
  }

  if (data.opacity !== undefined) {
    node.opacity = data.opacity;
  }

  if (data.visible === false) {
    node.visible = false;
  }
}

// ─── Image Handling ───────────────────────────────────────────────────────

function base64ToUint8Array(base64: string): Uint8Array {
  // Decode base64 manually for compatibility with all Figma versions
  const binaryString = atob(base64);
  const bytes = new Uint8Array(binaryString.length);
  for (let i = 0; i < binaryString.length; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes;
}

async function createImagePaint(
  base64Data: string
): Promise<ImagePaint | null> {
  try {
    let bytes: Uint8Array;
    if (typeof figma.base64Decode === "function") {
      bytes = figma.base64Decode(base64Data);
    } else {
      bytes = base64ToUint8Array(base64Data);
    }
    const image = figma.createImage(bytes);
    return {
      type: "IMAGE",
      scaleMode: "FILL",
      imageHash: image.hash,
    };
  } catch (e) {
    console.error("Failed to create image:", e);
    return null;
  }
}

// ─── Node Creation ────────────────────────────────────────────────────────

async function createNode(
  data: FigmaNode,
  parentX: number,
  parentY: number,
  images: Record<string, string>
): Promise<SceneNode | null> {
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

    // Fallback: create a frame for unknown types
    console.warn(`Unknown node type: ${nodeType}, creating frame`);
    return await createFrameNode(data, parentX, parentY, images);
  } catch (e) {
    console.error(`Error creating ${nodeType} node "${data.name}":`, e);
    return null;
  }
}

async function createFrameNode(
  data: FigmaNode,
  parentX: number,
  parentY: number,
  images: Record<string, string>
): Promise<FrameNode> {
  const frame = figma.createFrame();
  setCommonProps(frame, data, parentX, parentY);
  frame.name = data.name;

  if (data.fills) {
    frame.fills = fillsToPaints(data.fills);
  }

  if (data.clipsContent !== undefined) {
    frame.clipsContent = data.clipsContent;
  }

  if (data.cornerRadius !== undefined) {
    frame.cornerRadius = data.cornerRadius;
  }

  if (data.effects) {
    frame.effects = convertEffects(data.effects);
  }

  // Create children
  const box = data.absoluteBoundingBox ?? { x: 0, y: 0, width: 0, height: 0 };
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

async function createGroupNode(
  data: FigmaNode,
  parentX: number,
  parentY: number,
  images: Record<string, string>
): Promise<SceneNode> {
  // Compute the effective origin for children's coordinate conversion.
  // If the GROUP has a valid (non-zero) bounding box, use it.
  // Otherwise, use parentX/parentY so children keep their absolute positions.
  const box = data.absoluteBoundingBox;
  const hasValidBox = box && (box.width > 0 || box.height > 0);
  const childParentX = hasValidBox ? box.x : parentX;
  const childParentY = hasValidBox ? box.y : parentY;

  const children: SceneNode[] = [];

  if (data.children) {
    for (const childData of data.children) {
      const child = await createNode(childData, childParentX, childParentY, images);
      if (child) children.push(child);
    }
  }

  if (children.length === 0) {
    // Fallback to frame if no children
    const frame = figma.createFrame();
    setCommonProps(frame, data, parentX, parentY);
    frame.name = data.name;
    frame.fills = [];
    return frame;
  }

  // Add children to current page temporarily, then group
  for (const child of children) {
    figma.currentPage.appendChild(child);
  }
  const group = figma.group(children, figma.currentPage);
  group.name = data.name;

  // Only reposition the group if we have a valid bounding box.
  // When bounding box is {0,0,0,0}, let the group stay at its
  // natural position (auto-derived from children's bounding box).
  if (hasValidBox) {
    group.x = box.x - parentX;
    group.y = box.y - parentY;
  }

  if (data.opacity !== undefined) {
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

async function createRectangleNode(
  data: FigmaNode,
  parentX: number,
  parentY: number,
  images: Record<string, string>
): Promise<RectangleNode> {
  const rect = figma.createRectangle();
  setCommonProps(rect, data, parentX, parentY);
  rect.name = data.name;

  if (data.cornerRadius !== undefined) {
    rect.cornerRadius = data.cornerRadius;
  }

  if (data.rectangleCornerRadii && data.rectangleCornerRadii.length === 4) {
    rect.topLeftRadius = data.rectangleCornerRadii[0];
    rect.topRightRadius = data.rectangleCornerRadii[1];
    rect.bottomRightRadius = data.rectangleCornerRadii[2];
    rect.bottomLeftRadius = data.rectangleCornerRadii[3];
  }

  // Handle image fills
  let hasImageFill = false;
  if (data.fills) {
    const paints: Paint[] = [];
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
  if (data.strokeWeight !== undefined) {
    rect.strokeWeight = data.strokeWeight;
  }
  if (data.strokeAlign) {
    rect.strokeAlign = data.strokeAlign as "CENTER" | "INSIDE" | "OUTSIDE";
  }

  if (data.effects) {
    rect.effects = convertEffects(data.effects);
  }

  return rect;
}

async function createVectorNode(
  data: FigmaNode,
  parentX: number,
  parentY: number
): Promise<VectorNode> {
  const vector = figma.createVector();
  setCommonProps(vector, data, parentX, parentY);
  vector.name = data.name;

  // Set vector paths from fillGeometry
  if (data.fillGeometry && data.fillGeometry.length > 0) {
    const paths: VectorPath[] = data.fillGeometry.map((geom) => ({
      data: geom.path,
      windingRule: (geom.windingRule as "NONZERO" | "EVENODD") ?? "NONZERO",
    }));
    vector.vectorPaths = paths;
  }

  if (data.fills) {
    vector.fills = fillsToPaints(data.fills);
  }

  if (data.strokes) {
    vector.strokes = fillsToPaints(data.strokes);
  }
  if (data.strokeWeight !== undefined) {
    vector.strokeWeight = data.strokeWeight;
  }
  if (data.strokeAlign) {
    vector.strokeAlign = data.strokeAlign as "CENTER" | "INSIDE" | "OUTSIDE";
  }

  if (data.effects) {
    vector.effects = convertEffects(data.effects);
  }

  return vector;
}

async function createTextNode(
  data: FigmaNode,
  parentX: number,
  parentY: number
): Promise<TextNode> {
  const text = figma.createText();
  setCommonProps(text, data, parentX, parentY);
  text.name = data.name;

  // Load fonts and set styled text segments
  const segments = data.styledTextSegments ?? [];
  const characters = data.characters ?? "";

  // Always load Inter Regular first as a safe default/fallback.
  // Figma requires a font to be loaded before setting characters.
  try {
    await figma.loadFontAsync({ family: "Inter", style: "Regular" });
  } catch {
    console.error("Failed to load Inter Regular");
  }

  // Collect unique fonts and attempt to load them
  const fontsToLoad = new Set<string>();
  const loadedFonts = new Set<string>();
  loadedFonts.add("Inter::Regular"); // Already loaded above

  if (segments.length > 0) {
    for (const seg of segments) {
      // Try extracting weight/style from font family name (e.g., "SourceHanSans-Bold")
      const style = seg.italic ? "Italic" : "Regular";
      fontsToLoad.add(`${seg.fontFamily}::${style}`);
      // Also try with Bold style if fontWeight >= 700
      if (seg.fontWeight >= 700) {
        fontsToLoad.add(`${seg.fontFamily}::Bold`);
      }
    }
  } else if (data.style) {
    const fontFamily = (data.style.fontFamily as string) ?? "Inter";
    const italic = data.style.italic as boolean;
    const style = italic ? "Italic" : "Regular";
    fontsToLoad.add(`${fontFamily}::${style}`);
  }

  for (const fontKey of fontsToLoad) {
    const [family, style] = fontKey.split("::");
    try {
      await figma.loadFontAsync({ family, style });
      loadedFonts.add(fontKey);
    } catch {
      // Try common style name variations
      const variations = ["Regular", "Bold", "Medium", "Light"];
      let loaded = false;
      for (const variant of variations) {
        try {
          await figma.loadFontAsync({ family, style: variant });
          loadedFonts.add(`${family}::${variant}`);
          loaded = true;
          break;
        } catch {
          // continue trying
        }
      }
      if (!loaded) {
        console.warn(`Font not available: ${family}, using Inter Regular`);
      }
    }
  }

  // Set characters (Inter Regular is loaded, so this should always work)
  text.characters = characters;

  // Apply styled text segments
  if (segments.length > 0) {
    let offset = 0;
    for (const seg of segments) {
      const segLen = seg.characters.length;
      if (segLen === 0) continue;
      const start = offset;
      const end = offset + segLen;

      const fontFamily = seg.fontFamily ?? "Inter";
      const fontStyle = seg.italic ? "Italic" : "Regular";

      try {
        text.setRangeFontName(start, end, {
          family: fontFamily,
          style: fontStyle,
        });
      } catch {
        text.setRangeFontName(start, end, {
          family: "Inter",
          style: "Regular",
        });
      }

      if (seg.fontSize) {
        text.setRangeFontSize(start, end, seg.fontSize);
      }

      if (seg.letterSpacing !== undefined) {
        text.setRangeLetterSpacing(start, end, {
          value: seg.letterSpacing,
          unit: "PIXELS",
        });
      }

      if (seg.lineHeightPx !== undefined) {
        text.setRangeLineHeight(start, end, {
          value: seg.lineHeightPx,
          unit: "PIXELS",
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

  // Set text auto-resize
  if (data.style) {
    const autoResize = data.style.textAutoResize as string;
    if (autoResize === "NONE") {
      text.textAutoResize = "NONE";
    } else if (autoResize === "HEIGHT") {
      text.textAutoResize = "HEIGHT";
    } else {
      text.textAutoResize = "WIDTH_AND_HEIGHT";
    }

    // Text alignment
    const hAlign = data.style.textAlignHorizontal as string;
    if (hAlign === "CENTER") text.textAlignHorizontal = "CENTER";
    else if (hAlign === "RIGHT") text.textAlignHorizontal = "RIGHT";
    else if (hAlign === "JUSTIFIED") text.textAlignHorizontal = "JUSTIFIED";
    else text.textAlignHorizontal = "LEFT";

    const vAlign = data.style.textAlignVertical as string;
    if (vAlign === "CENTER") text.textAlignVertical = "CENTER";
    else if (vAlign === "BOTTOM") text.textAlignVertical = "BOTTOM";
    else text.textAlignVertical = "TOP";
  }

  if (data.effects) {
    text.effects = convertEffects(data.effects);
  }

  return text;
}

function createEllipseNode(
  data: FigmaNode,
  parentX: number,
  parentY: number
): EllipseNode {
  const ellipse = figma.createEllipse();
  setCommonProps(ellipse, data, parentX, parentY);
  ellipse.name = data.name;

  if (data.fills) {
    ellipse.fills = fillsToPaints(data.fills);
  }

  if (data.strokes) {
    ellipse.strokes = fillsToPaints(data.strokes);
  }
  if (data.strokeWeight !== undefined) {
    ellipse.strokeWeight = data.strokeWeight;
  }

  if (data.effects) {
    ellipse.effects = convertEffects(data.effects);
  }

  return ellipse;
}

// ─── Main Import Logic ────────────────────────────────────────────────────

async function importFigmaJson(jsonStr: string) {
  console.log(`[PSD→Figma] Starting import, JSON size: ${jsonStr.length} chars`);

  let data: FigmaDocument;
  try {
    data = JSON.parse(jsonStr);
  } catch (e) {
    figma.notify("Invalid JSON format");
    console.error("[PSD→Figma] JSON parse failed:", e);
    return;
  }

  if (!data.document) {
    figma.notify("Missing 'document' key in JSON");
    return;
  }

  const images = data.images ?? {};
  const imageCount = Object.keys(images).length;
  console.log(`[PSD→Figma] Document parsed. Images: ${imageCount}`);

  const pages = data.document.children;
  if (!pages || pages.length === 0) {
    figma.notify("No pages found in document");
    return;
  }

  // Process first page
  const page = pages[0];
  const pageChildren = page.children ?? [];
  console.log(`[PSD→Figma] Page "${page.name}": ${pageChildren.length} top-level nodes`);

  figma.notify(`Importing ${pageChildren.length} nodes...`);

  let importedCount = 0;
  let errorCount = 0;
  for (let i = 0; i < pageChildren.length; i++) {
    const nodeData = pageChildren[i];
    console.log(`[PSD→Figma] Creating node ${i + 1}/${pageChildren.length}: ${nodeData.type} "${nodeData.name}"`);
    try {
      const node = await createNode(nodeData, 0, 0, images);
      if (node) {
        figma.currentPage.appendChild(node);
        importedCount++;
        console.log(`[PSD→Figma]   → OK (${node.type})`);
      } else {
        console.warn(`[PSD→Figma]   → returned null`);
      }
    } catch (e) {
      errorCount++;
      console.error(`[PSD→Figma]   → ERROR:`, e);
    }
  }

  if (figma.currentPage.children.length > 0) {
    figma.viewport.scrollAndZoomIntoView(figma.currentPage.children);
  }

  const msg = `Imported ${importedCount} node(s)` + (errorCount > 0 ? `, ${errorCount} error(s)` : "");
  console.log(`[PSD→Figma] ${msg}`);
  figma.notify(msg);
}

// ─── Plugin Message Handler ───────────────────────────────────────────────

figma.showUI(__html__, { width: 480, height: 400 });

figma.ui.onmessage = async (msg: { type: string; data?: string }) => {
  if (msg.type === "import" && msg.data) {
    await importFigmaJson(msg.data);
    figma.closePlugin();
  } else if (msg.type === "cancel") {
    figma.closePlugin();
  }
};
