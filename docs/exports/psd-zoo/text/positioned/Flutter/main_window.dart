import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 500,
      width: 500,
      child: Stack(
        children: [
          Positioned(
            height: 500,
            left: 0,
            top: 0,
            width: 500,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 500,
              width: 500,
            ),
          ),
          Positioned(
            height: 32,
            left: 300,
            top: 375,
            width: 66,
            child: Text(
              "Offset", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 24,
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
