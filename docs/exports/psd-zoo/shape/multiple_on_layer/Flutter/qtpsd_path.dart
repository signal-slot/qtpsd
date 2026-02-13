import 'package:flutter/material.dart';

class QtPsdPathBorder extends ShapeBorder {
  const QtPsdPathBorder({required this.path, this.color = null, this.width = 1.0});

  final Path path;
  final Color? color;
  final double width;
  
  @override
  EdgeInsetsGeometry get dimensions => EdgeInsets.all(width);
  
  @override
  Path getInnerPath(Rect rect, {TextDirection? textDirection}) {
    return getOuterPath(rect, textDirection: textDirection);
  }
  
  @override
  Path getOuterPath(Rect rect, {TextDirection? textDirection}) {
    return path;
  }
  
  @override
  bool get preferPaintInterior => true;

  @override 
  void paintInterior(Canvas canvas, Rect rect, Paint paint, {TextDirection? textDirection}) {
    canvas.drawPath(path.shift(Offset(rect.left, rect.top)), paint);
  }

  @override
  void paint(Canvas canvas, Rect rect, {TextDirection? textDirection}) {
    if (color != null) {
      final paint = Paint()
        ..color = color!
        ..strokeWidth = width
        ..style = PaintingStyle.stroke;
      canvas.drawPath(path.shift(Offset(rect.left, rect.top)), paint);
    }
  }
  
  @override
  ShapeBorder scale(double t) {
    //TODO
    return this;
  }
}
