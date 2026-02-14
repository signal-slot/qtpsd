import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 400,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 400,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 400,
            ),
          ),
          Positioned(
            height: 252,
            left: 25,
            top: 23,
            width: 354,
            child: Text(
              "Paragraph text in a bounding box. This text wraps automatically when it reaches the edge of the box.", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 16,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
