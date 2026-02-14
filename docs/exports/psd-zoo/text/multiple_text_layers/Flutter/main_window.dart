import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 300,
            ),
          ),
          Positioned(
            height: 32,
            left: 100,
            top: 25,
            width: 40,
            child: Text(
              "Top", 
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
          Positioned(
            height: 32,
            left: 100,
            top: 125,
            width: 72,
            child: Text(
              "Middle", 
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
          Positioned(
            height: 32,
            left: 100,
            top: 225,
            width: 79,
            child: Text(
              "Bottom", 
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
