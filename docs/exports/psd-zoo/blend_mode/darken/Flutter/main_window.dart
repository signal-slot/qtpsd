import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 200,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: PsdBlend(
              blendMode: BlendMode.darken,
              child: Image.asset(
                "assets/images/darken_layer.png", 
                fit: BoxFit.contain,
                height: 200,
                width: 200,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// Composites its child against the backdrop with a Photoshop blend mode
class PsdBlend extends SingleChildRenderObjectWidget {
  const PsdBlend({super.key, required this.blendMode, super.child});

  final BlendMode blendMode;

  @override
  RenderObject createRenderObject(BuildContext context) =>
      _PsdBlendRender(blendMode);

  @override
  void updateRenderObject(
      BuildContext context, covariant _PsdBlendRender renderObject) {
    renderObject.blendMode = blendMode;
  }
}

class _PsdBlendRender extends RenderProxyBox {
  _PsdBlendRender(this._blendMode);

  BlendMode _blendMode;
  set blendMode(BlendMode value) {
    if (value == _blendMode) return;
    _blendMode = value;
    markNeedsPaint();
  }

  @override
  void paint(PaintingContext context, Offset offset) {
    if (child == null) return;
    context.canvas
        .saveLayer(offset & size, Paint()..blendMode = _blendMode);
    context.paintChild(child!, offset);
    context.canvas.restore();
  }
}
