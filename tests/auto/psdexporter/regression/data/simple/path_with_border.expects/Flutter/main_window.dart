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
                  height: 114,
                  left: 54,
                  top: 65,
                  width: 153,
                  child: Opacity(
                    opacity: 0.368627,
                    child: Container(
                      decoration: ShapeDecoration(
                        color: Color.fromARGB(255, 90, 111, 14),
                        shape: QtPsdPathBorder(
                          color: Color.fromARGB(255, 229, 252, 20),
                          path: Path()
                          ..fillType = PathFillType.evenOdd
                          ..moveTo(76.7117, 11.0005)
                          ..cubicTo(76.7117, 11.0005, 129.154, 29.6605, 129.154, 29.6605)
                          ..cubicTo(129.154, 29.6605, 141.937, 71.1084, 141.937, 71.1084)
                          ..cubicTo(141.937, 71.1084, 105.436, 104.133, 105.436, 104.133)
                          ..cubicTo(105.436, 104.133, 47.1361, 103.866, 47.1361, 103.866)
                          ..cubicTo(47.1361, 103.866, 10.9386, 70.5087, 10.9386, 70.5087)
                          ..cubicTo(10.9386, 70.5087, 24.101, 29.1796, 24.101, 29.1796)
                          ..cubicTo(24.101, 29.1796, 76.7117, 11.0005, 76.7117, 11.0005)
                          ..close(),
                          width: 17,
                        ),
                      ),
                    ),
                  ),
                ),
                Positioned(
                  height: 95,
                  left: 170,
                  top: 75,
                  width: 133,
                  child: Opacity(
                    opacity: 0.368627,
                    child: Container(
                      decoration: ShapeDecoration(
                        color: Color.fromARGB(255, 90, 111, 14),
                        shape: QtPsdPathBorder(
                          path: Path()
                          ..fillType = PathFillType.evenOdd
                          ..moveTo(66.5, 0.999999)
                          ..cubicTo(66.5, 0.999999, 119.027, 19.4198, 119.027, 19.4198)
                          ..cubicTo(119.027, 19.4198, 132, 60.8087, 132, 60.8087)
                          ..cubicTo(132, 60.8087, 95.6502, 94, 95.6502, 94)
                          ..cubicTo(95.6502, 94, 37.3498, 94, 37.3498, 94)
                          ..cubicTo(37.3498, 94, 1, 60.8087, 1, 60.8087)
                          ..cubicTo(1, 60.8087, 13.9731, 19.4198, 13.9731, 19.4198)
                          ..cubicTo(13.9731, 19.4198, 66.5, 0.999999, 66.5, 0.999999)
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
