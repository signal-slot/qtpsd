import './qtpsd_path.dart';
import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 300,
            ),
          ),
          Positioned(
            height: 142,
            left: 19,
            top: 29,
            width: 262,
            child: Container(
              decoration: ShapeDecoration(
                color: Color.fromARGB(255, 255, 128, 0),
                shape: QtPsdPathBorder(
                  path: Path()
                  ..fillType = PathFillType.evenOdd
                  ..moveTo(0.999999, 0.999995)
                  ..cubicTo(0.999999, 0.999995, 121, 0.999995, 121, 0.999995)
                  ..cubicTo(121, 0.999995, 121, 141, 121, 141)
                  ..cubicTo(121, 141, 0.999999, 141, 0.999999, 141)
                  ..cubicTo(0.999999, 141, 0.999999, 0.999995, 0.999999, 0.999995)
                  ..moveTo(141, 21)
                  ..cubicTo(141, 21, 261, 21, 261, 21)
                  ..cubicTo(261, 21, 261, 121, 261, 121)
                  ..cubicTo(261, 121, 141, 121, 141, 121)
                  ..cubicTo(141, 121, 141, 21, 141, 21)
                  ..close(),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
