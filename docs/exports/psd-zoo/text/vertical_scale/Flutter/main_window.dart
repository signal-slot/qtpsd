import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 250,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 250,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 250,
              width: 300,
            ),
          ),
          Positioned(
            height: 48,
            left: 100,
            top: 82,
            width: 57,
            child: Text(
              "Tall", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 36,
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
