import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 1080,
      width: 1920,
      child: Stack(
        children: [
          Positioned(
            height: 1080,
            left: 0,
            top: 0,
            width: 1920,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 1080,
              width: 1920,
            ),
          ),
          Text(
            "", 
            textAlign: TextAlign.left,
            textScaler: TextScaler.linear(1),
            style: TextStyle(
              color: Color.fromARGB(255, 0, 0, 0),
              fontFamily: "Sans Serif",
              fontSize: 9,
              fontVariations: [FontVariation.weight(600)],
              height: 1,
            ),
          ),
        ],
      ),
    );
  }
}
