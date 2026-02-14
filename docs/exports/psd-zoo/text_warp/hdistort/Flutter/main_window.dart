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
            height: 37,
            left: 50,
            top: 91,
            width: 114,
            child: Text(
              "DISTORT", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 28,
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
