import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 1920,
      width: 1080,
      child: Stack(
        children: [
          Positioned(
            height: 1920,
            left: 0,
            top: 0,
            width: 1080,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 1920,
              width: 1080,
            ),
          ),
          Positioned(
            height: 63,
            left: 400,
            top: 910,
            width: 159,
            child: Text(
              "Portrait", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 48,
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
