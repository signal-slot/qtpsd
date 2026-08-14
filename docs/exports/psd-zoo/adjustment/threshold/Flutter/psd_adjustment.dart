// Applies a Photoshop adjustment layer to everything painted below it.
// The child subtree is rasterized into a texture and run through the
// psd_adjustment.frag fragment shader.

import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';

class PsdAdjustment extends StatefulWidget {
  const PsdAdjustment({
    super.key,
    required this.params,
    this.lutAsset,
    this.weightAsset,
    required this.child,
  });

  final Map<String, double> params;
  final String? lutAsset;
  final String? weightAsset;
  final Widget child;

  @override
  State<PsdAdjustment> createState() => _PsdAdjustmentState();
}

class _PsdAdjustmentState extends State<PsdAdjustment> {
  static ui.FragmentProgram? _program;
  static final Map<String, ui.Image> _imageCache = {};
  static ui.Image? _blank;

  ui.FragmentShader? _shader;
  ui.Image? _lut;
  ui.Image? _weight;

  // Float uniform order in shaders/psd_adjustment.frag, after uSize
  static const List<String> _uniformNames = [
    'uType',
    'brightness', 'contrast', 'brit_pivot', 'brit_modern',
    'lvl_shadowIn', 'lvl_highlightIn', 'lvl_shadowOut', 'lvl_highlightOut', 'lvl_midtone',
    'lvlR_shadowIn', 'lvlR_highlightIn', 'lvlR_shadowOut', 'lvlR_highlightOut', 'lvlR_midtone',
    'lvlG_shadowIn', 'lvlG_highlightIn', 'lvlG_shadowOut', 'lvlG_highlightOut', 'lvlG_midtone',
    'lvlB_shadowIn', 'lvlB_highlightIn', 'lvlB_shadowOut', 'lvlB_highlightOut', 'lvlB_midtone',
    'exposure', 'offset', 'gamma',
    'hueShift', 'saturationShift', 'lightnessShift',
    'bal_shadow_cr', 'bal_shadow_mg', 'bal_shadow_yb',
    'bal_mid_cr', 'bal_mid_mg', 'bal_mid_yb',
    'bal_hi_cr', 'bal_hi_mg', 'bal_hi_yb',
    'bal_preserveLum',
    'phfl_r', 'phfl_g', 'phfl_b', 'phfl_density', 'phfl_preserveLum',
    'post_levels', 'threshold',
    'vibrance', 'vibranceSat',
    'mixr_outR_r', 'mixr_outR_g', 'mixr_outR_b', 'mixr_outR_c',
    'mixr_outG_r', 'mixr_outG_g', 'mixr_outG_b', 'mixr_outG_c',
    'mixr_outB_r', 'mixr_outB_g', 'mixr_outB_b', 'mixr_outB_c',
    'mixr_mono',
    'bw_red', 'bw_yellow', 'bw_green', 'bw_cyan', 'bw_blue', 'bw_magenta',
    'adjWeight', 'useWeightMask',
  ];

  static const Map<String, double> _uniformDefaults = {
    'uType': -1.0,
    'brit_pivot': 0.5,
    'lvl_highlightIn': 1.0, 'lvl_highlightOut': 1.0, 'lvl_midtone': 1.0,
    'lvlR_highlightIn': 1.0, 'lvlR_highlightOut': 1.0, 'lvlR_midtone': 1.0,
    'lvlG_highlightIn': 1.0, 'lvlG_highlightOut': 1.0, 'lvlG_midtone': 1.0,
    'lvlB_highlightIn': 1.0, 'lvlB_highlightOut': 1.0, 'lvlB_midtone': 1.0,
    'gamma': 1.0,
    'post_levels': 4.0,
    'threshold': 0.5,
    'adjWeight': 1.0,
  };

  @override
  void initState() {
    super.initState();
    _load();
  }

  static ui.Image _blankImage() {
    if (_blank != null) {
      return _blank!;
    }
    final recorder = ui.PictureRecorder();
    Canvas(recorder).drawRect(
      const Rect.fromLTWH(0, 0, 1, 1),
      Paint()..color = const Color(0xFFFFFFFF),
    );
    _blank = recorder.endRecording().toImageSync(1, 1);
    return _blank!;
  }

  Future<ui.Image> _loadImage(String asset) async {
    final cached = _imageCache[asset];
    if (cached != null) {
      return cached;
    }
    final data = await rootBundle.load(asset);
    final codec = await ui.instantiateImageCodec(data.buffer.asUint8List());
    final frame = await codec.getNextFrame();
    _imageCache[asset] = frame.image;
    return frame.image;
  }

  Future<void> _load() async {
    final ui.FragmentShader shader;
    final ui.Image? lut;
    final ui.Image? weight;
    try {
      _program ??=
          await ui.FragmentProgram.fromAsset('shaders/psd_adjustment.frag');
      shader = _program!.fragmentShader();
      lut = widget.lutAsset != null ? await _loadImage(widget.lutAsset!) : null;
      weight = widget.weightAsset != null
          ? await _loadImage(widget.weightAsset!)
          : null;
    } catch (e) {
      // Degrade gracefully: the child renders unadjusted
      debugPrint('PsdAdjustment: failed to load shader: $e');
      return;
    }
    if (!mounted) {
      return;
    }
    setState(() {
      _shader = shader;
      _lut = lut;
      _weight = weight;
    });
  }

  @override
  Widget build(BuildContext context) {
    final shader = _shader;
    if (shader == null) {
      return widget.child;
    }
    return _PsdSampler(
      (ui.Image image, Size size, Canvas canvas) {
        shader.setFloat(0, size.width);
        shader.setFloat(1, size.height);
        var i = 2;
        for (final name in _uniformNames) {
          double value = widget.params[name] ?? _uniformDefaults[name] ?? 0.0;
          if (name == 'useWeightMask') {
            value = _weight != null ? 1.0 : 0.0;
          }
          shader.setFloat(i++, value);
        }
        shader.setImageSampler(0, image);
        shader.setImageSampler(1, _lut ?? _blankImage());
        shader.setImageSampler(2, _weight ?? _blankImage());
        canvas.drawRect(Offset.zero & size, Paint()..shader = shader);
      },
      child: widget.child,
    );
  }
}

typedef _PsdSamplerBuilder = void Function(
    ui.Image image, Size size, Canvas canvas);

// Rasterizes its child into a ui.Image every time it repaints and lets the
// builder paint the processed result instead (same technique as package
// flutter_shaders' AnimatedSampler).
class _PsdSampler extends SingleChildRenderObjectWidget {
  const _PsdSampler(this.builder, {required Widget child})
      : super(child: child);

  final _PsdSamplerBuilder builder;

  @override
  RenderObject createRenderObject(BuildContext context) => _RenderPsdSampler(
      builder, MediaQuery.maybeOf(context)?.devicePixelRatio ?? 1.0);

  @override
  void updateRenderObject(
      BuildContext context, covariant _RenderPsdSampler renderObject) {
    renderObject
      ..builder = builder
      ..devicePixelRatio =
          MediaQuery.maybeOf(context)?.devicePixelRatio ?? 1.0;
  }
}

class _RenderPsdSampler extends RenderProxyBox {
  _RenderPsdSampler(this._builder, this._devicePixelRatio);

  _PsdSamplerBuilder _builder;
  set builder(_PsdSamplerBuilder value) {
    if (identical(value, _builder)) {
      return;
    }
    _builder = value;
    markNeedsCompositedLayerUpdate();
  }

  double _devicePixelRatio;
  set devicePixelRatio(double value) {
    if (value == _devicePixelRatio) {
      return;
    }
    _devicePixelRatio = value;
    markNeedsCompositedLayerUpdate();
  }

  @override
  bool get alwaysNeedsCompositing => true;

  @override
  bool get isRepaintBoundary => true;

  @override
  OffsetLayer updateCompositedLayer(
      {required covariant _PsdSamplerLayer? oldLayer}) {
    final layer = oldLayer ?? _PsdSamplerLayer(_builder);
    layer
      ..callback = _builder
      ..size = size
      ..devicePixelRatio = _devicePixelRatio;
    return layer;
  }
}

class _PsdSamplerLayer extends OffsetLayer {
  _PsdSamplerLayer(this._callback);

  Size size = Size.zero;
  double devicePixelRatio = 1.0;

  _PsdSamplerBuilder _callback;
  _PsdSamplerBuilder get callback => _callback;
  set callback(_PsdSamplerBuilder value) {
    if (identical(value, _callback)) {
      return;
    }
    _callback = value;
    markNeedsAddToScene();
  }

  ui.Image _buildChildScene(Rect rect, double pixelRatio) {
    final builder = ui.SceneBuilder();
    final transform = Matrix4.diagonal3Values(pixelRatio, pixelRatio, 1);
    builder.pushTransform(transform.storage);
    addChildrenToScene(builder);
    builder.pop();
    return builder.build().toImageSync(
          (pixelRatio * rect.width).ceil(),
          (pixelRatio * rect.height).ceil(),
        );
  }

  @override
  void addToScene(ui.SceneBuilder builder) {
    if (size.isEmpty) {
      return;
    }
    final image = _buildChildScene(offset & size, devicePixelRatio);
    final pictureRecorder = ui.PictureRecorder();
    final canvas = Canvas(pictureRecorder);
    try {
      callback(image, size, canvas);
    } finally {
      image.dispose();
    }
    final picture = pictureRecorder.endRecording();
    builder.addPicture(offset, picture);
  }
}
