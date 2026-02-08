import './qtpsd_path.dart';
import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 240,
      width: 320,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 143, 197, 243),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Positioned(
                  height: 96,
                  left: 64,
                  top: 74,
                  width: 134,
                  child: Opacity(
                    opacity: 0.368627,
                    child: Container(
                      decoration: ShapeDecoration(
                        color: Color.fromARGB(255, 90, 111, 14),
                        shape: QtPsdPathBorder(
                          path: Path()
                          ..fillType = PathFillType.nonZero
                          ..moveTo(66.5, 2)
                          ..cubicTo(66.5, 2, 119.027, 20.4198, 119.027, 20.4198)
                          ..cubicTo(119.027, 20.4198, 132, 61.8087, 132, 61.8087)
                          ..cubicTo(132, 61.8087, 95.6502, 95, 95.6502, 95)
                          ..cubicTo(95.6502, 95, 37.3498, 95, 37.3498, 95)
                          ..cubicTo(37.3498, 95, 1, 61.8087, 1, 61.8087)
                          ..cubicTo(1, 61.8087, 13.9731, 20.4198, 13.9731, 20.4198)
                          ..cubicTo(13.9731, 20.4198, 66.5, 2, 66.5, 2)
                          ..close(),
                        ),
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
